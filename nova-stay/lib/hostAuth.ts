import { NextRequest } from "next/server";
import { sql } from "./db";

export function isSuperAdmin(req: NextRequest): boolean {
  const expected = process.env.HOST_ADMIN_PASSWORD;
  if (!expected) return false;
  return req.headers.get("x-host-token") === expected;
}

export interface AuthorizedHost {
  hostId: number;
  propertyId: number;
}

// Validates the session token belongs to a host who owns the property
// identified by the x-property-slug header.
export async function getAuthorizedProperty(req: NextRequest): Promise<AuthorizedHost | null> {
  const sessionToken = req.headers.get("x-session-token");
  const slug = req.headers.get("x-property-slug");
  if (!sessionToken || !slug) return null;

  try {
    const { rows } = await sql`
      SELECT hs.host_id AS host_id, p.id AS property_id
      FROM host_sessions hs
      JOIN properties p ON p.host_id = hs.host_id AND p.slug = ${slug}
      WHERE hs.token = ${sessionToken} AND hs.expires_at > now()
      LIMIT 1;
    `;
    if (!rows[0]) return null;
    return { hostId: rows[0].host_id, propertyId: rows[0].property_id };
  } catch {
    return null;
  }
}

// True if the request is either a valid host session scoped to a property,
// or the legacy super-admin password (kept for the admin panel).
export async function isAuthorizedHost(req: NextRequest): Promise<boolean> {
  if (isSuperAdmin(req)) return true;
  const auth = await getAuthorizedProperty(req);
  return auth !== null;
}

export async function getPropertyIdBySlug(slug: string): Promise<number | null> {
  try {
    const { rows } = await sql`SELECT id FROM properties WHERE slug = ${slug} LIMIT 1;`;
    return rows[0]?.id ?? null;
  } catch {
    return null;
  }
}

export async function getPropertyIdByConfirmationCode(code: string): Promise<number | null> {
  try {
    const { rows } = await sql`SELECT property_id FROM bookings WHERE UPPER(confirmation_code) = ${code.toUpperCase()} LIMIT 1;`;
    return rows[0]?.property_id ?? null;
  } catch {
    return null;
  }
}
