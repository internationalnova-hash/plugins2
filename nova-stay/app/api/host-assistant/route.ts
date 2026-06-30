import { NextRequest, NextResponse } from "next/server";
import Anthropic from "@anthropic-ai/sdk";
import { sql, ensureSchema } from "@/lib/db";
import { isAuthorizedHost } from "@/lib/hostAuth";

export async function POST(req: NextRequest) {
  if (!isAuthorizedHost(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const apiKey = process.env.ANTHROPIC_API_KEY;
  if (!apiKey) {
    return NextResponse.json({ error: "ANTHROPIC_API_KEY is not configured." }, { status: 500 });
  }

  try {
    await ensureSchema();
    const [bookingsRes, propertyRes, requestsRes] = await Promise.all([
      sql`SELECT guest_name, check_in, check_out, nights, booked_guests FROM bookings ORDER BY check_in;`,
      sql`SELECT pool_lights_on, door_code, host_notice FROM property_state WHERE id = 1;`,
      sql`SELECT guest_name, message, is_read, created_at FROM guest_requests ORDER BY created_at DESC LIMIT 20;`,
    ]);

    const snapshot = {
      today: new Date().toISOString().slice(0, 10),
      bookings: bookingsRes.rows,
      property: propertyRes.rows[0] || null,
      recentRequests: requestsRes.rows,
    };

    const client = new Anthropic({ apiKey });
    const response = await client.messages.create({
      model: "claude-sonnet-4-6",
      max_tokens: 700,
      system:
        "You are an AI assistant for the host of a short-term rental property called Casanova ATL. " +
        "You're given a JSON snapshot of current reservations, property state, and recent guest requests. " +
        "Give the host a short, proactive briefing: flag anything that needs attention (unread requests, " +
        "upcoming check-ins/checkouts, unusual gaps or back-to-back turnovers), and 1-3 concrete suggestions. " +
        "Be concise — use short bullet points, no preamble, no markdown headers.",
      messages: [
        {
          role: "user",
          content: `Here is the current property snapshot:\n\n${JSON.stringify(snapshot, null, 2)}`,
        },
      ],
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
