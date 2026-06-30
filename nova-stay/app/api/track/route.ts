import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";

export async function POST(req: NextRequest) {
  const body = await req.json().catch(() => ({}));
  const { confirmationCode, guestName, amenity } = body;

  if (!amenity) {
    return NextResponse.json({ error: "Missing amenity" }, { status: 400 });
  }

  try {
    await ensureSchema();
    await sql`
      INSERT INTO amenity_views (confirmation_code, guest_name, amenity)
      VALUES (${confirmationCode ? confirmationCode.trim().toUpperCase() : null}, ${guestName || null}, ${amenity});
    `;
    return NextResponse.json({ ok: true }, { status: 201 });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while tracking view." }, { status: 500 });
  }
}
