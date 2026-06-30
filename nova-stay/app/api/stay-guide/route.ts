import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { getAuthorizedProperty, isSuperAdmin, getPropertyIdByConfirmationCode, getPropertyIdBySlug } from "@/lib/hostAuth";

export async function GET(req: NextRequest) {
  try {
    await ensureSchema();
    const code = req.nextUrl.searchParams.get("code")?.trim().toUpperCase();
    const slug = req.nextUrl.searchParams.get("slug")?.trim();
    let propertyId: number | null = null;
    if (code) propertyId = await getPropertyIdByConfirmationCode(code);
    else if (slug) propertyId = await getPropertyIdBySlug(slug);

    const { rows } = propertyId
      ? await sql`SELECT content, updated_at FROM stay_guide WHERE property_id = ${propertyId} LIMIT 1;`
      : await sql`SELECT content, updated_at FROM stay_guide WHERE id = 1;`;
    const row = rows[0];
    return NextResponse.json({ guide: row ? { content: row.content, updatedAt: row.updated_at } : null });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while loading guide." }, { status: 500 });
  }
}

export async function POST(req: NextRequest) {
  const auth = await getAuthorizedProperty(req);
  if (!auth && !isSuperAdmin(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const body = await req.json().catch(() => ({}));
  const { content } = body;
  if (!content || typeof content !== "object") {
    return NextResponse.json({ error: "Missing content" }, { status: 400 });
  }

  try {
    await ensureSchema();
    if (auth) {
      const { rows: existing } = await sql`SELECT id FROM stay_guide WHERE property_id = ${auth.propertyId} LIMIT 1;`;
      if (existing[0]) {
        await sql`UPDATE stay_guide SET content = ${JSON.stringify(content)}::jsonb, updated_at = now() WHERE property_id = ${auth.propertyId};`;
      } else {
        await sql`INSERT INTO stay_guide (content, updated_at, property_id) VALUES (${JSON.stringify(content)}::jsonb, now(), ${auth.propertyId});`;
      }
    } else {
      await sql`
        INSERT INTO stay_guide (id, content, updated_at)
        VALUES (1, ${JSON.stringify(content)}::jsonb, now())
        ON CONFLICT (id) DO UPDATE SET content = ${JSON.stringify(content)}::jsonb, updated_at = now();
      `;
    }
    return NextResponse.json({ ok: true });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while saving guide." }, { status: 500 });
  }
}
