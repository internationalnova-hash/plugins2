import { Pool } from "pg";

// ─────────────────────────────────────────────────────────────────────────────
// @vercel/postgres only speaks Neon's HTTP/fetch-based serverless protocol —
// it can't connect to a plain Postgres host like Supabase. We use the
// standard `pg` driver instead, which works with any Postgres provider, and
// wrap it in a `sql` tagged-template helper so call sites are unchanged.
// ─────────────────────────────────────────────────────────────────────────────

const rawConnectionString =
  process.env.POSTGRES_URL || process.env.DATABASE_URL || process.env.POSTGRES_URL_NON_POOLING;

// Strip any sslmode param from the URL — pg-connection-string derives its own
// ssl config from it, which can take precedence over the `ssl` option below
// and reject Supabase's self-signed chain. We force our own ssl settings instead.
function stripSslMode(url: string): string {
  return url.replace(/([?&])sslmode=[^&]*&?/i, (match, sep) => (match.endsWith("&") ? sep : "")).replace(/[?&]$/, "");
}

const connectionString = rawConnectionString ? stripSslMode(rawConnectionString) : undefined;

let pool: Pool | null = null;

function getPool(): Pool {
  if (!pool) {
    if (!connectionString) {
      throw new Error("No database connection string found (POSTGRES_URL/DATABASE_URL is not set).");
    }
    pool = new Pool({
      connectionString,
      ssl: { rejectUnauthorized: false },
    });
  }
  return pool;
}

export async function sql(strings: TemplateStringsArray, ...values: unknown[]) {
  const text = strings.reduce((acc, str, i) => acc + (i > 0 ? `$${i}` : "") + str, "");
  const result = await getPool().query(text, values);
  return result;
}

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
