import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { getAuthorizedProperty, isSuperAdmin } from "@/lib/hostAuth";

// POST — set a custom price for a specific date
export async function POST(req: NextRequest) {
  const auth = await getAuthorizedProperty(req);
  if (!auth && !isSuperAdmin(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  await ensureSchema();

  const { date, price } = await req.json();
  if (!date || price === undefined || price === null) {
    return NextResponse.json({ error: "date and price are required" }, { status: 400 });
  }

  const propertyId = auth?.propertyId;
  if (!propertyId) return NextResponse.json({ error: "No property" }, { status: 400 });

  if (price === null || price === "") {
    // Delete custom price (revert to default)
    await sql`DELETE FROM date_prices WHERE property_id = ${propertyId} AND date = ${date}::DATE;`;
  } else {
    await sql`
      INSERT INTO date_prices (property_id, date, price_per_night)
      VALUES (${propertyId}, ${date}::DATE, ${Number(price)})
      ON CONFLICT (property_id, date)
      DO UPDATE SET price_per_night = EXCLUDED.price_per_night;
    `;
  }

  return NextResponse.json({ ok: true });
}

// DELETE — remove custom price for a date
export async function DELETE(req: NextRequest) {
  const auth = await getAuthorizedProperty(req);
  if (!auth && !isSuperAdmin(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  await ensureSchema();

  const { date } = await req.json();
  const propertyId = auth?.propertyId;
  if (!propertyId) return NextResponse.json({ error: "No property" }, { status: 400 });

  await sql`DELETE FROM date_prices WHERE property_id = ${propertyId} AND date = ${date}::DATE;`;
  return NextResponse.json({ ok: true });
}
