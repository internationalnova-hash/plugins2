import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { getAuthorizedProperty, isSuperAdmin } from "@/lib/hostAuth";

export async function DELETE(req: NextRequest, { params }: { params: { code: string } }) {
  const auth = await getAuthorizedProperty(req);
  if (!auth && !isSuperAdmin(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  await ensureSchema();
  if (auth) {
    await sql`DELETE FROM bookings WHERE confirmation_code = ${params.code.toUpperCase()} AND property_id = ${auth.propertyId};`;
  } else {
    await sql`DELETE FROM bookings WHERE confirmation_code = ${params.code.toUpperCase()};`;
  }

  return NextResponse.json({ ok: true });
}
