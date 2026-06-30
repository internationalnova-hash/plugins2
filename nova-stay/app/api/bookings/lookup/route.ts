import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";

export async function GET(req: NextRequest) {
  const code = req.nextUrl.searchParams.get("code")?.trim().toUpperCase();
  if (!code) {
    return NextResponse.json({ error: "Missing code" }, { status: 400 });
  }

  await ensureSchema();
  const { rows } = await sql`
    SELECT confirmation_code, guest_name, check_in, check_out, nights, booked_guests
    FROM bookings
    WHERE UPPER(confirmation_code) = ${code}
    LIMIT 1;
  `;

  if (rows.length === 0) {
    return NextResponse.json({ booking: null }, { status: 404 });
  }

  const row = rows[0];
  return NextResponse.json({
    booking: {
      confirmationCode: row.confirmation_code,
      guestName: row.guest_name,
      checkIn: row.check_in,
      checkOut: row.check_out,
      nights: row.nights,
      bookedGuests: row.booked_guests,
    },
  });
}
