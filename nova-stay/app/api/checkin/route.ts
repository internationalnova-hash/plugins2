import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";

export async function POST(req: NextRequest) {
  const body = await req.json().catch(() => ({}));
  const { confirmationCode } = body;

  if (!confirmationCode) {
    return NextResponse.json({ error: "Missing confirmationCode" }, { status: 400 });
  }

  try {
    await ensureSchema();
    await sql`
      UPDATE bookings
      SET checked_in_at = NOW()
      WHERE confirmation_code = ${confirmationCode.trim().toUpperCase()}
        AND checked_in_at IS NULL;
    `;
    return NextResponse.json({ ok: true });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error" }, { status: 500 });
  }
}
