import { NextRequest, NextResponse } from "next/server";
import Anthropic from "@anthropic-ai/sdk";
import { sql, ensureSchema } from "@/lib/db";
import { isAuthorizedHost } from "@/lib/hostAuth";
import property from "@/config/property";

const SYSTEM_PROMPT =
  `You are Nova, the AI Operations Manager for ${property.name}, a short-term rental property. ` +
  `You're given a JSON snapshot of current reservations, property state, and recent guest requests. ` +
  `Speak directly to the host (${property.hostName}) like an excellent human concierge would: warm, sharp, ` +
  `concise, and a little personable — never robotic. ` +
  `Never use markdown syntax (no **, no #, no - for bullets, no backticks). Plain text only. ` +
  `You may use simple bullet characters like • or ✓ and line breaks to organize information, but no headers or asterisks. ` +
  `Keep it tight — a few short lines, not paragraphs. End a proactive briefing with a single relevant offer to help ` +
  `("Would you like me to draft a welcome message for them?") when appropriate.`;

async function getSnapshot() {
  await ensureSchema();
  const [bookingsRes, propertyRes, requestsRes] = await Promise.all([
    sql`SELECT guest_name, check_in, check_out, nights, booked_guests, confirmation_code FROM bookings ORDER BY check_in;`,
    sql`SELECT pool_lights_on, door_code, host_notice FROM property_state WHERE id = 1;`,
    sql`SELECT guest_name, message, is_read, created_at FROM guest_requests ORDER BY created_at DESC LIMIT 20;`,
  ]);

  return {
    today: new Date().toISOString().slice(0, 10),
    bookings: bookingsRes.rows,
    property: propertyRes.rows[0] || null,
    recentRequests: requestsRes.rows,
  };
}

export async function POST(req: NextRequest) {
  if (!isAuthorizedHost(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const apiKey = process.env.ANTHROPIC_API_KEY;
  if (!apiKey) {
    return NextResponse.json({ error: "ANTHROPIC_API_KEY is not configured." }, { status: 500 });
  }

  const body = await req.json().catch(() => ({}));
  const question: string | undefined = typeof body?.question === "string" ? body.question.trim() : undefined;

  try {
    const snapshot = await getSnapshot();
    const client = new Anthropic({ apiKey });

    const userMessage = question
      ? `Property snapshot:\n\n${JSON.stringify(snapshot, null, 2)}\n\nThe host asks: "${question}"\n\nAnswer directly using the snapshot data.`
      : `Here is the current property snapshot:\n\n${JSON.stringify(
          snapshot,
          null,
          2
        )}\n\nGreet ${property.hostName} appropriately for the time of day, then give a short proactive briefing: ` +
        `anything that needs attention (unread requests, upcoming check-ins/checkouts, unusual gaps or back-to-back ` +
        `turnovers), and at most a couple of concrete suggestions.`;

    const response = await client.messages.create({
      model: "claude-sonnet-4-6",
      max_tokens: 700,
      system: SYSTEM_PROMPT,
      messages: [{ role: "user", content: userMessage }],
    });

    const text = response.content
      .filter((block): block is Anthropic.TextBlock => block.type === "text")
      .map((block) => block.text)
      .join("\n");

    return NextResponse.json({ briefing: text });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Failed to generate briefing." }, { status: 500 });
  }
}
