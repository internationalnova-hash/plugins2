import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { isSuperAdmin } from "@/lib/hostAuth";
import { hashPassword } from "@/lib/crypto";

// Super-admin only: list every host and their properties.
export async function GET(req: NextRequest) {
  if (!isSuperAdmin(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  try {
    await ensureSchema();
    const { rows } = await sql`
      SELECT h.id, h.email, h.name, h.created_at,
             COALESCE(json_agg(json_build_object('id', p.id, 'slug', p.slug, 'name', p.name)) FILTER (WHERE p.id IS NOT NULL), '[]') AS properties
      FROM hosts h
      LEFT JOIN properties p ON p.host_id = h.id
      GROUP BY h.id
      ORDER BY h.created_at ASC;
    `;
    return NextResponse.json({
      hosts: rows.map((row) => ({ id: row.id, email: row.email, name: row.name, createdAt: row.created_at, properties: row.properties })),
    });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while loading hosts." }, { status: 500 });
  }
}

// Super-admin only: invite a new host. Optionally creates their first
// property in the same call.
export async function POST(req: NextRequest) {
  if (!isSuperAdmin(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const body = await req.json().catch(() => ({}));
  const { email, password, name, propertySlug, propertyName } = body;
  if (!email?.trim() || !password?.trim() || !name?.trim()) {
    return NextResponse.json({ error: "Email, password, and name are required." }, { status: 400 });
  }

  try {
    await ensureSchema();
    const passwordHash = await hashPassword(password.trim());
    const { rows } = await sql`
      INSERT INTO hosts (email, password_hash, name)
      VALUES (${email.trim().toLowerCase()}, ${passwordHash}, ${name.trim()})
      RETURNING id;
    `;
    const hostId = rows[0].id;

    let property = null;
    if (propertySlug?.trim()) {
      const { rows: propRows } = await sql`
        INSERT INTO properties (host_id, slug, name)
        VALUES (${hostId}, ${propertySlug.trim().toLowerCase()}, ${propertyName?.trim() || "My Property"})
        RETURNING id, slug, name;
      `;
      property = propRows[0];
    }

    return NextResponse.json({ ok: true, hostId, property }, { status: 201 });
  } catch (err: any) {
    if (err?.code === "23505") {
      return NextResponse.json({ error: "That email or property slug is already taken." }, { status: 409 });
    }
    return NextResponse.json({ error: err?.message || "Database error while creating host." }, { status: 500 });
  }
}
