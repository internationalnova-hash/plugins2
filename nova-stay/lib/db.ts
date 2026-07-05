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
    ALTER TABLE property_state ADD COLUMN IF NOT EXISTS property_name TEXT;
  `;
  await sql`
    ALTER TABLE property_state ADD COLUMN IF NOT EXISTS host_name TEXT;
  `;
  await sql`
    ALTER TABLE property_state ADD COLUMN IF NOT EXISTS city TEXT;
  `;
  await sql`
    ALTER TABLE property_state ADD COLUMN IF NOT EXISTS tagline TEXT;
  `;
  await sql`
    ALTER TABLE property_state ADD COLUMN IF NOT EXISTS hero_image_url TEXT;
  `;
  await sql`
    ALTER TABLE property_state ADD COLUMN IF NOT EXISTS gold_color TEXT;
  `;
  await sql`
    ALTER TABLE property_state ADD COLUMN IF NOT EXISTS amenities TEXT;
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
    ALTER TABLE guest_requests ADD COLUMN IF NOT EXISTS confirmation_code TEXT;
  `;
  await sql`
    ALTER TABLE guest_requests ADD COLUMN IF NOT EXISTS priority TEXT;
  `;
  await sql`
    ALTER TABLE guest_requests ADD COLUMN IF NOT EXISTS category TEXT;
  `;
  await sql`
    ALTER TABLE guest_requests ADD COLUMN IF NOT EXISTS suggested_response TEXT;
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
    CREATE TABLE IF NOT EXISTS guest_conversations (
      id SERIAL PRIMARY KEY,
      confirmation_code TEXT NOT NULL,
      role TEXT NOT NULL,
      content TEXT NOT NULL,
      created_at TIMESTAMPTZ NOT NULL DEFAULT now()
    );
  `;

  await sql`
    CREATE TABLE IF NOT EXISTS guest_inbox (
      id SERIAL PRIMARY KEY,
      confirmation_code TEXT NOT NULL,
      message TEXT NOT NULL,
      created_at TIMESTAMPTZ NOT NULL DEFAULT now()
    );
  `;

  await sql`
    CREATE TABLE IF NOT EXISTS amenity_views (
      id SERIAL PRIMARY KEY,
      confirmation_code TEXT,
      guest_name TEXT,
      amenity TEXT NOT NULL,
      viewed_at TIMESTAMPTZ NOT NULL DEFAULT now()
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

  // ───────────────────────────────────────────────────────────────────────
  // Multi-tenancy: a host account can own multiple properties. Every
  // guest/booking/content table below gets a nullable property_id so
  // existing single-property data keeps working while we migrate it.
  // ───────────────────────────────────────────────────────────────────────
  await sql`
    CREATE TABLE IF NOT EXISTS hosts (
      id SERIAL PRIMARY KEY,
      email TEXT UNIQUE NOT NULL,
      password_hash TEXT NOT NULL,
      name TEXT NOT NULL,
      created_at TIMESTAMPTZ NOT NULL DEFAULT now()
    );
  `;

  await sql`
    CREATE TABLE IF NOT EXISTS host_sessions (
      token TEXT PRIMARY KEY,
      host_id INTEGER NOT NULL REFERENCES hosts(id) ON DELETE CASCADE,
      created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
      expires_at TIMESTAMPTZ NOT NULL DEFAULT (now() + interval '30 days')
    );
  `;

  await sql`
    CREATE TABLE IF NOT EXISTS properties (
      id SERIAL PRIMARY KEY,
      host_id INTEGER NOT NULL REFERENCES hosts(id) ON DELETE CASCADE,
      slug TEXT UNIQUE NOT NULL,
      name TEXT NOT NULL DEFAULT 'My Property',
      host_name TEXT,
      city TEXT,
      tagline TEXT,
      hero_image_url TEXT,
      gold_color TEXT,
      amenities TEXT,
      pool_lights_on BOOLEAN NOT NULL DEFAULT false,
      door_code TEXT NOT NULL DEFAULT '0000',
      host_notice TEXT,
      host_notice_at TIMESTAMPTZ,
      created_at TIMESTAMPTZ NOT NULL DEFAULT now()
    );
  `;

  await sql`ALTER TABLE bookings ADD COLUMN IF NOT EXISTS property_id INTEGER REFERENCES properties(id);`;
  await sql`ALTER TABLE guest_requests ADD COLUMN IF NOT EXISTS property_id INTEGER REFERENCES properties(id);`;
  await sql`ALTER TABLE guest_messages ADD COLUMN IF NOT EXISTS property_id INTEGER REFERENCES properties(id);`;
  await sql`ALTER TABLE experience_requests ADD COLUMN IF NOT EXISTS property_id INTEGER REFERENCES properties(id);`;
  await sql`ALTER TABLE guest_conversations ADD COLUMN IF NOT EXISTS property_id INTEGER REFERENCES properties(id);`;
  await sql`ALTER TABLE guest_inbox ADD COLUMN IF NOT EXISTS property_id INTEGER REFERENCES properties(id);`;
  await sql`ALTER TABLE amenity_views ADD COLUMN IF NOT EXISTS property_id INTEGER REFERENCES properties(id);`;
  await sql`ALTER TABLE stay_guide ADD COLUMN IF NOT EXISTS property_id INTEGER REFERENCES properties(id);`;
  await sql`ALTER TABLE bookings ADD COLUMN IF NOT EXISTS checked_in_at TIMESTAMPTZ;`;
  await sql`ALTER TABLE bookings ADD COLUMN IF NOT EXISTS payment_status TEXT NOT NULL DEFAULT 'unpaid';`;
  await sql`ALTER TABLE bookings ADD COLUMN IF NOT EXISTS stripe_session_id TEXT;`;
  await sql`ALTER TABLE bookings ADD COLUMN IF NOT EXISTS total_price NUMERIC(10,2);`;
  await sql`ALTER TABLE bookings ADD COLUMN IF NOT EXISTS guest_email TEXT;`;

  await sql`
    CREATE TABLE IF NOT EXISTS date_prices (
      id SERIAL PRIMARY KEY,
      property_id INTEGER REFERENCES properties(id) ON DELETE CASCADE,
      date DATE NOT NULL,
      price_per_night NUMERIC(10,2) NOT NULL,
      UNIQUE(property_id, date)
    );
  `;
  await sql`
    CREATE TABLE IF NOT EXISTS property_pricing (
      property_id INTEGER PRIMARY KEY REFERENCES properties(id) ON DELETE CASCADE,
      default_price_per_night NUMERIC(10,2) NOT NULL DEFAULT 250,
      min_nights INTEGER NOT NULL DEFAULT 1,
      cleaning_fee NUMERIC(10,2) NOT NULL DEFAULT 175,
      updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
    );
  `;
  await sql`ALTER TABLE property_pricing ADD COLUMN IF NOT EXISTS cleaning_fee NUMERIC(10,2) NOT NULL DEFAULT 175;`;

  // One-time migration: fold the legacy single-property data (property_state
  // id=1, stay_guide id=1, and any rows with a null property_id) into a
  // "casanova" property under a seeded host, so existing data keeps working
  // under the new multi-tenant model without manual intervention.
  const { rows: existingProps } = await sql`SELECT id FROM properties LIMIT 1;`;
  if (!existingProps.length) {
    const seedEmail = process.env.SEED_HOST_EMAIL || "junior@staybynova.com";
    const seedPassword = process.env.SEED_HOST_PASSWORD || process.env.HOST_ADMIN_PASSWORD || "changeme";
    const { hashPassword } = await import("./crypto");
    const passwordHash = await hashPassword(seedPassword);

    const { rows: hostRows } = await sql`
      INSERT INTO hosts (email, password_hash, name)
      VALUES (${seedEmail}, ${passwordHash}, 'Junior')
      ON CONFLICT (email) DO UPDATE SET email = EXCLUDED.email
      RETURNING id;
    `;
    const hostId = hostRows[0].id;

    const { rows: stateRows } = await sql`
      SELECT pool_lights_on, door_code, host_notice, host_notice_at,
             property_name, host_name, city, tagline, hero_image_url, gold_color, amenities
      FROM property_state WHERE id = 1;
    `;
    const state = stateRows[0];

    const { rows: propRows } = await sql`
      INSERT INTO properties (
        host_id, slug, name, host_name, city, tagline, hero_image_url, gold_color, amenities,
        pool_lights_on, door_code, host_notice, host_notice_at
      )
      VALUES (
        ${hostId}, 'casanova', ${state?.property_name || 'Casanova ATL'}, ${state?.host_name || 'Junior'},
        ${state?.city || 'Atlanta, Georgia'}, ${state?.tagline || 'Your Luxury Home Away From Home'},
        ${state?.hero_image_url || null}, ${state?.gold_color || null}, ${state?.amenities || null},
        ${state?.pool_lights_on ?? false}, ${state?.door_code || '1710#'}, ${state?.host_notice || null}, ${state?.host_notice_at || null}
      )
      RETURNING id;
    `;
    const propertyId = propRows[0].id;

    await sql`UPDATE bookings SET property_id = ${propertyId} WHERE property_id IS NULL;`;
    await sql`UPDATE guest_requests SET property_id = ${propertyId} WHERE property_id IS NULL;`;
    await sql`UPDATE guest_messages SET property_id = ${propertyId} WHERE property_id IS NULL;`;
    await sql`UPDATE experience_requests SET property_id = ${propertyId} WHERE property_id IS NULL;`;
    await sql`UPDATE guest_conversations SET property_id = ${propertyId} WHERE property_id IS NULL;`;
    await sql`UPDATE guest_inbox SET property_id = ${propertyId} WHERE property_id IS NULL;`;
    await sql`UPDATE amenity_views SET property_id = ${propertyId} WHERE property_id IS NULL;`;
    await sql`UPDATE stay_guide SET property_id = ${propertyId} WHERE property_id IS NULL;`;
  }
}

export function ensureSchema(): Promise<void> {
  if (!schemaReady) {
    schemaReady = createSchema();
  }
  return schemaReady;
}
