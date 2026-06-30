"use client";

import { useEffect, useState } from "react";
import {
  Sun,
  CloudSun,
  Moon,
  Waves,
  Clapperboard,
  Music,
  Home,
  Compass,
  UtensilsCrossed,
  Sparkles,
  Coffee,
  Sunset,
  KeyRound,
  Heart,
  type LucideIcon,
} from "lucide-react";
import reservation from "@/config/reservation";
import property from "@/config/property";
import type { TabKey } from "@/components/TabNav";
import NovaJourney from "@/components/NovaJourney";
import { getJourneyStage, type JourneyStageKey } from "@/lib/novaJourney";
import theme from "@/config/theme";

const GOLD = theme.gold;
const GOLD_LIGHT = theme.goldLight;

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

const GREETINGS: Record<TOD, { text: string; Icon: LucideIcon; subtitle: (name: string) => string; overlay: string; cta: string }> = {
  morning: {
    text: "Good Morning",
    Icon: Sun,
    subtitle: (n) => `Welcome to Casanova ATL, ${n}. The pool is sparkling, the home has been prepared, and today is entirely yours to enjoy.`,
    overlay: "linear-gradient(to bottom, rgba(0,0,0,0.55) 0%, rgba(10,5,0,0.25) 40%, rgba(0,0,0,0.72) 100%)",
    cta: "Begin Your Morning",
  },
  afternoon: {
    text: "Good Afternoon",
    Icon: CloudSun,
    subtitle: (n) => `Whether you're relaxing by the pool, exploring Atlanta, or creating in the studio — everything has been prepared for you, ${n}.`,
    overlay: "linear-gradient(to bottom, rgba(0,0,0,0.5) 0%, rgba(0,0,0,0.15) 40%, rgba(0,0,0,0.75) 100%)",
    cta: "Continue Your Day",
  },
  evening: {
    text: "Good Evening",
    Icon: Moon,
    subtitle: (n) => `As the sun sets, the pool lights come alive, the theater is ready, and Casanova ATL becomes something truly special. Enjoy your evening, ${n}.`,
    overlay: "linear-gradient(to bottom, rgba(0,0,0,0.65) 0%, rgba(5,5,20,0.30) 40%, rgba(0,0,0,0.85) 100%)",
    cta: "Begin Your Evening",
  },
  night: {
    text: "Good Night",
    Icon: Moon,
    subtitle: (n) => `The lights are low, the theater is warm, and the city is alive outside. Rest well, ${n}.`,
    overlay: "linear-gradient(to bottom, rgba(0,0,0,0.7) 0%, rgba(5,5,20,0.4) 40%, rgba(0,0,0,0.9) 100%)",
    cta: "Explore the Night",
  },
};

/* ── StayByNova Moments ───────────────────────────────────────────────────── */

function getMoment(nights: number, stage: JourneyStageKey): { Icon: LucideIcon; text: string } | null {
  if (stage === "checkedIn")
    return { Icon: KeyRound, text: "You're checked in. Your door code and Wi-Fi details are ready in the Stay tab whenever you need them." };
  if (stage === "checkout")
    return { Icon: Sunset, text: "Checkout is tomorrow. Take a look at the checkout guide tonight so the morning goes smoothly." };
  if (stage === "review")
    return { Icon: Heart, text: "We hope you loved your stay. Leaving a review helps us keep delivering this experience." };

  const h = new Date().getHours();
  const m = new Date().getMinutes();
  const timeNum = h * 60 + m;

  if (timeNum >= 7 * 60 && timeNum < 9 * 60)
    return { Icon: Coffee, text: "Good morning. Fresh coffee is waiting, and the pool opens at 8 AM." };
  if (timeNum >= 14 * 60 && timeNum < 16 * 60)
    return { Icon: Clapperboard, text: "Perfect afternoon for a movie. Flip the light switches inside the theater to get started." };
  if (timeNum >= 19 * 60 && timeNum < 20 * 60)
    return { Icon: Sunset, text: "The pool lights just came on. It's a beautiful evening for a swim." };
  if (timeNum >= 21 * 60 && timeNum < 22 * 60)
    return { Icon: Moon, text: "Quiet hours begin at 10 PM. The indoor theater and studio are available all night." };
  if (nights <= 1)
    return { Icon: Sunset, text: "Your stay ends tomorrow. Check out our checkout guide so the morning goes smoothly." };

  return null;
}

