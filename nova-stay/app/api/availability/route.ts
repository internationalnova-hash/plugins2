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

    // Fetch all bookings regardless of date format — filter in JS
    const { rows: bookings } = await sql`
      SELECT check_in, check_out
      FROM bookings
      WHERE (property_id = ${propertyId} OR property_id IS NULL)
        AND payment_status IN ('paid', 'unpaid', 'pending')
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

    const todayStr = new Date().toISOString().slice(0, 10);

    // Normalize any date string (ISO, "July 10", "July 10 2026", "07/10/2026") to YYYY-MM-DD
    const normalizeDate = (raw: string): string | null => {
      if (!raw) return null;
      const trimmed = raw.trim();
      // Already ISO format
      if (/^\d{4}-\d{2}-\d{2}$/.test(trimmed)) return trimmed;
      // Try parsing natural language — default year to current if missing
      const year = new Date().getFullYear();
      const withYear = /\d{4}/.test(trimmed) ? trimmed : `${trimmed} ${year}`;
      const d = new Date(withYear);
      if (isNaN(d.getTime())) return null;
      return d.toISOString().slice(0, 10);
    }

    const bookedRanges = bookings
      .map((b) => ({ checkIn: normalizeDate(b.check_in), checkOut: normalizeDate(b.check_out) }))
      .filter((r) => r.checkIn && r.checkOut && r.checkOut! >= todayStr) as { checkIn: string; checkOut: string }[];

    return NextResponse.json({
      bookedRanges,
      prices,
      defaultPrice: Number(pricing.default_price_per_night),
      minNights: Number(pricing.min_nights),
    });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message }, { status: 500 });
  }
}
