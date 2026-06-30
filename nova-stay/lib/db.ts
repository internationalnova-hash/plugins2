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

async function createSchema(): Promise<void> {
  await sql`
    CREATE TABLE IF NOT EXISTS bookings (
      confirmation_code TEXT PRIMARY KEY,
      guest_name TEXT NOT NULL,
      check_in TEXT NOT NULL,
      check_out TEXT NOT NULL,
      nights INTEGER NOT NULL,
      booked_guests INTEGER NOT NULL,
      created_at TIMESTAMPTZ NOT NULL DEFAULT now()
    );
  `;
  await sql`
    ALTER TABLE bookings ADD COLUMN IF NOT EXISTS booking_source TEXT NOT NULL DEFAULT 'airbnb';
  `;

  await sql`
    CREATE TABLE IF NOT EXISTS property_state (
      id INTEGER PRIMARY KEY DEFAULT 1,
      pool_lights_on BOOLEAN NOT NULL DEFAULT false,
      door_code TEXT NOT NULL DEFAULT '1710#',
      host_notice TEXT,
      host_notice_at TIMESTAMPTZ
    );
  `;
  await sql`
    INSERT INTO property_state (id) VALUES (1) ON CONFLICT (id) DO NOTHING;
  `;

  await sql`
    CREATE TABLE IF NOT EXISTS guest_requests (
      id SERIAL PRIMARY KEY,
      guest_name TEXT NOT NULL,
      message TEXT NOT NULL,
      is_read BOOLEAN NOT NULL DEFAULT false,
      created_at TIMESTAMPTZ NOT NULL DEFAULT now()
    );
  `;

  await sql`
    CREATE TABLE IF NOT EXISTS guest_messages (
      confirmation_code TEXT PRIMARY KEY,
      message TEXT NOT NULL,
      created_at TIMESTAMPTZ NOT NULL DEFAULT now()
    );
  `;

  await sql`
    CREATE TABLE IF NOT EXISTS experience_requests (
      id SERIAL PRIMARY KEY,
      confirmation_code TEXT,
      guest_name TEXT NOT NULL,
      experience_title TEXT NOT NULL,
      price_from TEXT,
      booking_source TEXT NOT NULL DEFAULT 'direct',
      status TEXT NOT NULL DEFAULT 'requested',
      created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
      updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
    );
  `;

  await sql`
    CREATE TABLE IF NOT EXISTS stay_guide (
      id INTEGER PRIMARY KEY DEFAULT 1,
      content JSONB NOT NULL DEFAULT '{}'::jsonb,
      updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
    );
  `;
  await sql`
    INSERT INTO stay_guide (id) VALUES (1) ON CONFLICT (id) DO NOTHING;
  `;
}

export function ensureSchema(): Promise<void> {
  if (!schemaReady) {
    schemaReady = createSchema();
  }
  return schemaReady;
}
