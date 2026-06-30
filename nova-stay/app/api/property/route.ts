import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { isAuthorizedHost } from "@/lib/hostAuth";

export async function GET() {
  try {
    await ensureSchema();
    const { rows } = await sql`
      SELECT pool_lights_on, door_code, host_notice, host_notice_at
      FROM property_state WHERE id = 1;
    `;
    const row = rows[0];
    return NextResponse.json({
      poolLightsOn: row?.pool_lights_on ?? false,
      doorCode: row?.door_code ?? "0000",
      hostNotice: row?.host_notice ?? null,
      hostNoticeAt: row?.host_notice_at ?? null,
    });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while loading property state." }, { status: 500 });
  }
}

export async function PATCH(req: NextRequest) {
  if (!isAuthorizedHost(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const body = await req.json();
  const { poolLightsOn, doorCode, hostNotice } = body;

  try {
    await ensureSchema();
    if (typeof poolLightsOn === "boolean") {
      await sql`UPDATE property_state SET pool_lights_on = ${poolLightsOn} WHERE id = 1;`;
    }
    if (typeof doorCode === "string" && doorCode.trim()) {
      await sql`UPDATE property_state SET door_code = ${doorCode.trim()} WHERE id = 1;`;
    }
    if (typeof hostNotice === "string") {
      await sql`UPDATE property_state SET host_notice = ${hostNotice || null}, host_notice_at = now() WHERE id = 1;`;
    }
    return NextResponse.json({ ok: true });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while updating property state." }, { status: 500 });
  }
}
