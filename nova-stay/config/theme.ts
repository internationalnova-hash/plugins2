// ─────────────────────────────────────────────────────────────────────────────
// StayByNova — Theme Configuration
//
// Single source of truth for brand colors. Every component reads from here
// instead of redeclaring its own gold/bg constants, so re-skinning the app
// for a different property is a one-file change.
// ─────────────────────────────────────────────────────────────────────────────

const DEFAULT_GOLD = "#C9A84C";

// Shades derived from a base gold are recomputed below so a host-picked
// accent color still produces a coherent light/bright/dark ramp instead of
// clashing with the hardcoded defaults.
function shade(hex: string, percent: number): string {
  const n = parseInt(hex.replace("#", ""), 16);
  const r = Math.min(255, Math.max(0, ((n >> 16) & 0xff) + percent));
  const g = Math.min(255, Math.max(0, ((n >> 8) & 0xff) + percent));
  const b = Math.min(255, Math.max(0, (n & 0xff) + percent));
  return `#${((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1)}`;
}

function loadGoldOverride(): string {
  if (typeof window === "undefined") return DEFAULT_GOLD;
  try {
    const raw = localStorage.getItem("stayByNova_propertyOverrides");
    const parsed = raw ? JSON.parse(raw) : null;
    return parsed?.goldColor || DEFAULT_GOLD;
  } catch {
    return DEFAULT_GOLD;
  }
}

const gold = loadGoldOverride();

const theme = {
  gold,
  goldLight: shade(gold, 50),
  goldBright: shade(gold, 70),
  goldDark: shade(gold, -30),
  bg: "#0A0A0A",
  card: "#1A1A1A",
  border: "#2A2A2A",
};

export default theme;
