import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { getPropertyIdBySlug } from "@/lib/hostAuth";

// Returns booked date ranges and per-date pricing for a property.
// Public endpoint — no auth required.
export async function GET(req: NextRequest) {
  const { searchParams } = new URL(req.url);
  const slug = searchParams.get("slug") || "casanova";

  try {
    await ensureSchema();

    const propertyId = await getPropertyIdBySlug(slug);
    if (!propertyId) {
      return NextResponse.json({ bookedRanges: [], prices: {}, defaultPrice: 250 });
    }

    const { rows: bookings } = await sql`
      SELECT check_in, check_out
      FROM bookings
      WHERE property_id = ${propertyId}
        AND payment_status IN ('paid', 'unpaid')
        AND check_out >= CURRENT_DATE::TEXT
      ORDER BY check_in;
    `;

    const { rows: datePrices } = await sql`
      SELECT date::TEXT AS date, price_per_night
      FROM date_prices
      WHERE property_id = ${propertyId}
        AND date >= CURRENT_DATE
      ORDER BY date;
    `;

    const { rows: pricingRows } = await sql`
      SELECT default_price_per_night, min_nights
      FROM property_pricing
      WHERE property_id = ${propertyId}
      LIMIT 1;
    `;

    const pricing = pricingRows[0] || { default_price_per_night: 250, min_nights: 1 };

    const prices: Record<string, number> = {};
    for (const row of datePrices) {
      prices[row.date] = Number(row.price_per_night);
    }

    return NextResponse.json({
      bookedRanges: bookings.map((b) => ({ checkIn: b.check_in, checkOut: b.check_out })),
      prices,
      defaultPrice: Number(pricing.default_price_per_night),
      minNights: Number(pricing.min_nights),
    });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message }, { status: 500 });
  }
}
