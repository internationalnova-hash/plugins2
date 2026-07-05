import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { getAuthorizedProperty, isSuperAdmin } from "@/lib/hostAuth";

export async function GET(req: NextRequest) {
  const auth = await getAuthorizedProperty(req);
  if (!auth && !isSuperAdmin(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  await ensureSchema();

  const propertyId = auth?.propertyId;
  if (!propertyId) return NextResponse.json({ defaultPrice: 250, minNights: 1, cleaningFee: 175 });

  const { rows } = await sql`
    SELECT default_price_per_night, min_nights, cleaning_fee
    FROM property_pricing
    WHERE property_id = ${propertyId}
    LIMIT 1;
  `;

  const row = rows[0];
  return NextResponse.json({
    defaultPrice: row ? Number(row.default_price_per_night) : 250,
    minNights: row ? Number(row.min_nights) : 1,
    cleaningFee: row ? Number(row.cleaning_fee) : 175,
  });
}

export async function POST(req: NextRequest) {
  const auth = await getAuthorizedProperty(req);
  if (!auth && !isSuperAdmin(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  await ensureSchema();

  const { defaultPrice, minNights, cleaningFee } = await req.json();
  const propertyId = auth?.propertyId;
  if (!propertyId) return NextResponse.json({ error: "No property" }, { status: 400 });

  await sql`
    INSERT INTO property_pricing (property_id, default_price_per_night, min_nights, cleaning_fee, updated_at)
    VALUES (${propertyId}, ${Number(defaultPrice) || 250}, ${Number(minNights) || 1}, ${Number(cleaningFee) ?? 175}, now())
    ON CONFLICT (property_id)
    DO UPDATE SET default_price_per_night = EXCLUDED.default_price_per_night,
                  min_nights = EXCLUDED.min_nights,
                  cleaning_fee = EXCLUDED.cleaning_fee,
                  updated_at = now();
  `;

  return NextResponse.json({ ok: true });
}
