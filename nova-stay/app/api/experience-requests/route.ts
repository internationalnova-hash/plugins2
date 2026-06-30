import { NextRequest, NextResponse } from "next/server";
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
          SELECT id, confirmation_code, guest_name, experience_title, price_from, booking_source, status, created_at
          FROM experience_requests
          WHERE property_id = ${auth.propertyId}
          ORDER BY created_at DESC;
        `
      : await sql`
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
    const code = confirmationCode ? confirmationCode.trim().toUpperCase() : null;
    let propertyId: number | null = null;
    if (code) {
      const { rows } = await sql`
        SELECT booking_source, property_id FROM bookings WHERE UPPER(confirmation_code) = ${code} LIMIT 1;
      `;
      if (rows[0]) {
        bookingSource = rows[0].booking_source;
        propertyId = rows[0].property_id;
      }
    }

    await sql`
      INSERT INTO experience_requests (confirmation_code, guest_name, experience_title, price_from, booking_source, status, property_id)
      VALUES (${code}, ${guestName}, ${experienceTitle}, ${priceFrom || null}, ${bookingSource}, 'requested', ${propertyId});
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

  const body = await req.json().catch(() => ({}));
  const { id, status } = body;
  const VALID_STATUSES = ["requested", "approved", "billed", "paid", "scheduled", "completed", "declined"];
  if (!id || !VALID_STATUSES.includes(status)) {
    return NextResponse.json({ error: "Missing or invalid id/status" }, { status: 400 });
  }

  try {
    await ensureSchema();
    if (auth) {
      await sql`UPDATE experience_requests SET status = ${status}, updated_at = now() WHERE id = ${id} AND property_id = ${auth.propertyId};`;
    } else {
      await sql`UPDATE experience_requests SET status = ${status}, updated_at = now() WHERE id = ${id};`;
    }
    return NextResponse.json({ ok: true });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while updating request." }, { status: 500 });
  }
}
