import { NextRequest, NextResponse } from "next/server";
import Anthropic from "@anthropic-ai/sdk";
import { sql, ensureSchema } from "@/lib/db";
import { propertyConfig } from "@/data/config";
import property from "@/config/property";

const HISTORY_LIMIT = 20;

export async function GET(req: NextRequest) {
  const code = req.nextUrl.searchParams.get("code")?.trim().toUpperCase();
  if (!code) {
    return NextResponse.json({ error: "Missing code" }, { status: 400 });
  }

  try {
    await ensureSchema();
    const { rows } = await sql`
      SELECT id, role, content, created_at FROM guest_conversations
      WHERE confirmation_code = ${code}
      ORDER BY created_at ASC;
    `;
    return NextResponse.json({
      messages: rows.map((row) => ({ id: row.id, role: row.role, text: row.content, createdAt: row.created_at })),
    });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while loading conversation." }, { status: 500 });
  }
}

async function getGroundingContext(code: string) {
  const [bookingRes, guideRes] = await Promise.all([
    sql`SELECT guest_name, check_in, check_out, nights, booked_guests FROM bookings WHERE UPPER(confirmation_code) = ${code} LIMIT 1;`,
    sql`SELECT content FROM stay_guide WHERE id = 1;`,
  ]);

  return {
    booking: bookingRes.rows[0] || null,
    stayGuide: guideRes.rows[0]?.content || null,
    amenities: {
      theater: propertyConfig.theater,
      studio: propertyConfig.studio,
      pool: propertyConfig.pool,
      gameRoom: propertyConfig.gameRoom,
      grill: propertyConfig.grill,
      wifi: propertyConfig.wifi,
      doorCode: propertyConfig.doorCode,
      checkout: propertyConfig.checkout,
      emergency: propertyConfig.emergency,
      faq: propertyConfig.faq,
    },
    recommendations: {
      dining: propertyConfig.dining,
      explore: propertyConfig.explore,
      recommendations: propertyConfig.recommendations,
    },
  };
}

export async function POST(req: NextRequest) {
  const apiKey = process.env.ANTHROPIC_API_KEY;
  if (!apiKey) {
    return NextResponse.json({ error: "ANTHROPIC_API_KEY is not configured." }, { status: 500 });
  }

  const body = await req.json().catch(() => ({}));
  const { confirmationCode, message } = body;
  if (!confirmationCode || typeof message !== "string" || !message.trim()) {
    return NextResponse.json({ error: "Missing confirmationCode or message" }, { status: 400 });
  }

  const code = confirmationCode.trim().toUpperCase();

  try {
    await ensureSchema();

    const [context, historyRes] = await Promise.all([
      getGroundingContext(code),
      sql`
        SELECT role, content FROM guest_conversations
        WHERE confirmation_code = ${code}
        ORDER BY created_at DESC LIMIT ${HISTORY_LIMIT};
      `,
    ]);
    const history = historyRes.rows.reverse();

    const client = new Anthropic({ apiKey });
    const response = await client.messages.create({
      model: "claude-sonnet-4-6",
      max_tokens: 500,
      system:
        `You are Nova, the AI concierge for ${property.name}, speaking directly to a guest currently staying there. ` +
        `You remember this guest's whole conversation across their stay — refer back to things they or you mentioned earlier when relevant ` +
        `(e.g. if you recommended a restaurant yesterday and they ask about it today, you know which one). ` +
        `Answer using the property context JSON below — never invent door codes, instructions, or facts not present in it. ` +
        `If something isn't covered by the context, say so warmly and suggest they message the host directly. ` +
        `Be warm, concise, and a little personable — a few sentences, not paragraphs. No markdown syntax (no **, #, backticks).\n\n` +
        `Property context:\n${JSON.stringify(context, null, 2)}`,
      messages: [
        ...history.map((h) => ({ role: h.role as "user" | "assistant", content: h.content })),
        { role: "user" as const, content: message.trim() },
      ],
    });

    const reply = response.content
      .filter((block): block is Anthropic.TextBlock => block.type === "text")
      .map((block) => block.text)
      .join("\n");

    await sql`
      INSERT INTO guest_conversations (confirmation_code, role, content) VALUES (${code}, 'user', ${message.trim()});
    `;
    await sql`
      INSERT INTO guest_conversations (confirmation_code, role, content) VALUES (${code}, 'assistant', ${reply});
    `;

    return NextResponse.json({ reply });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Failed to get a response from Nova." }, { status: 500 });
  }
}
