import { NextRequest } from "next/server";

export function isAuthorizedHost(req: NextRequest): boolean {
  const expected = process.env.HOST_ADMIN_PASSWORD;
  if (!expected) return false; // fail closed if not configured
  const provided = req.headers.get("x-host-token");
  return provided === expected;
}
