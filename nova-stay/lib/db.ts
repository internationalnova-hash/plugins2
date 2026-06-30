import { sql } from "@vercel/postgres";

let schemaReady: Promise<void> | null = null;

export function ensureSchema(): Promise<void> {
  if (!schemaReady) {
    schemaReady = sql`
      CREATE TABLE IF NOT EXISTS bookings (
        confirmation_code TEXT PRIMARY KEY,
        guest_name TEXT NOT NULL,
        check_in TEXT NOT NULL,
        check_out TEXT NOT NULL,
        nights INTEGER NOT NULL,
        booked_guests INTEGER NOT NULL,
        created_at TIMESTAMPTZ NOT NULL DEFAULT now()
      );
    `.then(() => undefined);
  }
  return schemaReady;
}

export { sql };
