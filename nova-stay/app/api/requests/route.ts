import { NextRequest, NextResponse } from "next/server";
import Anthropic from "@anthropic-ai/sdk";
import { sql, ensureSchema } from "@/lib/db";
import { getAuthorizedProperty, isSuperAdmin, getPropertyIdByConfirmationCode } from "@/lib/hostAuth";

export async function GET(req: NextRequest) {
  const auth = await getAuthorizedProperty(req);
  if (!auth && !isSuperAdmin(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  try {
    await ensureSchema();
    const { rows } = auth
      ? await sql`
          SELECT id, guest_name, message, is_read, created_at, confirmation_code, priority, category, suggested_response
          FROM guest_requests
          WHERE property_id = ${auth.propertyId}
          ORDER BY created_at DESC;
        `
      : await sql`
          SELECT id, guest_name, message, is_read, created_at, confirmation_code, priority, category, suggested_response
          FROM guest_requests
          ORDER BY created_at DESC;
        `;
    return NextResponse.json({
      requests: rows.map((row) => ({
        id: row.id,
        guestName: row.guest_name,
        message: row.message,
        isRead: row.is_read,
        createdAt: row.created_at,
        confirmationCode: row.confirmation_code,
        priority: row.priority,
        category: row.category,
        suggestedResponse: row.suggested_response,
      })),
    });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while loading requests." }, { status: 500 });
  }
}

const SENTIMENT_TOOL: Anthropic.Tool = {
  name: "classify_message",
  description: "Classify a guest message for a short-term rental host.",
  input_schema: {
    type: "object",
    properties: {
      category: {
        type: "string",
        enum: ["property_issue", "late_checkout", "upsell_opportunity", "question", "complaint", "general"],
        description: "The best-fitting category for this message.",
      },
      priority: {
        type: "string",
        enum: ["low", "medium", "high", "urgent"],
        description: "Urgent = needs the host's attention right away (e.g. something broken, safety, guest stuck). High = same-day. Medium/low = can wait.",
      },
      suggestedResponse: {
        type: "string",
        description: "A short, warm, ready-to-send reply the host could send as-is or lightly edit.",
      },
    },
    required: ["category", "priority", "suggestedResponse"],
  },
};

async function classifyMessage(message: string): Promise<{ category: string; priority: string; suggestedResponse: string } | null> {
  const apiKey = process.env.ANTHROPIC_API_KEY;
  if (!apiKey) return null;
  try {
    const client = new Anthropic({ apiKey });
    const response = await client.messages.create({
      model: "claude-sonnet-4-6",
      max_tokens: 400,
      system:
        "You classify incoming guest messages for a short-term rental host so they can triage their inbox at a glance. " +
        "Be decisive — pick the single best category and priority. Property issues (broken/not working/leaking/etc) " +
        "are always at least high priority, and urgent if it affects safety or basic livability (no AC, no hot water, locked out, etc).",
      tools: [SENTIMENT_TOOL],
      tool_choice: { type: "tool", name: "classify_message" },
      messages: [{ role: "user", content: `Guest message: "${message}"` }],
    });
    const toolUse = response.content.find(
      (block): block is Anthropic.ToolUseBlock => block.type === "tool_use" && block.name === "classify_message"
    );
    if (!toolUse) return null;
    return toolUse.input as { category: string; priority: string; suggestedResponse: string };
  } catch {
    return null;
  }
}

export async function POST(req: NextRequest) {
  const body = await req.json();
  const { guestName, message, confirmationCode } = body;

  if (!guestName?.trim() || !message?.trim()) {
    return NextResponse.json({ error: "Name and message are required." }, { status: 400 });
  }

  try {
    await ensureSchema();
    const classification = await classifyMessage(message.trim());
    const code = confirmationCode ? confirmationCode.trim().toUpperCase() : null;
    const propertyId = code ? await getPropertyIdByConfirmationCode(code) : null;
    await sql`
      INSERT INTO guest_requests (guest_name, message, confirmation_code, priority, category, suggested_response, property_id)
      VALUES (
        ${guestName.trim()},
        ${message.trim()},
        ${code},
        ${classification?.priority || null},
        ${classification?.category || null},
        ${classification?.suggestedResponse || null},
        ${propertyId}
      );
    `;
    return NextResponse.json({ ok: true }, { status: 201 });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while saving request." }, { status: 500 });
  }
}

export async function PATCH(req: NextRequest) {
  const auth = await getAuthorizedProperty(req);
  if (!auth && !isSuperAdmin(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const body = await req.json();
  const { id } = body;
  if (!id) return NextResponse.json({ error: "Missing id" }, { status: 400 });

  try {
    await ensureSchema();
    if (auth) {
      await sql`UPDATE guest_requests SET is_read = true WHERE id = ${id} AND property_id = ${auth.propertyId};`;
    } else {
      await sql`UPDATE guest_requests SET is_read = true WHERE id = ${id};`;
    }
    return NextResponse.json({ ok: true });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while updating request." }, { status: 500 });
  }
}
