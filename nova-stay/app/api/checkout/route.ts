import { NextRequest, NextResponse } from "next/server";
import Stripe from "stripe";
import { sql, ensureSchema } from "@/lib/db";
import { getPropertyIdBySlug } from "@/lib/hostAuth";

function getStripe() {
  const key = process.env.STRIPE_SECRET_KEY;
  if (!key) throw new Error("STRIPE_SECRET_KEY is not set");
  return new Stripe(key, { apiVersion: "2026-06-24.dahlia" });
}

// Given check-in, check-out, and slug, calculate total using date_prices + default.
async function calculateTotal(
  propertyId: number,
  checkIn: string,
  checkOut: string
): Promise<{ total: number; nights: number; breakdown: { date: string; price: number }[] }> {
  const { rows: customPrices } = await sql`
    SELECT date::TEXT AS date, price_per_night
    FROM date_prices
    WHERE property_id = ${propertyId}
      AND date >= ${checkIn}::DATE
      AND date < ${checkOut}::DATE;
  `;

  const { rows: pricingRows } = await sql`
    SELECT default_price_per_night FROM property_pricing WHERE property_id = ${propertyId} LIMIT 1;
  `;
  const defaultPrice = Number(pricingRows[0]?.default_price_per_night ?? 250);

  const customMap: Record<string, number> = {};
  for (const row of customPrices) {
    customMap[row.date] = Number(row.price_per_night);
  }

  const breakdown: { date: string; price: number }[] = [];
  const start = new Date(checkIn + "T12:00:00Z");
  const end = new Date(checkOut + "T12:00:00Z");
  const cur = new Date(start);

  while (cur < end) {
    const dateStr = cur.toISOString().slice(0, 10);
    breakdown.push({ date: dateStr, price: customMap[dateStr] ?? defaultPrice });
    cur.setUTCDate(cur.getUTCDate() + 1);
  }

  const total = breakdown.reduce((sum, d) => sum + d.price, 0);
  return { total, nights: breakdown.length, breakdown };
}

export async function POST(req: NextRequest) {
  const body = await req.json();
  const { checkIn, checkOut, guestName, guestEmail, slug = "casanova" } = body;

  if (!checkIn || !checkOut || !guestName || !guestEmail) {
    return NextResponse.json({ error: "checkIn, checkOut, guestName, and guestEmail are required" }, { status: 400 });
  }

  try {
    await ensureSchema();
    const stripe = getStripe();

    const propertyId = await getPropertyIdBySlug(slug);
    if (!propertyId) return NextResponse.json({ error: "Property not found" }, { status: 404 });

    // Check availability — reject if any booking overlaps
    const { rows: conflicts } = await sql`
      SELECT confirmation_code FROM bookings
      WHERE property_id = ${propertyId}
        AND payment_status IN ('paid', 'pending')
        AND check_in < ${checkOut}
        AND check_out > ${checkIn}
      LIMIT 1;
    `;
    if (conflicts.length > 0) {
      return NextResponse.json({ error: "Those dates are no longer available." }, { status: 409 });
    }

    const { total, nights, breakdown } = await calculateTotal(propertyId, checkIn, checkOut);

    const origin = req.headers.get("origin") || process.env.NEXT_PUBLIC_APP_URL || "https://nova-stay.vercel.app";

    const session = await stripe.checkout.sessions.create({
      mode: "payment",
      payment_method_types: ["card"],
      customer_email: guestEmail,
      line_items: [
        {
          price_data: {
            currency: "usd",
            product_data: {
              name: `Direct Booking — ${nights} night${nights !== 1 ? "s" : ""}`,
              description: `Check-in: ${checkIn}  ·  Check-out: ${checkOut}`,
            },
            unit_amount: Math.round(total * 100),
          },
          quantity: 1,
        },
      ],
      metadata: {
        propertyId: String(propertyId),
        slug,
        checkIn,
        checkOut,
        nights: String(nights),
        guestName,
        guestEmail,
        totalPrice: String(total),
      },
      success_url: `${origin}/${slug}?booking=success`,
      cancel_url: `${origin}/${slug}?booking=cancelled`,
    });

    // Create a pending booking immediately so dates are held
    const confirmationCode = `DIRECT-${Date.now()}`;
    await sql`
      INSERT INTO bookings (confirmation_code, guest_name, guest_email, check_in, check_out, nights,
        booked_guests, booking_source, property_id, payment_status, stripe_session_id, total_price)
      VALUES (${confirmationCode}, ${guestName}, ${guestEmail}, ${checkIn}, ${checkOut}, ${nights},
        1, 'direct', ${propertyId}, 'pending', ${session.id}, ${total});
    `;

    return NextResponse.json({ url: session.url, sessionId: session.id });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Checkout error" }, { status: 500 });
  }
}
