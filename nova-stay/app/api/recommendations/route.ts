import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { isAuthorizedHost } from "@/lib/hostAuth";
import property from "@/config/property";

// Amenities that signal interest in a paid experience, mapped to the
// matching marketplace listing (see data/experiences.ts).
const AMENITY_TO_EXPERIENCE: Record<string, { id: string; title: string }> = {
  theater: { id: "content-creation", title: "Content Creation" },
  studio: { id: "recording-studio", title: "Recording Studio" },
  pool: { id: "pool-heating", title: "Pool Heating" },
  grill: { id: "private-chef", title: "Private Chef" },
};

const VIEW_THRESHOLD = 3;

function addDays(dateStr: string, days: number): string {
  const d = new Date(dateStr + "T00:00:00");
  d.setDate(d.getDate() + days);
  return d.toISOString().slice(0, 10);
}

export async function GET(req: NextRequest) {
  if (!isAuthorizedHost(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  try {
    await ensureSchema();

    const [bookingsRes, requestsRes, viewsRes, weather] = await Promise.all([
      sql`SELECT confirmation_code, guest_name, check_in, check_out FROM bookings;`,
      sql`SELECT confirmation_code, experience_title, status FROM experience_requests WHERE status != 'declined';`,
      sql`
        SELECT confirmation_code, guest_name, amenity, COUNT(*) AS views
        FROM amenity_views
        WHERE viewed_at > now() - interval '14 days'
        GROUP BY confirmation_code, guest_name, amenity;
      `,
      fetchWeather(),
    ]);

    const today = new Date().toISOString().slice(0, 10);
    const recommendations: { id: string; text: string; confirmationCode?: string; suggestedMessage?: string }[] = [];

    const requestedTitles = new Set(
      requestsRes.rows.map((r) => `${r.confirmation_code || ""}::${r.experience_title}`)
    );
    const bookingByCode = new Map(bookingsRes.rows.map((b) => [b.confirmation_code, b]));

    // 1. Amenity interest -> experience upsell
    for (const row of viewsRes.rows) {
      const amenity = row.amenity as string;
      const mapping = AMENITY_TO_EXPERIENCE[amenity];
      if (!mapping) continue;
      if (Number(row.views) < VIEW_THRESHOLD) continue;
      const key = `${row.confirmation_code || ""}::${mapping.title}`;
      if (requestedTitles.has(key)) continue;
      const who = row.guest_name || "A guest";
      const code = row.confirmation_code as string | null;
      const booking = code ? bookingByCode.get(code) : null;
      recommendations.push({
        id: `view-${code || "anon"}-${amenity}`,
        text: `${who} viewed ${amenity.replace("-", " ")} ${row.views}x. Recommend sending the ${mapping.title} package.`,
        confirmationCode: booking ? code! : undefined,
        suggestedMessage: booking
          ? `Hi ${booking.guest_name}! We noticed you're checking out the ${amenity.replace("-", " ")} space — during your stay we also offer our ${mapping.title} package if you're interested. Just let us know!`
          : undefined,
      });
    }

    // 2. Pool heating upsell for currently-hosted guests in cooler weather
    if (weather && weather.tempF < 75) {
      for (const b of bookingsRes.rows) {
        if (b.check_in > today || b.check_out <= today) continue;
        const key = `${b.confirmation_code}::Pool Heating`;
        if (requestedTitles.has(key)) continue;
        recommendations.push({
          id: `pool-heat-${b.confirmation_code}`,
          text: `Recommend offering Pool Heating to the ${b.guest_name} reservation — it's ${weather.tempF}° today.`,
          confirmationCode: b.confirmation_code,
          suggestedMessage: `Hi ${b.guest_name}! It's a bit cool out today (${weather.tempF}°) — we'd be happy to warm up the pool for the rest of your stay if you're interested. Just let us know!`,
        });
      }
    }

    // 3. Tomorrow's arrivals -> towel prep
    const tomorrow = addDays(today, 1);
    const arrivingTomorrow = bookingsRes.rows.filter((b) => b.check_in === tomorrow);
    if (arrivingTomorrow.length > 0) {
      const sets = arrivingTomorrow.length * 2;
      recommendations.push({
        id: "towels-tomorrow",
        text: `Guests arriving tomorrow typically request extra towels. Prepare ${sets} additional sets.`,
      });
    }

    // 4. Guests currently hosted who haven't opened the guide (no amenity views logged)
    const viewedCodes = new Set(viewsRes.rows.map((r) => r.confirmation_code));
    for (const b of bookingsRes.rows) {
      if (b.check_in > today || b.check_out <= today) continue;
      if (viewedCodes.has(b.confirmation_code)) continue;
      recommendations.push({
        id: `unopened-${b.confirmation_code}`,
        text: `${b.guest_name} hasn't opened the guide yet.`,
      });
    }

    return NextResponse.json({
      todaysFocus: {
        arrivals: bookingsRes.rows.filter((b) => b.check_in === today).length,
        checkouts: bookingsRes.rows.filter((b) => b.check_out === today).length,
        tempF: weather?.tempF ?? null,
        pendingExperienceRequests: requestsRes.rows.filter((r) => r.status === "requested").length,
      },
      recommendations,
    });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Database error while building recommendations." }, { status: 500 });
  }
}

async function fetchWeather(): Promise<{ tempF: number } | null> {
  try {
    const { lat, lon } = property.coordinates;
    const res = await fetch(
      `https://api.open-meteo.com/v1/forecast?latitude=${lat}&longitude=${lon}&current=temperature_2m&temperature_unit=fahrenheit&timezone=auto`
    );
    if (!res.ok) return null;
    const data = await res.json();
    return { tempF: Math.round(data.current.temperature_2m) };
  } catch {
    return null;
  }
}
