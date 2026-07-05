"use client";

import { useState, useEffect, useCallback } from "react";

interface BookedRange {
  checkIn: string;
  checkOut: string;
}

interface CalendarProps {
  slug?: string;
  onBook?: (checkIn: string, checkOut: string, nights: number, total: number) => void;
}

function isoDate(y: number, m: number, d: number) {
  return `${y}-${String(m + 1).padStart(2, "0")}-${String(d).padStart(2, "0")}`;
}

function isBooked(date: string, ranges: BookedRange[]) {
  return ranges.some((r) => date >= r.checkIn && date < r.checkOut);
}

function addMonths(y: number, m: number, delta: number) {
  const d = new Date(y, m + delta, 1);
  return { year: d.getFullYear(), month: d.getMonth() };
}

const MONTH_NAMES = ["January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December"];

export default function AvailabilityCalendar({ slug = "casanova", onBook }: CalendarProps) {
  const today = new Date();
  const [viewYear, setViewYear] = useState(today.getFullYear());
  const [viewMonth, setViewMonth] = useState(today.getMonth());
  const [bookedRanges, setBookedRanges] = useState<BookedRange[]>([]);
  const [prices, setPrices] = useState<Record<string, number>>({});
  const [defaultPrice, setDefaultPrice] = useState(250);
  const [minNights, setMinNights] = useState(1);
  const [cleaningFee, setCleaningFee] = useState(175);
  const [checkIn, setCheckIn] = useState<string | null>(null);
  const [checkOut, setCheckOut] = useState<string | null>(null);
  const [hovered, setHovered] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);
  const [guestName, setGuestName] = useState("");
  const [guestEmail, setGuestEmail] = useState("");
  const [bookingStep, setBookingStep] = useState<"calendar" | "details" | "processing">("calendar");
  const [bookingError, setBookingError] = useState("");

  useEffect(() => {
    fetch(`/api/availability?slug=${slug}`)
      .then((r) => r.json())
      .then((data) => {
        setBookedRanges(data.bookedRanges || []);
        setPrices(data.prices || {});
        setDefaultPrice(data.defaultPrice ?? 250);
        setMinNights(data.minNights ?? 1);
        setCleaningFee(data.cleaningFee ?? 175);
      })
      .finally(() => setLoading(false));
  }, [slug]);

  const getPrice = (date: string) => prices[date] ?? defaultPrice;

  const calcTotal = useCallback((ci: string, co: string) => {
    let total = 0;
    const cur = new Date(ci + "T12:00:00Z");
    const end = new Date(co + "T12:00:00Z");
    while (cur < end) {
      total += getPrice(cur.toISOString().slice(0, 10));
      cur.setUTCDate(cur.getUTCDate() + 1);
    }
    return total;
  }, [prices, defaultPrice]);

  const nightsBetween = (ci: string, co: string) => {
    const ms = new Date(co + "T12:00:00Z").getTime() - new Date(ci + "T12:00:00Z").getTime();
    return Math.round(ms / 86400000);
  };

  const handleDayClick = (dateStr: string) => {
    if (isBooked(dateStr, bookedRanges)) return;
    if (dateStr < today.toISOString().slice(0, 10)) return;

    if (!checkIn || (checkIn && checkOut)) {
      setCheckIn(dateStr);
      setCheckOut(null);
    } else {
      if (dateStr <= checkIn) {
        setCheckIn(dateStr);
        setCheckOut(null);
        return;
      }
      // Validate no booked dates in range
      const cur = new Date(checkIn + "T12:00:00Z");
      const end = new Date(dateStr + "T12:00:00Z");
      let hasConflict = false;
      while (cur < end) {
        if (isBooked(cur.toISOString().slice(0, 10), bookedRanges)) { hasConflict = true; break; }
        cur.setUTCDate(cur.getUTCDate() + 1);
      }
      if (hasConflict) { setCheckIn(dateStr); setCheckOut(null); return; }
      const nights = nightsBetween(checkIn, dateStr);
      if (nights < minNights) { return; }
      setCheckOut(dateStr);
    }
  };

  const renderMonth = (year: number, month: number) => {
    const firstDay = new Date(year, month, 1).getDay();
    const daysInMonth = new Date(year, month + 1, 0).getDate();
    const todayStr = today.toISOString().slice(0, 10);
    const endDate = checkIn && !checkOut && hovered && hovered > checkIn ? hovered : checkOut;

    const cells = [];
    for (let i = 0; i < firstDay; i++) cells.push(<div key={`e${i}`} />);

    for (let d = 1; d <= daysInMonth; d++) {
      const dateStr = isoDate(year, month, d);
      const booked = isBooked(dateStr, bookedRanges);
      const past = dateStr < todayStr;
      const isCheckIn = dateStr === checkIn;
      const isCheckOut = dateStr === checkOut;
      const inRange = checkIn && endDate && dateStr > checkIn && dateStr < endDate;
      const isStart = isCheckIn;
      const isEnd = isCheckOut || (checkIn && !checkOut && hovered === dateStr && dateStr > checkIn);
      const price = getPrice(dateStr);
      const isCustomPrice = prices[dateStr] !== undefined;

      let bg = "transparent";
      let textColor = booked || past ? "#555" : "#fff";
      let cursor = booked || past ? "default" : "pointer";
      let border = "1px solid transparent";

      if (isStart || isEnd) { bg = "#b8982a"; textColor = "#000"; }
      else if (inRange) { bg = "rgba(184,152,42,0.2)"; border = "1px solid rgba(184,152,42,0.3)"; }
      else if (!booked && !past && dateStr === hovered) { bg = "rgba(255,255,255,0.08)"; }

      if (booked) { bg = "rgba(255,80,80,0.15)"; border = "1px solid rgba(255,80,80,0.25)"; }

      cells.push(
        <div
          key={dateStr}
          onClick={() => handleDayClick(dateStr)}
          onMouseEnter={() => setHovered(dateStr)}
          onMouseLeave={() => setHovered(null)}
          style={{
            padding: "6px 4px",
            borderRadius: 8,
            background: bg,
            border,
            cursor,
            textAlign: "center",
            opacity: past || booked ? 0.45 : 1,
            userSelect: "none",
          }}
        >
          <div style={{ fontSize: 14, color: textColor, fontWeight: isStart || isEnd ? 700 : 400 }}>{d}</div>
          {!past && !booked && (
            <div style={{ fontSize: 9, color: isCustomPrice ? "#ffd700" : "#aaa", marginTop: 1 }}>
              ${price}
            </div>
          )}
          {booked && <div style={{ fontSize: 9, color: "#ff6060" }}>booked</div>}
        </div>
      );
    }

    return cells;
  };

  const selectedNights = checkIn && checkOut ? nightsBetween(checkIn, checkOut) : 0;
  const nightlySubtotal = checkIn && checkOut ? calcTotal(checkIn, checkOut) : 0;
  const processingFee = checkIn && checkOut ? Math.round((nightlySubtotal + cleaningFee) * 0.029 * 100 + 30) / 100 : 0;
  const selectedTotal = nightlySubtotal + cleaningFee + processingFee;

  const handleProceed = () => {
    if (!checkIn || !checkOut) return;
    setBookingStep("details");
  };

  const handleSubmit = async () => {
    if (!guestName.trim() || !guestEmail.trim()) {
      setBookingError("Please enter your name and email.");
      return;
    }
    setBookingError("");
    setBookingStep("processing");
    try {
      const res = await fetch("/api/checkout", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ checkIn, checkOut, guestName, guestEmail, slug }),
      });
      const data = await res.json();
      if (!res.ok) { setBookingError(data.error || "Something went wrong."); setBookingStep("details"); return; }
      window.location.href = data.url;
    } catch {
      setBookingError("Network error. Please try again.");
      setBookingStep("details");
    }
  };

  const { year: prevY, month: prevM } = addMonths(viewYear, viewMonth, -1);
  const { year: nextY, month: nextM } = addMonths(viewYear, viewMonth, 1);
  const canGoPrev = viewYear > today.getFullYear() || viewMonth > today.getMonth();

  if (loading) {
    return (
      <div style={{ textAlign: "center", padding: 40, color: "#aaa" }}>Loading availability…</div>
    );
  }

  return (
    <div style={{ maxWidth: 700, margin: "0 auto", padding: "0 16px" }}>
      <h2 style={{ color: "#b8982a", fontSize: 22, fontWeight: 700, marginBottom: 4, textAlign: "center" }}>
        Check Availability
      </h2>
      <p style={{ color: "#888", textAlign: "center", fontSize: 13, marginBottom: 24 }}>
        Select your check-in and check-out dates
      </p>

      {bookingStep === "calendar" && (
        <>
          {/* Month navigation */}
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 16 }}>
            <button
              onClick={() => { if (canGoPrev) { setViewYear(prevY); setViewMonth(prevM); } }}
              disabled={!canGoPrev}
              style={{ background: "none", border: "none", color: canGoPrev ? "#b8982a" : "#444", fontSize: 20, cursor: canGoPrev ? "pointer" : "default" }}
            >‹</button>
            <span style={{ color: "#fff", fontWeight: 600, fontSize: 16 }}>
              {MONTH_NAMES[viewMonth]} {viewYear}
            </span>
            <button
              onClick={() => { setViewYear(nextY); setViewMonth(nextM); }}
              style={{ background: "none", border: "none", color: "#b8982a", fontSize: 20, cursor: "pointer" }}
            >›</button>
          </div>

          {/* Day headers */}
          <div style={{ display: "grid", gridTemplateColumns: "repeat(7,1fr)", gap: 4, marginBottom: 4 }}>
            {["Sun","Mon","Tue","Wed","Thu","Fri","Sat"].map((d) => (
              <div key={d} style={{ textAlign: "center", color: "#666", fontSize: 11, fontWeight: 600, padding: "4px 0" }}>{d}</div>
            ))}
          </div>

          {/* Calendar grid */}
          <div style={{ display: "grid", gridTemplateColumns: "repeat(7,1fr)", gap: 4 }}>
            {renderMonth(viewYear, viewMonth)}
          </div>

          {/* Legend */}
          <div style={{ display: "flex", gap: 20, marginTop: 16, justifyContent: "center" }}>
            {[
              { color: "#b8982a", label: "Selected" },
              { color: "rgba(255,80,80,0.4)", label: "Booked" },
              { color: "#ffd700", label: "Custom price" },
            ].map(({ color, label }) => (
              <div key={label} style={{ display: "flex", alignItems: "center", gap: 6 }}>
                <div style={{ width: 12, height: 12, borderRadius: 3, background: color }} />
                <span style={{ color: "#888", fontSize: 11 }}>{label}</span>
              </div>
            ))}
          </div>

          {/* Selection summary */}
          {checkIn && (
            <div style={{ marginTop: 24, background: "rgba(184,152,42,0.1)", border: "1px solid rgba(184,152,42,0.3)", borderRadius: 12, padding: "16px 20px" }}>
              <div style={{ display: "flex", justifyContent: "space-between", flexWrap: "wrap", gap: 8 }}>
                <div>
                  <div style={{ color: "#888", fontSize: 11, marginBottom: 2 }}>CHECK-IN</div>
                  <div style={{ color: "#fff", fontWeight: 600 }}>{checkIn}</div>
                </div>
                {checkOut ? (
                  <>
                    <div>
                      <div style={{ color: "#888", fontSize: 11, marginBottom: 2 }}>CHECK-OUT</div>
                      <div style={{ color: "#fff", fontWeight: 600 }}>{checkOut}</div>
                    </div>
                    <div>
                      <div style={{ color: "#888", fontSize: 11, marginBottom: 2 }}>NIGHTS</div>
                      <div style={{ color: "#fff", fontWeight: 600 }}>{selectedNights}</div>
                    </div>
                  </>
                ) : (
                  <div style={{ color: "#aaa", fontSize: 13, alignSelf: "center" }}>
                    {minNights > 1 ? `Select check-out (min ${minNights} nights)` : "Now select your check-out date"}
                  </div>
                )}
              </div>

              {checkOut && (
                <div style={{ marginTop: 16, borderTop: "1px solid rgba(184,152,42,0.2)", paddingTop: 14 }}>
                  {[
                    { label: `${selectedNights} night${selectedNights !== 1 ? "s" : ""} × avg $${selectedNights ? Math.round(nightlySubtotal / selectedNights) : 0}`, value: nightlySubtotal },
                    { label: "Cleaning fee", value: cleaningFee },
                    { label: "Processing fee (2.9% + $0.30)", value: processingFee },
                  ].map(({ label, value }) => (
                    <div key={label} style={{ display: "flex", justifyContent: "space-between", marginBottom: 6 }}>
                      <span style={{ color: "#aaa", fontSize: 13 }}>{label}</span>
                      <span style={{ color: "#fff", fontSize: 13 }}>${value.toFixed(2)}</span>
                    </div>
                  ))}
                  <div style={{ display: "flex", justifyContent: "space-between", borderTop: "1px solid rgba(184,152,42,0.2)", paddingTop: 10, marginTop: 6 }}>
                    <span style={{ color: "#fff", fontWeight: 700 }}>Total</span>
                    <span style={{ color: "#b8982a", fontWeight: 700, fontSize: 18 }}>${selectedTotal.toFixed(2)}</span>
                  </div>
                  <button
                    onClick={handleProceed}
                    style={{
                      marginTop: 14, width: "100%", padding: "12px 0",
                      background: "#b8982a", color: "#000", fontWeight: 700,
                      fontSize: 15, border: "none", borderRadius: 8, cursor: "pointer",
                    }}
                  >
                    Continue to Book
                  </button>
                </div>
              )}

              <button
                onClick={() => { setCheckIn(null); setCheckOut(null); }}
                style={{ marginTop: 8, width: "100%", padding: "8px 0", background: "none", color: "#888", fontSize: 12, border: "none", cursor: "pointer" }}
              >
                Clear selection
              </button>
            </div>
          )}
        </>
      )}

      {bookingStep === "details" && (
        <div style={{ background: "rgba(255,255,255,0.04)", border: "1px solid rgba(184,152,42,0.3)", borderRadius: 12, padding: 24 }}>
          <h3 style={{ color: "#b8982a", marginBottom: 4 }}>Your Details</h3>
          <p style={{ color: "#888", fontSize: 13, marginBottom: 20 }}>
            {checkIn} → {checkOut} · {selectedNights} nights · ${selectedTotal.toLocaleString()}
          </p>

          <label style={{ color: "#aaa", fontSize: 12, display: "block", marginBottom: 4 }}>Full Name</label>
          <input
            value={guestName}
            onChange={(e) => setGuestName(e.target.value)}
            placeholder="Your name"
            style={{ width: "100%", padding: "10px 14px", background: "#1a1a1a", border: "1px solid #333", borderRadius: 8, color: "#fff", fontSize: 14, marginBottom: 14, boxSizing: "border-box" }}
          />

          <label style={{ color: "#aaa", fontSize: 12, display: "block", marginBottom: 4 }}>Email Address</label>
          <input
            type="email"
            value={guestEmail}
            onChange={(e) => setGuestEmail(e.target.value)}
            placeholder="your@email.com"
            style={{ width: "100%", padding: "10px 14px", background: "#1a1a1a", border: "1px solid #333", borderRadius: 8, color: "#fff", fontSize: 14, marginBottom: 14, boxSizing: "border-box" }}
          />

          {bookingError && <p style={{ color: "#ff6060", fontSize: 13, marginBottom: 12 }}>{bookingError}</p>}

          <button
            onClick={handleSubmit}
            style={{ width: "100%", padding: "13px 0", background: "#b8982a", color: "#000", fontWeight: 700, fontSize: 15, border: "none", borderRadius: 8, cursor: "pointer", marginBottom: 10 }}
          >
            Proceed to Payment
          </button>
          <button
            onClick={() => setBookingStep("calendar")}
            style={{ width: "100%", padding: "8px 0", background: "none", color: "#888", fontSize: 12, border: "none", cursor: "pointer" }}
          >
            ← Back to calendar
          </button>
        </div>
      )}

      {bookingStep === "processing" && (
        <div style={{ textAlign: "center", padding: 40 }}>
          <div style={{ color: "#b8982a", fontSize: 16, marginBottom: 8 }}>Redirecting to secure checkout…</div>
          <div style={{ color: "#888", fontSize: 13 }}>Please do not close this page.</div>
        </div>
      )}
    </div>
  );
}
