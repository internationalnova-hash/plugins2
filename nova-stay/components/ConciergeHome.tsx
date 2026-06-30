"use client";

import { useEffect, useState } from "react";
import reservation from "@/config/reservation";
import property from "@/config/property";
import type { TabKey } from "@/components/TabNav";

const GOLD = "#C9A84C";
const GOLD_LIGHT = "#E8C97A";

interface GuestData {
  guestName?: string;
  checkIn?: string;
  checkOut?: string;
  nights?: number;
}

/* ── Time helpers ─────────────────────────────────────────────────────────── */

type TOD = "morning" | "afternoon" | "evening" | "night";

function timeOfDay(): TOD {
  const h = new Date().getHours();
  if (h >= 6  && h < 12) return "morning";
  if (h >= 12 && h < 17) return "afternoon";
  if (h >= 17 && h < 22) return "evening";
  return "night";
}

const GREETINGS: Record<TOD, { text: string; emoji: string; subtitle: (name: string) => string; overlay: string; cta: string }> = {
  morning: {
    text: "Good Morning",
    emoji: "☀️",
    subtitle: (n) => `Welcome to Casanova ATL, ${n}. The pool is sparkling, the home has been prepared, and today is entirely yours to enjoy.`,
    overlay: "linear-gradient(to bottom, rgba(0,0,0,0.55) 0%, rgba(10,5,0,0.25) 40%, rgba(0,0,0,0.72) 100%)",
    cta: "Begin Your Morning",
  },
  afternoon: {
    text: "Good Afternoon",
    emoji: "🌤️",
    subtitle: (n) => `Whether you're relaxing by the pool, exploring Atlanta, or creating in the studio — everything has been prepared for you, ${n}.`,
    overlay: "linear-gradient(to bottom, rgba(0,0,0,0.5) 0%, rgba(0,0,0,0.15) 40%, rgba(0,0,0,0.75) 100%)",
    cta: "Continue Your Day",
  },
  evening: {
    text: "Good Evening",
    emoji: "🌙",
    subtitle: (n) => `As the sun sets, the pool lights come alive, the theater is ready, and Casanova ATL becomes something truly special. Enjoy your evening, ${n}.`,
    overlay: "linear-gradient(to bottom, rgba(0,0,0,0.65) 0%, rgba(5,5,20,0.30) 40%, rgba(0,0,0,0.85) 100%)",
    cta: "Begin Your Evening",
  },
  night: {
    text: "Good Night",
    emoji: "🌙",
    subtitle: (n) => `The lights are low, the theater is warm, and the city is alive outside. Rest well, ${n}.`,
    overlay: "linear-gradient(to bottom, rgba(0,0,0,0.7) 0%, rgba(5,5,20,0.4) 40%, rgba(0,0,0,0.9) 100%)",
    cta: "Explore the Night",
  },
};

/* ── Nova Moments ─────────────────────────────────────────────────────────── */

function getNovaMoment(nights: number): { icon: string; text: string } | null {
  const h = new Date().getHours();
  const m = new Date().getMinutes();
  const timeNum = h * 60 + m;

  if (timeNum >= 7 * 60 && timeNum < 9 * 60)
    return { icon: "☕", text: "Good morning. Fresh coffee is waiting, and the pool opens at 8 AM." };
  if (timeNum >= 14 * 60 && timeNum < 16 * 60)
    return { icon: "🎬", text: "Perfect afternoon for a movie. Flip the light switches inside the theater to get started." };
  if (timeNum >= 19 * 60 && timeNum < 20 * 60)
    return { icon: "🌅", text: "The pool lights just came on. It's a beautiful evening for a swim." };
  if (timeNum >= 21 * 60 && timeNum < 22 * 60)
    return { icon: "🌙", text: "Quiet hours begin at 10 PM. The indoor theater and studio are available all night." };
  if (nights <= 1)
    return { icon: "🌅", text: "Your stay ends tomorrow. Check out our checkout guide so the morning goes smoothly." };

  return null;
}

/* ── Status chips ─────────────────────────────────────────────────────────── */

const STATUS: { icon: string; label: string; value: string; ok: boolean; tab: TabKey }[] = [
  { icon: "🌡️", label: "Weather", value: "74°",       ok: true,  tab: "stay"          },
  { icon: "🏊",  label: "Pool",    value: "Open",      ok: true,  tab: "entertainment" },
  { icon: "🎬",  label: "Theater", value: "Ready",     ok: true,  tab: "entertainment" },
  { icon: "🎵",  label: "Studio",  value: "Available", ok: false, tab: "entertainment" },
];

/* ── Quick actions ────────────────────────────────────────────────────────── */

