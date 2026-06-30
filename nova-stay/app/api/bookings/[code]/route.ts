import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { isAuthorizedHost } from "@/lib/hostAuth";

export async function DELETE(req: NextRequest, { params }: { params: { code: string } }) {
  if (!isAuthorizedHost(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  await ensureSchema();
  await sql`DELETE FROM bookings WHERE confirmation_code = ${params.code.toUpperCase()};`;

  return NextResponse.json({ ok: true });
}
