"use client";

import { useEffect, useState } from "react";
import reservation from "@/config/reservation";
import type { TabKey } from "@/components/TabNav";

const GOLD = "#C9A84C";
const GOLD_LIGHT = "#E8C97A";

interface GuestData {
  guestName?: string;
  checkIn?: string;
  checkOut?: string;
  nights?: number;
}

function greeting() {
  const h = new Date().getHours();
  if (h < 12) return { text: "Good Morning",   emoji: "☀️" };
  if (h < 17) return { text: "Good Afternoon",  emoji: "🌤️" };
  return           { text: "Good Evening",     emoji: "🌙" };
}

function greetingSubtitle(name: string): string {
  const h = new Date().getHours();
  if (h < 12) return `Welcome to Casanova ATL, ${name}. The pool is sparkling, the home has been prepared, and today is entirely yours to enjoy.`;
  if (h < 17) return `Whether you're relaxing by the pool, exploring Atlanta, or creating in the studio — everything has been prepared for you, ${name}.`;
  return `As the sun sets, the pool lights come alive, the theater is ready, and Casanova ATL becomes something truly special. Enjoy your evening, ${name}.`;
}

// "Nova Moments" — time-aware, config-driven nudges
// Later: these will come from AI Concierge. For now, driven by time + stay data.
function getNovaMoment(nights: number, checkOut: string): { icon: string; text: string } | null {
  const h = new Date().getHours();
  if (nights === 1 && checkOut) return { icon: "🌅", text: `Your stay ends tomorrow. Here's a reminder to check our checkout guide before ${checkOut}.` };
  if (h >= 19 && h < 23) return { icon: "🌙", text: "Quiet hours begin at 10 PM. The pool and outdoor areas will be winding down soon." };
  if (h >= 7 && h < 10) return { icon: "☀️", text: "Good morning. The pool is available all day — towels are in the outdoor storage cabinet." };
  if (h >= 14 && h < 17) return { icon: "🎬", text: "Afternoon is a great time for a movie. Your private theater is ready — flip the light switches inside to start." };
  return null;
}

const STATUS_CARDS = [
  { icon: "🏊", label: "Pool",          status: "Open",      cls: "ready"     },
  { icon: "🎬", label: "Movie Theater", status: "Ready",     cls: "ready"     },
  { icon: "📶", label: "Wi-Fi",         status: "Connected", cls: "ready"     },
  { icon: "🔐", label: "Door Code",     status: "Available", cls: "available" },
  { icon: "🎵", label: "Studio",        status: "Available", cls: "available" },
  { icon: "🎮", label: "Game Room",     status: "Open",      cls: "ready"     },
];

const QUICK_LINKS: { icon: string; label: string; tab: TabKey }[] = [
  { icon: "🏠", label: "Stay",          tab: "stay"          },
  { icon: "🎭", label: "Entertainment", tab: "entertainment" },
  { icon: "📍", label: "Explore",       tab: "explore"       },
  { icon: "🍽️", label: "Dining",       tab: "dining"        },
  { icon: "💬", label: "Support",       tab: "support"       },
];

function D(delay: string): React.CSSProperties {
  return { "--delay": delay } as React.CSSProperties;
}