/* ── Quick actions — mirrors the main tab nav ────────────────────────────── */

const ACTIONS: { Icon: LucideIcon; label: string; tab: TabKey }[] = [
  { Icon: Home,            label: "Stay",        tab: "stay"          },
  { Icon: Clapperboard,    label: "Experience",  tab: "entertainment" },
  { Icon: Compass,         label: "Explore",     tab: "explore"       },
  { Icon: UtensilsCrossed, label: "Dining",      tab: "dining"        },
  { Icon: Sparkles,        label: "Support",     tab: "support"       },
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

  const tod      = timeOfDay();
  const g        = GREETINGS[tod];
  const stage    = getJourneyStage(checkIn, checkOut);
  const moment   = getMoment(nights, stage);
  const isPoolOk = tod === "evening" || tod === "afternoon";

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
          <span className="status-pill ready pill-pop" style={{ fontSize: 10 }}><span>●</span> Live</span>
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
            style={{ fontSize: "clamp(2rem, 7vw, 2.8rem)", fontWeight: 700, color: "#fff", lineHeight: 1.15, marginBottom: 10, textShadow: "0 2px 20px rgba(0,0,0,0.5)", display: "flex", alignItems: "center", gap: 12 }}
          >
            {g.text}, {name}
            <g.Icon size={30} strokeWidth={1.6} className="icon-float" style={{ color: GOLD }} />
          </h1>

          {/* Subtitle */}
          <p style={{ color: "rgba(255,255,255,0.7)", fontSize: 14, lineHeight: 1.65, marginBottom: 24, maxWidth: 340 }}>
            {g.subtitle(name)}
          </p>

          {/* CTA */}
          <button
            className="btn-press lux-btn-gold"
            onClick={() => onEnterGuide("stay")}
            style={{ padding: "13px 28px", fontSize: 12 }}
          >
            {g.cta} →
          </button>
        </div>
      </div>

      {/* ── CONTENT BELOW HERO ────────────────────────────────────────── */}
      <div style={{ maxWidth: 480, margin: "0 auto", padding: "0 20px 100px" }}>

        {/* Nova Journey — guest stay-stage timeline */}
        <div className="anim-fade-up" style={{ margin: "20px 0 4px" }}>
          <NovaJourney current={stage} onSelectTab={onEnterGuide} />
        </div>

        {/* StayByNova Moment strip */}
        {moment && !momentDismissed && (
          <div className="anim-fade-up" style={{ margin: "var(--space-4) 0 0" }}>
            <div className="lux-card" style={{ padding: "var(--space-3) var(--space-3)", display: "flex", alignItems: "flex-start", gap: 12, borderColor: `${GOLD}33` }}>
              <moment.Icon size={20} strokeWidth={1.7} style={{ color: GOLD, flexShrink: 0, marginTop: 1 }} />
              <p style={{ color: "#D1B96B", fontSize: 13, lineHeight: 1.6, flex: 1 }}>{moment.text}</p>
              <button onClick={() => setMomentDismissed(true)} style={{ background: "transparent", border: "none", color: "#4B5563", fontSize: 18, cursor: "pointer", lineHeight: 1, padding: 0, flexShrink: 0 }}>×</button>
            </div>
          </div>
        )}

        {/* Today Card — immersive, replaces the old 2×2 status grid */}
        <div className="anim-fade-up anim-delay" style={{ "--delay": "0.08s" } as React.CSSProperties}>
          <p style={{ color: "#4B5563", fontSize: 10, letterSpacing: "0.2em", textTransform: "uppercase", margin: "28px 0 12px" }}>Tonight at Casanova ATL</p>

          <div
            className="lux-card-glow"
            style={{
              padding: "var(--space-4)",
              marginBottom: 28,
              background: "linear-gradient(160deg, #14110a 0%, #111 60%)",
            }}
          >
            <div style={{ display: "flex", alignItems: "flex-start", justifyContent: "space-between", marginBottom: 18 }}>
              <div>
                <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.1em", textTransform: "uppercase", marginBottom: 4 }}>Tonight</p>
                <p className="serif" style={{ color: "#fff", fontSize: 40, fontWeight: 700, lineHeight: 1 }}>74°</p>
              </div>
              <Sun size={34} strokeWidth={1.5} className="icon-float" style={{ color: GOLD, marginTop: 4 }} />
            </div>

            <p style={{ color: "rgba(255,255,255,0.75)", fontSize: 14, lineHeight: 1.6, marginBottom: 18 }}>
              {isPoolOk ? "Perfect evening for the pool." : "A cozy night in is calling."} Movie theater available.
            </p>

            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 10 }}>
              <button
                className="btn-press card-hover"
                onClick={() => onEnterGuide("entertainment")}
                style={{ background: "rgba(255,255,255,0.03)", border: "1px solid #1e1e1e", borderRadius: "var(--radius-sm)", padding: "12px 14px", display: "flex", alignItems: "center", gap: 8, cursor: "pointer" }}
              >
                <Waves size={17} strokeWidth={1.7} style={{ color: GOLD }} />
                <span style={{ color: "#fff", fontSize: 12, fontWeight: 600 }}>Pool Open</span>
              </button>
              <button
                className="btn-press card-hover"
                onClick={() => onEnterGuide("entertainment")}
                style={{ background: "rgba(255,255,255,0.03)", border: "1px solid #1e1e1e", borderRadius: "var(--radius-sm)", padding: "12px 14px", display: "flex", alignItems: "center", gap: 8, cursor: "pointer" }}
              >
                <Music size={17} strokeWidth={1.7} style={{ color: GOLD }} />
                <span style={{ color: "#fff", fontSize: 12, fontWeight: 600 }}>Studio Available</span>
              </button>
            </div>
          </div>
        </div>

        {/* Quick actions */}
        <div className="anim-fade-up anim-delay" style={{ "--delay": "0.15s" } as React.CSSProperties}>
          <p style={{ color: "#4B5563", fontSize: 10, letterSpacing: "0.2em", textTransform: "uppercase", marginBottom: 12 }}>Quick Actions</p>
          <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 10, marginBottom: 28 }}>
            {ACTIONS.map((a, i) => (
              <button
                key={a.label}
                className="btn-press card-hover lux-card"
                onClick={() => onEnterGuide(a.tab)}
                style={{
                  padding: "20px 16px",
                  display: "flex",
                  alignItems: "center",
                  gap: 12,
                  cursor: "pointer",
                  textAlign: "left",
                  gridColumn: i === ACTIONS.length - 1 && ACTIONS.length % 2 === 1 ? "1 / -1" : undefined,
                }}
              >
                <a.Icon size={22} strokeWidth={1.6} style={{ color: GOLD }} />
                <span style={{ color: "#9CA3AF", fontSize: 13, fontWeight: 600 }}>{a.label}</span>
              </button>
            ))}
          </div>
        </div>

        {/* AI Concierge */}
        <div className="anim-fade-up anim-delay" style={{ "--delay": "0.22s" } as React.CSSProperties}>
          <div className="lux-card-glow" style={{ padding: "var(--space-4) var(--space-3)", marginBottom: 14, textAlign: "center" }}>
            <Sparkles size={24} strokeWidth={1.5} className="icon-float" style={{ color: GOLD, marginBottom: 10 }} />
            <p className="serif" style={{ color: "#fff", fontSize: 19, fontWeight: 700, marginBottom: 8 }}>Need anything?</p>
            <p style={{ color: "#9CA3AF", fontSize: 14, lineHeight: 1.65, marginBottom: 18 }}>
              Nova can recommend restaurants, book experiences, answer questions, or help during your stay.
            </p>
            <button
              className="btn-press lux-btn-gold"
              onClick={() => onEnterGuide("support")}
              disabled
              style={{ padding: "13px 28px", fontSize: 12, width: "100%", opacity: 0.5, cursor: "default" }}
            >
              Start Conversation — Coming Soon
            </button>
          </div>
        </div>

        {/* Full guide subtle link */}
        <div className="anim-fade-up anim-delay" style={{ "--delay": "0.28s" } as React.CSSProperties}>
          <button
            className="btn-press"
            onClick={() => onEnterGuide()}
            style={{ width: "100%", background: "transparent", border: "1px solid #1e1e1e", borderRadius: "var(--radius-sm)", padding: "14px", color: "#4B5563", fontSize: 12, cursor: "pointer", letterSpacing: "0.05em" }}
          >
            View Full Guest Guide →
          </button>
        </div>

      </div>
    </div>
  );
}
