import { NextRequest, NextResponse } from "next/server";
import Stripe from "stripe";
import { sql, ensureSchema } from "@/lib/db";
import { getPropertyIdBySlug } from "@/lib/hostAuth";

function getStripe() {
  const key = process.env.STRIPE_SECRET_KEY;
  if (!key) throw new Error("STRIPE_SECRET_KEY is not set");
  return new Stripe(key, { apiVersion: "2026-06-24.dahlia" });
}

async function calculateNightlyTotal(
  propertyId: number,
  checkIn: string,
  checkOut: string
): Promise<{ nightlyTotal: number; nights: number; cleaningFee: number }> {
  const { rows: customPrices } = await sql`
    SELECT date::TEXT AS date, price_per_night
    FROM date_prices
    WHERE property_id = ${propertyId}
      AND date >= ${checkIn}::DATE
      AND date < ${checkOut}::DATE;
  `;

  const { rows: pricingRows } = await sql`
    SELECT default_price_per_night, cleaning_fee FROM property_pricing WHERE property_id = ${propertyId} LIMIT 1;
  `;
  const defaultPrice = Number(pricingRows[0]?.default_price_per_night ?? 250);
  const cleaningFee = Number(pricingRows[0]?.cleaning_fee ?? 175);

  const customMap: Record<string, number> = {};
  for (const row of customPrices) {
    customMap[row.date] = Number(row.price_per_night);
  }

  const start = new Date(checkIn + "T12:00:00Z");
  const end = new Date(checkOut + "T12:00:00Z");
  const cur = new Date(start);
  let nightlyTotal = 0;
  let nights = 0;

  while (cur < end) {
    const dateStr = cur.toISOString().slice(0, 10);
    nightlyTotal += customMap[dateStr] ?? defaultPrice;
    nights++;
    cur.setUTCDate(cur.getUTCDate() + 1);
  }

  return { nightlyTotal, nights, cleaningFee };
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

    const { rows: conflicts } = await sql`
      SELECT confirmation_code FROM bookings
      WHERE (property_id = ${propertyId} OR property_id IS NULL)
        AND payment_status IN ('paid', 'pending', 'unpaid')
        AND check_in < ${checkOut}
        AND check_out > ${checkIn}
      LIMIT 1;
    `;
    if (conflicts.length > 0) {
      return NextResponse.json({ error: "Those dates are no longer available." }, { status: 409 });
    }

    const { nightlyTotal, nights, cleaningFee } = await calculateNightlyTotal(propertyId, checkIn, checkOut);

    // Stripe processing fee: 2.9% + $0.30 — passed to guest
    const subtotal = nightlyTotal + cleaningFee;
    const processingFee = Math.round((subtotal * 0.029 + 0.30) * 100) / 100;
    const grandTotal = subtotal + processingFee;

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
              name: `${nights} Night${nights !== 1 ? "s" : ""} — Casanova ATL`,
              description: `Check-in: ${checkIn}  ·  Check-out: ${checkOut}`,
            },
            unit_amount: Math.round(nightlyTotal * 100),
          },
          quantity: 1,
        },
        {
          price_data: {
            currency: "usd",
            product_data: { name: "Cleaning Fee" },
            unit_amount: Math.round(cleaningFee * 100),
          },
          quantity: 1,
        },
        {
          price_data: {
            currency: "usd",
            product_data: { name: "Processing Fee" },
            unit_amount: Math.round(processingFee * 100),
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
        totalPrice: String(grandTotal),
      },
      success_url: `${origin}/${slug}?booking=success`,
      cancel_url: `${origin}/${slug}?booking=cancelled`,
    });

    const confirmationCode = `DIRECT-${Date.now()}`;
    await sql`
      INSERT INTO bookings (confirmation_code, guest_name, guest_email, check_in, check_out, nights,
        booked_guests, booking_source, property_id, payment_status, stripe_session_id, total_price)
      VALUES (${confirmationCode}, ${guestName}, ${guestEmail}, ${checkIn}, ${checkOut}, ${nights},
        1, 'direct', ${propertyId}, 'pending', ${session.id}, ${grandTotal});
    `;

    return NextResponse.json({ url: session.url, sessionId: session.id });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Checkout error" }, { status: 500 });
  }
}
