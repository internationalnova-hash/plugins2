import { NextRequest, NextResponse } from "next/server";

export async function POST(req: NextRequest) {
  const { password } = await req.json();
  const expected = process.env.HOST_ADMIN_PASSWORD;

  if (!expected) {
    return NextResponse.json({ error: "Host admin password not configured" }, { status: 500 });
  }
  if (password !== expected) {
    return NextResponse.json({ error: "Incorrect password" }, { status: 401 });
  }

  return NextResponse.json({ ok: true });
}
