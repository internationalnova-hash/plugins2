import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { getPropertyIdByConfirmationCode } from "@/lib/hostAuth";

export async function POST(req: NextRequest) {
  const body = await req.json().catch(() => ({}));
  const { confirmationCode, guestName, amenity } = body;

  if (!amenity) {
    return NextResponse.json({ error: "Missing amenity" }, { status: 400 });
  }

  try {
    await ensureSchema();
    const code = confirmationCode ? confirmationCode.trim().toUpperCase() : null;
    const propertyId = code ? await getPropertyIdByConfirmationCode(code) : null;
    await sql`
      INSERT INTO amenity_views (confirmation_code, guest_name, amenity, property_id)
      VALUES (${code}, ${guestName || null}, ${amenity}, ${propertyId});
    `;
    return NextResponse.json({ ok: true }, { status: 201 });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while tracking view." }, { status: 500 });
  }
}
