import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { isAuthorizedHost } from "@/lib/hostAuth";

export async function GET(req: NextRequest) {
  if (!isAuthorizedHost(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  try {
    await ensureSchema();
    const { rows } = await sql`
      SELECT id, guest_name, message, is_read, created_at
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
      })),
    });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while loading requests." }, { status: 500 });
  }
}

export async function POST(req: NextRequest) {
  const body = await req.json();
  const { guestName, message } = body;

  if (!guestName?.trim() || !message?.trim()) {
    return NextResponse.json({ error: "Name and message are required." }, { status: 400 });
  }

  try {
    await ensureSchema();
    await sql`
      INSERT INTO guest_requests (guest_name, message) VALUES (${guestName.trim()}, ${message.trim()});
    `;
    return NextResponse.json({ ok: true }, { status: 201 });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while saving request." }, { status: 500 });
  }
}

export async function PATCH(req: NextRequest) {
  if (!isAuthorizedHost(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const body = await req.json();
  const { id } = body;
  if (!id) return NextResponse.json({ error: "Missing id" }, { status: 400 });

  try {
    await ensureSchema();
    await sql`UPDATE guest_requests SET is_read = true WHERE id = ${id};`;
    return NextResponse.json({ ok: true });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while updating request." }, { status: 500 });
  }
}
