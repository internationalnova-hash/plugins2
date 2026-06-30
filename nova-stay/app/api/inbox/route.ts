import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { isAuthorizedHost } from "@/lib/hostAuth";

export async function GET(req: NextRequest) {
  const code = req.nextUrl.searchParams.get("code")?.trim().toUpperCase();
  if (!code) {
    return NextResponse.json({ error: "Missing code" }, { status: 400 });
  }

  try {
    await ensureSchema();
    const { rows } = await sql`
      SELECT id, message, created_at FROM guest_inbox
      WHERE confirmation_code = ${code}
      ORDER BY created_at DESC;
    `;
    return NextResponse.json({
      messages: rows.map((row) => ({ id: row.id, text: row.message, createdAt: row.created_at })),
    });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while loading inbox." }, { status: 500 });
  }
}

export async function POST(req: NextRequest) {
  if (!isAuthorizedHost(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const body = await req.json().catch(() => ({}));
  const { confirmationCode, message } = body;
  if (!confirmationCode || typeof message !== "string" || !message.trim()) {
    return NextResponse.json({ error: "Missing confirmationCode or message" }, { status: 400 });
  }

  try {
    await ensureSchema();
    await sql`
      INSERT INTO guest_inbox (confirmation_code, message)
      VALUES (${confirmationCode.trim().toUpperCase()}, ${message.trim()});
    `;
    return NextResponse.json({ ok: true }, { status: 201 });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while sending message." }, { status: 500 });
  }
}
