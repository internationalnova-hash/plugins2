import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { getAuthorizedProperty, isSuperAdmin } from "@/lib/hostAuth";

export async function GET(req: NextRequest) {
  try {
    await ensureSchema();
    const auth = await getAuthorizedProperty(req);

    const { rows } = auth
      ? await sql`
          SELECT pool_lights_on, door_code, host_notice, host_notice_at,
                 name AS property_name, host_name, city, tagline, hero_image_url, gold_color, amenities
          FROM properties WHERE id = ${auth.propertyId};
        `
      : await sql`
          SELECT pool_lights_on, door_code, host_notice, host_notice_at,
                 property_name, host_name, city, tagline, hero_image_url, gold_color, amenities
          FROM property_state WHERE id = 1;
        `;
    const row = rows[0];
    return NextResponse.json({
      poolLightsOn: row?.pool_lights_on ?? false,
      doorCode: row?.door_code ?? "0000",
      hostNotice: row?.host_notice ?? null,
      hostNoticeAt: row?.host_notice_at ?? null,
      propertyName: row?.property_name ?? null,
      hostName: row?.host_name ?? null,
      city: row?.city ?? null,
      tagline: row?.tagline ?? null,
      heroImageUrl: row?.hero_image_url ?? null,
      goldColor: row?.gold_color ?? null,
      amenities: row?.amenities ?? null,
    });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while loading property state." }, { status: 500 });
  }
}

export async function PATCH(req: NextRequest) {
  const auth = await getAuthorizedProperty(req);
  if (!auth && !isSuperAdmin(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const body = await req.json();
  const { poolLightsOn, doorCode, hostNotice, propertyName, hostName, city, tagline, heroImageUrl, goldColor, amenities } = body;

  try {
    await ensureSchema();
    if (auth) {
      const id = auth.propertyId;
      if (typeof poolLightsOn === "boolean") {
        await sql`UPDATE properties SET pool_lights_on = ${poolLightsOn} WHERE id = ${id};`;
      }
      if (typeof doorCode === "string" && doorCode.trim()) {
        await sql`UPDATE properties SET door_code = ${doorCode.trim()} WHERE id = ${id};`;
      }
      if (typeof hostNotice === "string") {
        await sql`UPDATE properties SET host_notice = ${hostNotice || null}, host_notice_at = now() WHERE id = ${id};`;
      }
      if (typeof propertyName === "string") {
        await sql`UPDATE properties SET name = ${propertyName.trim() || null} WHERE id = ${id};`;
      }
      if (typeof hostName === "string") {
        await sql`UPDATE properties SET host_name = ${hostName.trim() || null} WHERE id = ${id};`;
      }
      if (typeof city === "string") {
        await sql`UPDATE properties SET city = ${city.trim() || null} WHERE id = ${id};`;
      }
      if (typeof tagline === "string") {
        await sql`UPDATE properties SET tagline = ${tagline.trim() || null} WHERE id = ${id};`;
      }
      if (typeof heroImageUrl === "string") {
        await sql`UPDATE properties SET hero_image_url = ${heroImageUrl.trim() || null} WHERE id = ${id};`;
      }
      if (typeof goldColor === "string") {
        await sql`UPDATE properties SET gold_color = ${goldColor.trim() || null} WHERE id = ${id};`;
      }
      if (typeof amenities === "string") {
        await sql`UPDATE properties SET amenities = ${amenities.trim() || null} WHERE id = ${id};`;
      }
    } else {
      if (typeof poolLightsOn === "boolean") {
        await sql`UPDATE property_state SET pool_lights_on = ${poolLightsOn} WHERE id = 1;`;
      }
      if (typeof doorCode === "string" && doorCode.trim()) {
        await sql`UPDATE property_state SET door_code = ${doorCode.trim()} WHERE id = 1;`;
      }
      if (typeof hostNotice === "string") {
        await sql`UPDATE property_state SET host_notice = ${hostNotice || null}, host_notice_at = now() WHERE id = 1;`;
      }
      if (typeof propertyName === "string") {
        await sql`UPDATE property_state SET property_name = ${propertyName.trim() || null} WHERE id = 1;`;
      }
      if (typeof hostName === "string") {
        await sql`UPDATE property_state SET host_name = ${hostName.trim() || null} WHERE id = 1;`;
      }
      if (typeof city === "string") {
        await sql`UPDATE property_state SET city = ${city.trim() || null} WHERE id = 1;`;
      }
      if (typeof tagline === "string") {
        await sql`UPDATE property_state SET tagline = ${tagline.trim() || null} WHERE id = 1;`;
      }
      if (typeof heroImageUrl === "string") {
        await sql`UPDATE property_state SET hero_image_url = ${heroImageUrl.trim() || null} WHERE id = 1;`;
      }
      if (typeof goldColor === "string") {
        await sql`UPDATE property_state SET gold_color = ${goldColor.trim() || null} WHERE id = 1;`;
      }
      if (typeof amenities === "string") {
        await sql`UPDATE property_state SET amenities = ${amenities.trim() || null} WHERE id = 1;`;
      }
    }
    return NextResponse.json({ ok: true });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while updating property state." }, { status: 500 });
  }
}
