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

export type ExperienceRequestStatus = "requested" | "approved" | "billed" | "paid" | "scheduled" | "completed" | "declined";

export interface ExperienceRequest {
  id: number;
  confirmationCode: string | null;
  guestName: string;
  experienceTitle: string;
  priceFrom: string | null;
  bookingSource: string;
  status: ExperienceRequestStatus;
  createdAt: string;
}

export async function submitExperienceRequest(
  confirmationCode: string | null,
  guestName: string,
  experienceTitle: string,
  priceFrom: string
): Promise<{ ok: boolean; error?: string }> {
  const res = await fetch("/api/experience-requests", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ confirmationCode, guestName, experienceTitle, priceFrom }),
  });
  if (!res.ok) {
    const data = await res.json().catch(() => ({}));
    return { ok: false, error: data.error || "Failed to send request." };
  }
  return { ok: true };
}

export async function getExperienceRequests(): Promise<ExperienceRequest[]> {
  const res = await fetch("/api/experience-requests", { headers: authHeaders() });
  if (!res.ok) return [];
  const data = await res.json();
  return data.requests as ExperienceRequest[];
}

export async function updateExperienceRequestStatus(id: number, status: ExperienceRequestStatus): Promise<boolean> {
  const res = await fetch("/api/experience-requests", {
    method: "PATCH",
    headers: { "Content-Type": "application/json", ...authHeaders() },
    body: JSON.stringify({ id, status }),
  });
  return res.ok;
}

export interface StayGuide {
  welcomeMessage: string;
  checkinInstructions: string;
  parkingInstructions: string;
  checkoutInstructions: string;
  houseRules: string;
  amenitiesHighlights: string;
  diningRecommendations: string;
  faq: string;
  emergencyContacts: string;
}

export async function getStayGuide(): Promise<{ content: StayGuide; updatedAt: string } | null> {
  const res = await fetch("/api/stay-guide");
  if (!res.ok) return null;
  const data = await res.json();
  return data.guide ?? null;
}

export async function saveStayGuide(content: StayGuide): Promise<{ ok: boolean; error?: string }> {
  const res = await fetch("/api/stay-guide", {
    method: "POST",
    headers: { "Content-Type": "application/json", ...authHeaders() },
    body: JSON.stringify({ content }),
  });
  if (!res.ok) {
    const data = await res.json().catch(() => ({}));
    return { ok: false, error: data.error || "Failed to save guide." };
  }
  return { ok: true };
}

export async function generateStayGuide(answers: Record<string, string>): Promise<{ ok: boolean; guide?: StayGuide; error?: string }> {
  const res = await fetch("/api/stay-builder", {
    method: "POST",
    headers: { "Content-Type": "application/json", ...authHeaders() },
    body: JSON.stringify({ answers }),
  });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) {
    return { ok: false, error: data.error || "Failed to generate guide." };
  }
  return { ok: true, guide: data.guide };
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
