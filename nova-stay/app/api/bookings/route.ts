import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { getAuthorizedProperty, isSuperAdmin } from "@/lib/hostAuth";

export async function GET(req: NextRequest) {
  const auth = await getAuthorizedProperty(req);
  if (!auth && !isSuperAdmin(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  try {
    await ensureSchema();
    const { rows } = auth
      ? await sql`
          SELECT confirmation_code, guest_name, check_in, check_out, nights, booked_guests, booking_source
          FROM bookings
          WHERE property_id = ${auth.propertyId}
          ORDER BY created_at DESC;
        `
      : await sql`
          SELECT confirmation_code, guest_name, check_in, check_out, nights, booked_guests, booking_source
          FROM bookings
          ORDER BY created_at DESC;
        `;

    return NextResponse.json({
      bookings: rows.map((row) => ({
        confirmationCode: row.confirmation_code,
        guestName: row.guest_name,
        checkIn: row.check_in,
        checkOut: row.check_out,
        nights: row.nights,
        bookedGuests: row.booked_guests,
        bookingSource: row.booking_source,
      })),
    });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while loading reservations." }, { status: 500 });
  }
}

export async function POST(req: NextRequest) {
  const auth = await getAuthorizedProperty(req);
  if (!auth && !isSuperAdmin(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const body = await req.json();
  const { guestName, confirmationCode, checkIn, checkOut, nights, bookedGuests, bookingSource } = body;

  if (!guestName || !confirmationCode || !checkIn || !checkOut) {
    return NextResponse.json({ error: "Missing required fields" }, { status: 400 });
  }

  try {
    await ensureSchema();
    await sql`
      INSERT INTO bookings (confirmation_code, guest_name, check_in, check_out, nights, booked_guests, booking_source, property_id)
      VALUES (${confirmationCode.toUpperCase()}, ${guestName}, ${checkIn}, ${checkOut}, ${Number(nights) || 1}, ${Number(bookedGuests) || 1}, ${bookingSource || "airbnb"}, ${auth?.propertyId ?? null});
    `;
  } catch (err: any) {
    if (err?.code === "23505") {
      return NextResponse.json({ error: "A reservation with that confirmation code already exists." }, { status: 409 });
    }
    return NextResponse.json({ error: err?.message || "Database error while saving reservation." }, { status: 500 });
  }

  return NextResponse.json({ ok: true }, { status: 201 });
}
