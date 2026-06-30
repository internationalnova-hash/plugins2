export function trackAmenityView(amenity: string): void {
  let guestName: string | null = null;
  let confirmationCode: string | null = null;
  try {
    const raw = localStorage.getItem("stayByNova_guestInfo");
    const parsed = raw ? JSON.parse(raw) : null;
    if (parsed?.guestName) guestName = parsed.guestName;
    if (parsed?.confirmationCode) confirmationCode = parsed.confirmationCode;
  } catch {
    /* ignore */
  }

  fetch("/api/track", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ confirmationCode, guestName, amenity }),
  }).catch(() => {});
}
