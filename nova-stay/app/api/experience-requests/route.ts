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
      SELECT id, confirmation_code, guest_name, experience_title, price_from, booking_source, status, created_at
      FROM experience_requests
      ORDER BY created_at DESC;
    `;
    return NextResponse.json({
      requests: rows.map((row) => ({
        id: row.id,
        confirmationCode: row.confirmation_code,
        guestName: row.guest_name,
        experienceTitle: row.experience_title,
        priceFrom: row.price_from,
        bookingSource: row.booking_source,
        status: row.status,
        createdAt: row.created_at,
      })),
    });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while loading requests." }, { status: 500 });
  }
}

export async function POST(req: NextRequest) {
  const body = await req.json().catch(() => ({}));
  const { confirmationCode, guestName, experienceTitle, priceFrom } = body;

  if (!guestName || !experienceTitle) {
    return NextResponse.json({ error: "Missing guestName or experienceTitle" }, { status: 400 });
  }

  try {
    await ensureSchema();
    let bookingSource = "direct";
    if (confirmationCode) {
      const { rows } = await sql`
        SELECT booking_source FROM bookings WHERE UPPER(confirmation_code) = ${confirmationCode.trim().toUpperCase()} LIMIT 1;
      `;
      if (rows[0]) bookingSource = rows[0].booking_source;
    }

    await sql`
      INSERT INTO experience_requests (confirmation_code, guest_name, experience_title, price_from, booking_source, status)
      VALUES (${confirmationCode ? confirmationCode.trim().toUpperCase() : null}, ${guestName}, ${experienceTitle}, ${priceFrom || null}, ${bookingSource}, 'requested');
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

  const body = await req.json().catch(() => ({}));
  const { id, status } = body;
  const VALID_STATUSES = ["requested", "approved", "billed", "paid", "scheduled", "completed", "declined"];
  if (!id || !VALID_STATUSES.includes(status)) {
    return NextResponse.json({ error: "Missing or invalid id/status" }, { status: 400 });
  }

  try {
    await ensureSchema();
    await sql`UPDATE experience_requests SET status = ${status}, updated_at = now() WHERE id = ${id};`;
    return NextResponse.json({ ok: true });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while updating request." }, { status: 500 });
  }
}
