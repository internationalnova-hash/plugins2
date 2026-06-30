import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { verifyPassword, generateToken } from "@/lib/crypto";

export async function POST(req: NextRequest) {
  const body = await req.json().catch(() => ({}));
  const { email, password } = body;
  if (!email || !password) {
    return NextResponse.json({ error: "Email and password are required." }, { status: 400 });
  }

  try {
    await ensureSchema();
    const { rows } = await sql`SELECT id, password_hash, name FROM hosts WHERE email = ${email.trim().toLowerCase()} LIMIT 1;`;
    const host = rows[0];
    if (!host || !(await verifyPassword(password, host.password_hash))) {
      return NextResponse.json({ error: "Incorrect email or password." }, { status: 401 });
    }

    const token = generateToken();
    await sql`INSERT INTO host_sessions (token, host_id) VALUES (${token}, ${host.id});`;

    const { rows: properties } = await sql`SELECT id, slug, name FROM properties WHERE host_id = ${host.id} ORDER BY created_at ASC;`;

    return NextResponse.json({
      ok: true,
      sessionToken: token,
      host: { id: host.id, name: host.name },
      properties: properties.map((p) => ({ id: p.id, slug: p.slug, name: p.name })),
    });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Login failed." }, { status: 500 });
  }
}
