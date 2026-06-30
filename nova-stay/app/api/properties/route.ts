import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";

// List the properties belonging to the host identified by the session
// token — used to populate the property switcher right after login.
export async function GET(req: NextRequest) {
  const token = req.headers.get("x-session-token");
  if (!token) return NextResponse.json({ error: "Unauthorized" }, { status: 401 });

  try {
    await ensureSchema();
    const { rows: sessionRows } = await sql`
      SELECT host_id FROM host_sessions WHERE token = ${token} AND expires_at > now() LIMIT 1;
    `;
    if (!sessionRows[0]) return NextResponse.json({ error: "Session expired" }, { status: 401 });

    const { rows } = await sql`
      SELECT id, slug, name FROM properties WHERE host_id = ${sessionRows[0].host_id} ORDER BY created_at ASC;
    `;
    return NextResponse.json({ properties: rows });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while loading properties." }, { status: 500 });
  }
}

// A logged-in host adds a new property of their own (e.g. via Stay Builder).
export async function POST(req: NextRequest) {
  const token = req.headers.get("x-session-token");
  if (!token) return NextResponse.json({ error: "Unauthorized" }, { status: 401 });

  const body = await req.json().catch(() => ({}));
  const { slug, name } = body;
  if (!slug?.trim() || !name?.trim()) {
    return NextResponse.json({ error: "Slug and name are required." }, { status: 400 });
  }
  const cleanSlug = slug.trim().toLowerCase().replace(/[^a-z0-9-]/g, "-");

  try {
    await ensureSchema();
    const { rows: sessionRows } = await sql`
      SELECT host_id FROM host_sessions WHERE token = ${token} AND expires_at > now() LIMIT 1;
    `;
    if (!sessionRows[0]) return NextResponse.json({ error: "Session expired" }, { status: 401 });

    const { rows } = await sql`
      INSERT INTO properties (host_id, slug, name)
      VALUES (${sessionRows[0].host_id}, ${cleanSlug}, ${name.trim()})
      RETURNING id, slug, name;
    `;
    return NextResponse.json({ ok: true, property: rows[0] }, { status: 201 });
  } catch (err: any) {
    if (err?.code === "23505") {
      return NextResponse.json({ error: "That property URL is already taken — try a different one." }, { status: 409 });
    }
    return NextResponse.json({ error: err?.message || "Database error while creating property." }, { status: 500 });
  }
}
