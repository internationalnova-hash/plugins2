import { NextRequest, NextResponse } from "next/server";
import Stripe from "stripe";
import { sql, ensureSchema } from "@/lib/db";

function getStripe() {
  const key = process.env.STRIPE_SECRET_KEY;
  if (!key) throw new Error("STRIPE_SECRET_KEY is not set");
  return new Stripe(key, { apiVersion: "2026-06-24.dahlia" });
}

export async function POST(req: NextRequest) {
  const webhookSecret = process.env.STRIPE_WEBHOOK_SECRET;
  if (!webhookSecret) {
    return NextResponse.json({ error: "Webhook secret not configured" }, { status: 500 });
  }

  const body = await req.text();
  const signature = req.headers.get("stripe-signature");
  if (!signature) return NextResponse.json({ error: "No signature" }, { status: 400 });

  let event: Stripe.Event;
  try {
    const stripe = getStripe();
    event = stripe.webhooks.constructEvent(body, signature, webhookSecret);
  } catch (err: any) {
    return NextResponse.json({ error: `Webhook verification failed: ${err.message}` }, { status: 400 });
  }

  if (event.type === "checkout.session.completed") {
    const session = event.data.object as Stripe.Checkout.Session;
    await ensureSchema();

    // Mark the pending booking as paid
    await sql`
      UPDATE bookings
      SET payment_status = 'paid', checked_in_at = NULL
      WHERE stripe_session_id = ${session.id};
    `;
  }

  if (event.type === "checkout.session.expired") {
    const session = event.data.object as Stripe.Checkout.Session;
    await ensureSchema();
    // Release the held dates
    await sql`DELETE FROM bookings WHERE stripe_session_id = ${session.id} AND payment_status = 'pending';`;
  }

  return NextResponse.json({ received: true });
}
