import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { isAuthorizedHost } from "@/lib/hostAuth";

export async function GET(req: NextRequest) {
  if (!isAuthorizedHost(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  await ensureSchema();
  const { rows } = await sql`
    SELECT confirmation_code, guest_name, check_in, check_out, nights, booked_guests
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
    })),
  });
}

export async function POST(req: NextRequest) {
  if (!isAuthorizedHost(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const body = await req.json();
  const { guestName, confirmationCode, checkIn, checkOut, nights, bookedGuests } = body;

  if (!guestName || !confirmationCode || !checkIn || !checkOut) {
    return NextResponse.json({ error: "Missing required fields" }, { status: 400 });
  }

  await ensureSchema();
  try {
    await sql`
      INSERT INTO bookings (confirmation_code, guest_name, check_in, check_out, nights, booked_guests)
      VALUES (${confirmationCode.toUpperCase()}, ${guestName}, ${checkIn}, ${checkOut}, ${Number(nights) || 1}, ${Number(bookedGuests) || 1});
    `;
  } catch (err: any) {
    if (err?.code === "23505") {
      return NextResponse.json({ error: "A reservation with that confirmation code already exists." }, { status: 409 });
    }
    throw err;
  }

  return NextResponse.json({ ok: true }, { status: 201 });
}
