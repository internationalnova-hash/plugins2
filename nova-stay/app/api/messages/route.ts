import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { isAuthorizedHost } from "@/lib/hostAuth";

export async function GET(req: NextRequest) {
  const code = req.nextUrl.searchParams.get("code");
  if (!code) return NextResponse.json({ error: "Missing code" }, { status: 400 });

  try {
    await ensureSchema();
    const { rows } = await sql`
      SELECT message, created_at FROM guest_messages WHERE confirmation_code = ${code.trim().toUpperCase()};
    `;
    const row = rows[0];
    return NextResponse.json({ message: row ? { text: row.message, createdAt: row.created_at } : null });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while loading message." }, { status: 500 });
  }
}

export async function POST(req: NextRequest) {
  if (!isAuthorizedHost(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const body = await req.json();
  const { confirmationCode, message } = body;
  if (!confirmationCode || typeof message !== "string") {
    return NextResponse.json({ error: "Missing confirmationCode or message" }, { status: 400 });
  }

  try {
    await ensureSchema();
    await sql`
      INSERT INTO guest_messages (confirmation_code, message)
      VALUES (${confirmationCode.trim().toUpperCase()}, ${message.trim()})
      ON CONFLICT (confirmation_code) DO UPDATE SET message = ${message.trim()}, created_at = now();
    `;
    return NextResponse.json({ ok: true });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while saving message." }, { status: 500 });
  }
}
