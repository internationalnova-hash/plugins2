import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";

export async function POST(req: NextRequest) {
  const token = req.headers.get("x-session-token");
  if (!token) return NextResponse.json({ ok: true });

  try {
    await ensureSchema();
    await sql`DELETE FROM host_sessions WHERE token = ${token};`;
  } catch { /* ignore */ }

  return NextResponse.json({ ok: true });
}
