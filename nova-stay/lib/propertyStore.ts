import { authHeaders } from "@/lib/bookingsStore";

export interface PropertyState {
  poolLightsOn: boolean;
  doorCode: string;
  hostNotice: string | null;
  hostNoticeAt: string | null;
}

export interface GuestRequest {
  id: number;
  guestName: string;
  message: string;
  isRead: boolean;
  createdAt: string;
}

export async function getPropertyState(): Promise<PropertyState | null> {
  const res = await fetch("/api/property");
  if (!res.ok) return null;
  return res.json();
}

export async function updatePropertyState(
  patch: Partial<Pick<PropertyState, "poolLightsOn" | "doorCode" | "hostNotice">>
): Promise<{ ok: boolean; error?: string }> {
  const res = await fetch("/api/property", {
    method: "PATCH",
    headers: { "Content-Type": "application/json", ...authHeaders() },
    body: JSON.stringify(patch),
  });
  if (!res.ok) {
    const data = await res.json().catch(() => ({}));
    return { ok: false, error: data.error || "Failed to update property." };
  }
  return { ok: true };
}

export async function getGuestRequests(): Promise<GuestRequest[]> {
  const res = await fetch("/api/requests", { headers: authHeaders() });
  if (!res.ok) return [];
  const data = await res.json();
  return data.requests as GuestRequest[];
}

export async function submitGuestRequest(guestName: string, message: string): Promise<{ ok: boolean; error?: string }> {
  const res = await fetch("/api/requests", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ guestName, message }),
  });
  if (!res.ok) {
    const data = await res.json().catch(() => ({}));
    return { ok: false, error: data.error || "Failed to send request." };
  }
  return { ok: true };
}

export async function markRequestRead(id: number): Promise<boolean> {
  const res = await fetch("/api/requests", {
    method: "PATCH",
    headers: { "Content-Type": "application/json", ...authHeaders() },
    body: JSON.stringify({ id }),
  });
  return res.ok;
}

export interface WelcomeMessage {
  text: string;
  createdAt: string;
}

export async function getWelcomeMessage(confirmationCode: string): Promise<WelcomeMessage | null> {
  const res = await fetch(`/api/messages?code=${encodeURIComponent(confirmationCode)}`);
  if (!res.ok) return null;
  const data = await res.json();
  return data.message ?? null;
}

export async function getHostBriefing(question?: string): Promise<{ ok: boolean; briefing?: string; error?: string }> {
  const res = await fetch("/api/host-assistant", {
    method: "POST",
    headers: { "Content-Type": "application/json", ...authHeaders() },
    body: JSON.stringify(question ? { question } : {}),
  });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) {
    return { ok: false, error: data.error || "Failed to generate briefing." };
  }
  return { ok: true, briefing: data.briefing };
}

export async function sendWelcomeMessage(confirmationCode: string, message: string): Promise<{ ok: boolean; error?: string }> {
  const res = await fetch("/api/messages", {
    method: "POST",
    headers: { "Content-Type": "application/json", ...authHeaders() },
    body: JSON.stringify({ confirmationCode, message }),
  });
  if (!res.ok) {
    const data = await res.json().catch(() => ({}));
    return { ok: false, error: data.error || "Failed to send message." };
  }
  return { ok: true };
}
