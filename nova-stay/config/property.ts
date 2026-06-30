// ─────────────────────────────────────────────────────────────────────────────
// StayByNova — Property Configuration
//
// To deploy this guide for a new property:
//   1. Drop the hero photo into /public/images/hero/
//   2. Update the fields below
//   3. Redeploy to Vercel
//
// No component code needs to change.
// ─────────────────────────────────────────────────────────────────────────────

const DEFAULTS = {
  name: "Casanova ATL",
  hostName: "Junior",
  city: "Atlanta, Georgia",
  tagline: "Your Luxury Home Away From Home",
  amenities: ["Luxury Pool", "Theater", "Recording Studio", "Game Room"],
  heroImage: "/images/hero/casanova-hero.jpg",
};

// Hosts can override identity fields via the Settings screen; those values
// are synced from the database into localStorage on app boot (see
// components/PropertySync.tsx) so this still works for server-rendered
// defaults while reflecting host edits on the client without touching every
// component that imports this file.
function loadOverrides(): Partial<typeof DEFAULTS> {
  if (typeof window === "undefined") return {};
  try {
    const raw = localStorage.getItem("stayByNova_propertyOverrides");
    const parsed = raw ? JSON.parse(raw) : null;
    if (!parsed) return {};
    const overrides: Partial<typeof DEFAULTS> = {};
    if (parsed.propertyName) overrides.name = parsed.propertyName;
    if (parsed.hostName) overrides.hostName = parsed.hostName;
    if (parsed.city) overrides.city = parsed.city;
    if (parsed.tagline) overrides.tagline = parsed.tagline;
    if (parsed.heroImageUrl) overrides.heroImage = parsed.heroImageUrl;
    if (parsed.amenities) overrides.amenities = parsed.amenities.split(",").map((a: string) => a.trim()).filter(Boolean);
    return overrides;
  } catch {
    return {};
  }
}

const property = {
  ...DEFAULTS,
  ...loadOverrides(),

  // Coordinates — used to fetch real local weather (Open-Meteo)
  coordinates: { lat: 33.749, lon: -84.388 },

  // Social links (set to "" to hide)
  social: {
    instagram: "https://instagram.com/internationalnova",
    facebook: "",
    youtube: "",
    tiktok: "",
  },
};

export default property;
