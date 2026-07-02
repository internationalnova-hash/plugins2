// ─────────────────────────────────────────────────────────────────────────────
// StayByNova — Bookings Store
//
// Reservations are created and managed through the Host Reservation Manager
// UI (inside Host View), not by editing source files. This module is the
// single read/write boundary for that data — backed by a shared Postgres
// database via /api/bookings, so reservations are visible to every guest's
// device, not just the host's own browser.
//
// All functions are async now (network calls). Callers should await them.
// ─────────────────────────────────────────────────────────────────────────────

export type BookingSource = "airbnb" | "vrbo" | "direct" | "booking.com" | "other";

export interface Booking {
  confirmationCode: string;
  guestName: string;
  checkIn: string;
  checkOut: string;
  nights: number;
  bookedGuests: number;
  bookingSource: BookingSource;
}

const HOST_TOKEN_KEY = "stayByNova_hostToken";
const SESSION_TOKEN_KEY = "nova_session_token";
const ACTIVE_PROPERTY_SLUG_KEY = "nova_active_property_slug";

function getHostToken(): string | null {
  if (typeof window === "undefined") return null;
  return localStorage.getItem(HOST_TOKEN_KEY);
}

function getSessionToken(): string | null {
  if (typeof window === "undefined") return null;
  return localStorage.getItem(SESSION_TOKEN_KEY);
}

function getActivePropertySlug(): string | null {
  if (typeof window === "undefined") return null;
  return localStorage.getItem(ACTIVE_PROPERTY_SLUG_KEY);
}

export function authHeaders(): Record<string, string> {
  const headers: Record<string, string> = {};
  const token = getHostToken();
  if (token) headers["x-host-token"] = token;
  const sessionToken = getSessionToken();
  if (sessionToken) headers["x-session-token"] = sessionToken;
  const slug = getActivePropertySlug();
  if (slug) headers["x-property-slug"] = slug;
  return headers;
}

export async function loginHost(password: string): Promise<boolean> {
  const res = await fetch("/api/bookings/login", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ password }),
  });
  if (res.ok) {
    localStorage.setItem(HOST_TOKEN_KEY, password);
    return true;
  }
  return false;
}

export function isHostLoggedIn(): boolean {
  return !!getHostToken();
}

export function logoutHost(): void {
  localStorage.removeItem(HOST_TOKEN_KEY);
}

export async function getBookings(): Promise<Booking[]> {
  const res = await fetch("/api/bookings", { headers: authHeaders() });
  if (!res.ok) return [];
  const data = await res.json();
  return data.bookings as Booking[];
}

export async function findBooking(code: string): Promise<Booking | undefined> {
  const res = await fetch(`/api/bookings/lookup?code=${encodeURIComponent(code.trim())}`);
  if (!res.ok) return undefined;
  const data = await res.json();
  return data.booking ?? undefined;
}

export async function addBooking(booking: Booking): Promise<{ ok: boolean; error?: string }> {
  const res = await fetch("/api/bookings", {
    method: "POST",
    headers: { "Content-Type": "application/json", ...authHeaders() },
    body: JSON.stringify(booking),
  });
  if (!res.ok) {
    const data = await res.json().catch(() => ({}));
    return { ok: false, error: data.error || "Failed to add reservation" };
  }
  return { ok: true };
}

export async function updateBooking(booking: Booking): Promise<{ ok: boolean; error?: string }> {
  const res = await fetch("/api/bookings", {
    method: "PUT",
    headers: { "Content-Type": "application/json", ...authHeaders() },
    body: JSON.stringify(booking),
  });
  if (!res.ok) {
    const data = await res.json().catch(() => ({}));
    return { ok: false, error: data.error || "Failed to update reservation." };
  }
  return { ok: true };
}

export async function deleteBooking(confirmationCode: string): Promise<boolean> {
  const res = await fetch(`/api/bookings/${encodeURIComponent(confirmationCode)}`, {
    method: "DELETE",
    headers: authHeaders(),
  });
  return res.ok;
}
