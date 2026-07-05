import { NextRequest, NextResponse } from "next/server";
import Stripe from "stripe";
import { sql, ensureSchema } from "@/lib/db";
import { getAuthorizedProperty, isSuperAdmin } from "@/lib/hostAuth";

function getStripe() {
  const key = process.env.STRIPE_SECRET_KEY;
  if (!key) throw new Error("STRIPE_SECRET_KEY is not set");
  return new Stripe(key, { apiVersion: "2026-06-24.dahlia" });
}

export async function POST(req: NextRequest) {
  const auth = await getAuthorizedProperty(req);
  if (!auth && !isSuperAdmin(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const body = await req.json();
  const { guestName, guestEmail, checkIn, checkOut, nightlyRate, cleaningFee, description, slug = "casanova" } = body;

  if (!guestName || !guestEmail || !checkIn || !checkOut || nightlyRate === undefined) {
    return NextResponse.json({ error: "Missing required fields" }, { status: 400 });
  }

  await ensureSchema();

  const stripe = getStripe();
  const propertyId = auth?.propertyId ?? null;

  // Calculate nights
  const start = new Date(checkIn + "T12:00:00Z");
  const end = new Date(checkOut + "T12:00:00Z");
  const nights = Math.round((end.getTime() - start.getTime()) / 86400000);
  if (nights <= 0) return NextResponse.json({ error: "Invalid dates" }, { status: 400 });

  const nightlyTotal = Number(nightlyRate) * nights;
  const cleaning = Number(cleaningFee ?? 0);
  const subtotal = nightlyTotal + cleaning;
  const processingFee = Math.round((subtotal * 0.029 + 0.30) * 100) / 100;
  const grandTotal = subtotal + processingFee;

  const origin = req.headers.get("origin") || process.env.NEXT_PUBLIC_APP_URL || "https://nova-stay.vercel.app";

  const lineItems: Stripe.Checkout.SessionCreateParams.LineItem[] = [
    {
      price_data: {
        currency: "usd",
        product_data: {
          name: `${nights} Night${nights !== 1 ? "s" : ""} — Casanova ATL`,
          description: description || `Check-in: ${checkIn}  ·  Check-out: ${checkOut}`,
        },
        unit_amount: Math.round(nightlyTotal * 100),
      },
      quantity: 1,
    },
  ];

  if (cleaning > 0) {
    lineItems.push({
      price_data: {
        currency: "usd",
        product_data: { name: "Cleaning Fee" },
        unit_amount: Math.round(cleaning * 100),
      },
      quantity: 1,
    });
  }

  lineItems.push({
    price_data: {
      currency: "usd",
      product_data: { name: "Processing Fee" },
      unit_amount: Math.round(processingFee * 100),
    },
    quantity: 1,
  });

  const session = await stripe.checkout.sessions.create({
    mode: "payment",
    payment_method_types: ["card"],
    customer_email: guestEmail,
    line_items: lineItems,
    metadata: {
      propertyId: String(propertyId),
      slug,
      checkIn,
      checkOut,
      nights: String(nights),
      guestName,
      guestEmail,
      totalPrice: String(grandTotal),
      source: "host_quote",
    },
    success_url: `${origin}/${slug}?booking=success`,
    cancel_url: `${origin}/${slug}/book`,
  });

  // Hold the dates immediately as pending
  const confirmationCode = `QUOTE-${Date.now()}`;
  await sql`
    INSERT INTO bookings (confirmation_code, guest_name, guest_email, check_in, check_out, nights,
      booked_guests, booking_source, property_id, payment_status, stripe_session_id, total_price)
    VALUES (${confirmationCode}, ${guestName}, ${guestEmail}, ${checkIn}, ${checkOut}, ${nights},
      1, 'direct', ${propertyId}, 'pending', ${session.id}, ${grandTotal});
  `;

  return NextResponse.json({ url: session.url, total: grandTotal, nights, processingFee });
}
