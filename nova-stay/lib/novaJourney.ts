export type JourneyStageKey =
  | "before"
  | "checkedIn"
  | "enjoying"
  | "experiences"
  | "checkout"
  | "review";

export interface JourneyStage {
  key: JourneyStageKey;
  label: string;
}

export const JOURNEY_STAGES: JourneyStage[] = [
  { key: "before",      label: "Before Arrival" },
  { key: "checkedIn",   label: "Checked In" },
  { key: "enjoying",    label: "Enjoying Your Stay" },
  { key: "experiences", label: "Experiences" },
  { key: "checkout",    label: "Checkout" },
  { key: "review",      label: "Leave a Review" },
];

function parseLooseDate(value: string): Date | null {
  if (!value) return null;
  const now = new Date();
  const withYear = /\d{4}/.test(value) ? value : `${value}, ${now.getFullYear()}`;
  const parsed = new Date(withYear);
  if (Number.isNaN(parsed.getTime())) return null;
  return parsed;
}

function startOfDay(d: Date): Date {
  const copy = new Date(d);
  copy.setHours(0, 0, 0, 0);
  return copy;
}

/**
 * Best-effort stage detection from free-text check-in/checkout dates
 * (e.g. "July 24"). Falls back to "enjoying" when dates can't be parsed,
 * since that's the safest mid-stay default.
 */
export function getJourneyStage(checkIn: string, checkOut: string): JourneyStageKey {
  const inDate = parseLooseDate(checkIn);
  const outDate = parseLooseDate(checkOut);
  if (!inDate || !outDate) return "enjoying";

  const today = startOfDay(new Date());
  const ci = startOfDay(inDate);
  const co = startOfDay(outDate);

  if (today < ci) return "before";
  if (today.getTime() === ci.getTime()) return "checkedIn";
  if (today >= co) return "review";
  if (today.getTime() === co.getTime() - 86400000) return "checkout";
  return "enjoying";
}