const ACTIONS: { icon: string; label: string; tab: TabKey }[] = [
  { icon: "🏡", label: "Stay",       tab: "stay"          },
  { icon: "🎬", label: "Experience", tab: "entertainment" },
  { icon: "🍽️", label: "Dining",    tab: "dining"        },
  { icon: "📍", label: "Explore",    tab: "explore"       },
];

/* ── Component ───────────────────────────────────────────────────────────── */

export default function ConciergeHome({ onEnterGuide }: { onEnterGuide: (tab?: TabKey) => void }) {
  const [guest, setGuest] = useState<GuestData>({});
  const [visible, setVisible] = useState(false);
  const [momentDismissed, setMomentDismissed] = useState(false);

  useEffect(() => {
    try {
      const raw = localStorage.getItem("stayByNova_guestInfo");
      if (raw) setGuest(JSON.parse(raw));
    } catch { /* ignore */ }
    const t = setTimeout(() => setVisible(true), 60);
    return () => clearTimeout(t);
  }, []);

  const name    = guest.guestName || reservation.guestName;
  const checkIn = guest.checkIn   || reservation.checkIn;
  const checkOut= guest.checkOut  || reservation.checkOut;
  const nights  = guest.nights    ?? reservation.nights;

  const tod     = timeOfDay();
  const g       = GREETINGS[tod];
  const moment  = getNovaMoment(nights);

  return (
    <div style={{ minHeight: "100vh", background: "#0A0A0A", overflowX: "hidden", opacity: visible ? 1 : 0, transition: "opacity 0.6s ease" }}>

      {/* ── HERO ───────────────────────────────────────────────────────── */}
      <div style={{ position: "relative", width: "100%", height: "62vh", minHeight: 420, overflow: "hidden" }}>

        {/* Fallback gradient — always visible */}
        <div style={{ position: "absolute", inset: 0, background: "linear-gradient(160deg,#1a0f04,#0d0d12 55%,#060a16)", zIndex: 0 }} />

        {/* Hero image */}
        <div
          style={{
            position: "absolute",
            inset: "-8% 0",
            backgroundImage: `url('${property.heroImage}')`,
            backgroundSize: "cover",
            backgroundPosition: "center 65%",
            zIndex: 1,
          }}
        />

        {/* Time-based color overlay */}
        <div style={{ position: "absolute", inset: 0, background: g.overlay, zIndex: 2 }} />

        {/* StayByNova wordmark — top */}
        <div style={{ position: "absolute", top: 0, left: 0, right: 0, zIndex: 10, display: "flex", alignItems: "center", justifyContent: "space-between", padding: "20px 22px" }}>
          <span style={{ color: GOLD, fontSize: 11, fontWeight: 700, letterSpacing: "0.25em", textTransform: "uppercase" }}>StayByNova</span>
          <span className="status-pill ready" style={{ fontSize: 10 }}><span>●</span> Live</span>
        </div>

        {/* Hero content — bottom of image */}
        <div style={{ position: "absolute", bottom: 0, left: 0, right: 0, zIndex: 10, padding: "0 24px 32px" }}>
          {/* Dates */}
          <p style={{ color: "rgba(255,255,255,0.45)", fontSize: 11, letterSpacing: "0.18em", textTransform: "uppercase", marginBottom: 10 }}>
            {checkIn} – {checkOut}{nights ? ` · ${nights} night${nights !== 1 ? "s" : ""}` : ""}
          </p>

          {/* Greeting */}
          <h1
            className="serif anim-fade-up"
            style={{ fontSize: "clamp(2rem, 7vw, 2.8rem)", fontWeight: 700, color: "#fff", lineHeight: 1.15, marginBottom: 10, textShadow: "0 2px 20px rgba(0,0,0,0.5)" }}
          >
            {g.text}, {name} {g.emoji}
          </h1>

          {/* Subtitle */}
          <p style={{ color: "rgba(255,255,255,0.7)", fontSize: 14, lineHeight: 1.65, marginBottom: 24, maxWidth: 340 }}>
            {g.subtitle(name)}
          </p>

          {/* CTA */}
          <button
            className="btn-press"
            onClick={() => onEnterGuide("stay")}
            style={{
              background: `linear-gradient(135deg, ${GOLD}, ${GOLD_LIGHT})`,
              color: "#0A0A0A",
              border: "none",
              borderRadius: 10,
              padding: "13px 28px",
              fontSize: 12,
              fontWeight: 700,
              letterSpacing: "0.15em",
              textTransform: "uppercase",
              cursor: "pointer",
            }}
          >
            {g.cta} →
          </button>
        </div>
      </div>

      {/* ── CONTENT BELOW HERO ────────────────────────────────────────── */}
      <div style={{ maxWidth: 480, margin: "0 auto", padding: "0 20px 100px" }}>

        {/* Nova Moment strip */}
        {moment && !momentDismissed && (
          <div className="anim-fade-up" style={{ margin: "20px 0 0" }}>
            <div style={{ background: `rgba(201,168,76,0.06)`, border: `1px solid ${GOLD}33`, borderRadius: 14, padding: "14px 16px", display: "flex", alignItems: "flex-start", gap: 12 }}>
              <span style={{ fontSize: 20, lineHeight: 1, marginTop: 1 }}>{moment.icon}</span>
              <p style={{ color: "#D1B96B", fontSize: 13, lineHeight: 1.6, flex: 1 }}>{moment.text}</p>
              <button onClick={() => setMomentDismissed(true)} style={{ background: "transparent", border: "none", color: "#4B5563", fontSize: 18, cursor: "pointer", lineHeight: 1, padding: 0, flexShrink: 0 }}>×</button>
            </div>
          </div>
        )}

        {/* Today's status — horizontal chips */}
        <div className="anim-fade-up anim-delay" style={{ "--delay": "0.08s" } as React.CSSProperties}>
          <p style={{ color: "#4B5563", fontSize: 10, letterSpacing: "0.2em", textTransform: "uppercase", margin: "28px 0 12px" }}>Today at Casanova ATL</p>
          <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 10, marginBottom: 28 }}>
            {STATUS.map((s) => (
              <button
                key={s.label}
                className="btn-press card-hover"
                onClick={() => onEnterGuide(s.tab)}
                style={{ background: "#111", border: "1px solid #1e1e1e", borderRadius: 14, padding: "16px", cursor: "pointer", textAlign: "left" }}
              >
                <span style={{ fontSize: 20, display: "block", marginBottom: 8 }}>{s.icon}</span>
                <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.12em", textTransform: "uppercase", marginBottom: 4 }}>{s.label}</p>
                <p style={{ color: s.ok ? "#fff" : GOLD, fontSize: 16, fontWeight: 700 }}>{s.value}</p>
              </button>
            ))}
          </div>
        </div>

        {/* Quick actions — 2×2 */}
        <div className="anim-fade-up anim-delay" style={{ "--delay": "0.15s" } as React.CSSProperties}>
          <p style={{ color: "#4B5563", fontSize: 10, letterSpacing: "0.2em", textTransform: "uppercase", marginBottom: 12 }}>Quick Access</p>
          <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 10, marginBottom: 28 }}>
            {ACTIONS.map((a) => (
              <button
                key={a.label}
                className="btn-press card-hover"
                onClick={() => onEnterGuide(a.tab)}
                style={{ background: "#111", border: "1px solid #1e1e1e", borderRadius: 14, padding: "20px 16px", display: "flex", alignItems: "center", gap: 12, cursor: "pointer", textAlign: "left" }}
              >
                <span style={{ fontSize: 22 }}>{a.icon}</span>
                <span style={{ color: "#9CA3AF", fontSize: 13, fontWeight: 600 }}>{a.label}</span>
              </button>
            ))}
          </div>
        </div>

        {/* Ask Nova CTA */}
        <div className="anim-fade-up anim-delay" style={{ "--delay": "0.22s" } as React.CSSProperties}>
          <div style={{ background: "rgba(201,168,76,0.04)", border: `1px solid ${GOLD}22`, borderRadius: 16, padding: "24px 20px", marginBottom: 14, textAlign: "center" }}>
            <p style={{ color: GOLD, fontSize: 11, letterSpacing: "0.2em", textTransform: "uppercase", marginBottom: 8 }}>✦ Ask Nova</p>
            <p style={{ color: "#9CA3AF", fontSize: 14, lineHeight: 1.65, marginBottom: 16 }}>
              Restaurant recommendations, theater instructions, local tips — your AI concierge is coming soon.
            </p>
            <button
              className="btn-press"
              onClick={() => onEnterGuide("support")}
              disabled
              style={{ background: `linear-gradient(135deg, ${GOLD}, ${GOLD_LIGHT})`, color: "#0A0A0A", border: "none", borderRadius: 10, padding: "13px 28px", fontSize: 12, fontWeight: 700, letterSpacing: "0.15em", textTransform: "uppercase", cursor: "default", opacity: 0.5, width: "100%" }}
            >
              Ask Nova — Coming Soon
            </button>
          </div>
        </div>

        {/* Full guide subtle link */}
        <div className="anim-fade-up anim-delay" style={{ "--delay": "0.28s" } as React.CSSProperties}>
          <button
            className="btn-press"
            onClick={() => onEnterGuide()}
            style={{ width: "100%", background: "transparent", border: "1px solid #1e1e1e", borderRadius: 12, padding: "14px", color: "#4B5563", fontSize: 12, cursor: "pointer", letterSpacing: "0.05em" }}
          >
            View Full Guest Guide →
          </button>
        </div>

      </div>
    </div>
  );
}
