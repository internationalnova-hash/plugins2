import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { isAuthorizedHost } from "@/lib/hostAuth";

export async function GET() {
  try {
    await ensureSchema();
    const { rows } = await sql`SELECT content, updated_at FROM stay_guide WHERE id = 1;`;
    const row = rows[0];
    return NextResponse.json({ guide: row ? { content: row.content, updatedAt: row.updated_at } : null });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while loading guide." }, { status: 500 });
  }
}

export async function POST(req: NextRequest) {
  if (!isAuthorizedHost(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const body = await req.json().catch(() => ({}));
  const { content } = body;
  if (!content || typeof content !== "object") {
    return NextResponse.json({ error: "Missing content" }, { status: 400 });
  }

  try {
    await ensureSchema();
    await sql`
      INSERT INTO stay_guide (id, content, updated_at)
      VALUES (1, ${JSON.stringify(content)}::jsonb, now())
      ON CONFLICT (id) DO UPDATE SET content = ${JSON.stringify(content)}::jsonb, updated_at = now();
    `;
    return NextResponse.json({ ok: true });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while saving guide." }, { status: 500 });
  }
}