export default function ConciergeHome({ onEnterGuide }: { onEnterGuide: (tab?: TabKey) => void }) {
  const [guest, setGuest] = useState<GuestData>({});
  const [visible, setVisible] = useState(false);
  const [momentDismissed, setMomentDismissed] = useState(false);

  useEffect(() => {
    try {
      const raw = localStorage.getItem("novaStay_guestInfo");
      if (raw) setGuest(JSON.parse(raw));
    } catch { /* ignore */ }
    const t = setTimeout(() => setVisible(true), 60);
    return () => clearTimeout(t);
  }, []);

  const name    = guest.guestName || reservation.guestName;
  const checkIn = guest.checkIn   || reservation.checkIn;
  const checkOut= guest.checkOut  || reservation.checkOut;
  const nights  = guest.nights    ?? reservation.nights;

  const moment = getNovaMoment(nights, checkOut ?? "");

  return (
    <div style={{ minHeight: "100vh", background: "#0A0A0A", overflowX: "hidden", opacity: visible ? 1 : 0, transition: "opacity 0.5s ease" }}>

      {/* Top bar */}
      <div style={{ position: "sticky", top: 0, zIndex: 50, background: "rgba(10,10,10,0.92)", backdropFilter: "blur(12px)", borderBottom: "1px solid #1a1a1a", padding: "14px 20px", display: "flex", alignItems: "center", justifyContent: "space-between" }}>
        <span style={{ color: GOLD, fontSize: 11, fontWeight: 700, letterSpacing: "0.25em", textTransform: "uppercase" }}>Nova Stay</span>
        <span className="status-pill ready" style={{ fontSize: 10 }}><span>●</span> Live</span>
      </div>

      <div style={{ maxWidth: 480, margin: "0 auto", padding: "0 20px 100px" }}>

        {/* Greeting */}
        <div className="anim-fade-up anim-delay" style={{ ...D("0.05s"), padding: "36px 0 28px" }}>
          <p style={{ color: "#4B5563", fontSize: 11, letterSpacing: "0.2em", textTransform: "uppercase", marginBottom: 12 }}>
            {checkIn} – {checkOut}{nights ? ` · ${nights} night${nights !== 1 ? "s" : ""}` : ""}
          </p>
          <h1 className="serif" style={{ fontSize: "clamp(1.8rem, 6vw, 2.4rem)", fontWeight: 700, color: "#fff", lineHeight: 1.2, marginBottom: 16 }}>
            {greeting().text}, {name} {greeting().emoji}
          </h1>
          <p style={{ color: "#9CA3AF", fontSize: 15, lineHeight: 1.75 }}>
            {greetingSubtitle(name)}
          </p>
        </div>

        {/* Nova Moments strip */}
        {moment && !momentDismissed && (
          <div className="anim-fade-up anim-delay" style={D("0.10s")}>
            <div style={{ background: `rgba(201,168,76,0.06)`, border: `1px solid ${GOLD}33`, borderRadius: 14, padding: "14px 16px", marginBottom: 22, display: "flex", alignItems: "flex-start", gap: 12 }}>
              <span style={{ fontSize: 20, lineHeight: 1, marginTop: 1 }}>{moment.icon}</span>
              <p style={{ color: "#D1B96B", fontSize: 13, lineHeight: 1.6, flex: 1 }}>{moment.text}</p>
              <button
                onClick={() => setMomentDismissed(true)}
                style={{ background: "transparent", border: "none", color: "#4B5563", fontSize: 16, cursor: "pointer", lineHeight: 1, padding: 0, flexShrink: 0 }}
              >
                ×
              </button>
            </div>
          </div>
        )}

        {/* Weather + Today hero */}
        <div className="anim-fade-up anim-delay" style={D("0.14s")}>
          <div style={{ background: "#111", border: "1px solid #1e1e1e", borderRadius: 16, padding: "20px", marginBottom: 28 }}>
            <p style={{ color: "#4B5563", fontSize: 10, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 10 }}>Today in Atlanta</p>
            <div style={{ display: "flex", alignItems: "flex-end", justifyContent: "space-between" }}>
              <div>
                <p style={{ color: "#fff", fontSize: 42, fontWeight: 200, letterSpacing: "-0.03em", lineHeight: 1 }}>74°</p>
                <p style={{ color: "#6B7280", fontSize: 12, marginTop: 4 }}>Partly Cloudy</p>
              </div>
              <div style={{ textAlign: "right" }}>
                <p style={{ color: "#4B5563", fontSize: 10, letterSpacing: "0.1em", textTransform: "uppercase", marginBottom: 4 }}>Pool lights</p>
                <p style={{ color: "#9CA3AF", fontSize: 13 }}>On at sunset</p>
              </div>
            </div>
          </div>
        </div>

        {/* Property status grid */}
        <div className="anim-fade-up anim-delay" style={D("0.20s")}>
          <p style={{ color: "#4B5563", fontSize: 10, letterSpacing: "0.2em", textTransform: "uppercase", marginBottom: 14 }}>Property Status</p>
          <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 10, marginBottom: 28 }}>
            {STATUS_CARDS.map((c) => (
              <div key={c.label} className="card-hover" style={{ background: "#111", border: "1px solid #1e1e1e", borderRadius: 14, padding: "16px 16px 14px" }}>
                <span style={{ fontSize: 22, display: "block", marginBottom: 8 }}>{c.icon}</span>
                <p style={{ color: "#fff", fontSize: 13, fontWeight: 600, marginBottom: 6 }}>{c.label}</p>
                <span className={`status-pill ${c.cls}`}>{c.status}</span>
              </div>
            ))}
          </div>
        </div>

        {/* Quick nav */}
        <div className="anim-fade-up anim-delay" style={D("0.28s")}>
          <p style={{ color: "#4B5563", fontSize: 10, letterSpacing: "0.2em", textTransform: "uppercase", marginBottom: 14 }}>Quick Access</p>
          <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 10, marginBottom: 28 }}>
            {QUICK_LINKS.map((q) => (
              <button key={q.label} className="btn-press card-hover" onClick={() => onEnterGuide(q.tab)} style={{ background: "#111", border: "1px solid #1e1e1e", borderRadius: 14, padding: "16px", display: "flex", alignItems: "center", gap: 10, cursor: "pointer", textAlign: "left" }}>
                <span style={{ fontSize: 20 }}>{q.icon}</span>
                <span style={{ color: "#9CA3AF", fontSize: 13, fontWeight: 500 }}>{q.label}</span>
              </button>
            ))}
          </div>
        </div>

        {/* Nova Concierge CTA */}
        <div className="anim-fade-up anim-delay" style={D("0.36s")}>
          <div style={{ background: "rgba(201,168,76,0.04)", border: `1px solid ${GOLD}22`, borderRadius: 16, padding: "24px 20px", marginBottom: 14, textAlign: "center" }}>
            <p style={{ color: GOLD, fontSize: 11, letterSpacing: "0.2em", textTransform: "uppercase", marginBottom: 8 }}>✦ Nova Concierge</p>
            <p style={{ color: "#9CA3AF", fontSize: 14, lineHeight: 1.65, marginBottom: 16 }}>
              Need restaurant recommendations, theater instructions, or anything else? Your AI concierge is coming soon.
            </p>
            <button
              className="btn-press"
              disabled
              style={{ background: `linear-gradient(135deg, ${GOLD}, ${GOLD_LIGHT})`, color: "#0A0A0A", border: "none", borderRadius: 10, padding: "13px 28px", fontSize: 12, fontWeight: 700, letterSpacing: "0.15em", textTransform: "uppercase", cursor: "default", opacity: 0.5, width: "100%" }}
            >
              Ask Nova — Coming Soon
            </button>
          </div>
        </div>

        {/* Guest guide link */}
        <div className="anim-fade-up anim-delay" style={D("0.42s")}>
          <button
            className="btn-press"
            onClick={() => onEnterGuide()}
            style={{ width: "100%", background: "transparent", border: "1px solid #222", borderRadius: 12, padding: "14px", color: "#6B7280", fontSize: 13, cursor: "pointer", letterSpacing: "0.05em" }}
          >
            View Guest Guide →
          </button>
        </div>

      </div>
    </div>
  );
}
