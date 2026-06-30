import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { getAuthorizedProperty, isSuperAdmin, getPropertyIdByConfirmationCode } from "@/lib/hostAuth";

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
  const auth = await getAuthorizedProperty(req);
  if (!auth && !isSuperAdmin(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const body = await req.json().catch(() => ({}));
  const { confirmationCode, message } = body;
  if (!confirmationCode || typeof message !== "string" || !message.trim()) {
    return NextResponse.json({ error: "Missing confirmationCode or message" }, { status: 400 });
  }

  try {
    await ensureSchema();
    const code = confirmationCode.trim().toUpperCase();
    const propertyId = auth ? auth.propertyId : await getPropertyIdByConfirmationCode(code);
    await sql`
      INSERT INTO guest_inbox (confirmation_code, message, property_id)
      VALUES (${code}, ${message.trim()}, ${propertyId});
    `;
    return NextResponse.json({ ok: true }, { status: 201 });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while sending message." }, { status: 500 });
  }
}
