"use client";

import { useEffect, useState } from "react";
import reservation from "@/config/reservation";

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

const STATUS_CARDS = [
  { icon: "🏊", label: "Pool",          status: "Open",      cls: "ready"     },
  { icon: "🎬", label: "Movie Theater", status: "Ready",     cls: "ready"     },
  { icon: "📶", label: "Wi-Fi",         status: "Connected", cls: "ready"     },
  { icon: "🔐", label: "Door Code",     status: "Available", cls: "available" },
  { icon: "🎵", label: "Studio",        status: "Available", cls: "available" },
  { icon: "🎮", label: "Game Room",     status: "Open",      cls: "ready"     },
];

const QUICK_LINKS = [
  { icon: "🏠", label: "Stay"          },
  { icon: "🎭", label: "Entertainment" },
  { icon: "📍", label: "Explore"       },
  { icon: "🛎️", label: "Support"      },
];

function D(delay: string): React.CSSProperties {
  return { "--delay": delay } as React.CSSProperties;
}

export default function ConciergeHome({ onEnterGuide }: { onEnterGuide: () => void }) {
  const [guest, setGuest] = useState<GuestData>({});
  const [visible, setVisible] = useState(false);

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

        {/* Weather strip */}
        <div className="anim-fade-up anim-delay" style={D("0.12s")}>
          <div style={{ background: "#111", border: "1px solid #1e1e1e", borderRadius: 16, padding: "18px 20px", display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: 28 }}>
            <div>
              <p style={{ color: "#4B5563", fontSize: 10, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 4 }}>Atlanta, GA</p>
              <p style={{ color: "#fff", fontSize: 28, fontWeight: 300, letterSpacing: "-0.02em" }}>
                74°
                <span style={{ color: "#4B5563", fontSize: 13, fontWeight: 400, marginLeft: 8 }}>Partly Cloudy</span>
              </p>
            </div>
            <div style={{ textAlign: "right" }}>
              <p style={{ color: "#4B5563", fontSize: 10, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 4 }}>Tonight</p>
              <p style={{ color: "#6B7280", fontSize: 13 }}>Pool lights at sunset</p>
            </div>
          </div>
        </div>

        {/* Property status grid */}
        <div className="anim-fade-up anim-delay" style={D("0.18s")}>
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
        <div className="anim-fade-up anim-delay" style={D("0.32s")}>
          <p style={{ color: "#4B5563", fontSize: 10, letterSpacing: "0.2em", textTransform: "uppercase", marginBottom: 14 }}>Quick Access</p>
          <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 10, marginBottom: 28 }}>
            {QUICK_LINKS.map((q) => (
              <button key={q.label} className="btn-press card-hover" onClick={onEnterGuide} style={{ background: "#111", border: "1px solid #1e1e1e", borderRadius: 14, padding: "16px", display: "flex", alignItems: "center", gap: 10, cursor: "pointer", textAlign: "left" }}>
                <span style={{ fontSize: 20 }}>{q.icon}</span>
                <span style={{ color: "#9CA3AF", fontSize: 13, fontWeight: 500 }}>{q.label}</span>
              </button>
            ))}
          </div>
        </div>

        {/* Nova Concierge CTA */}
        <div className="anim-fade-up anim-delay" style={D("0.42s")}>
          <div style={{ background: "rgba(201,168,76,0.04)", border: `1px solid ${GOLD}22`, borderRadius: 16, padding: "24px 20px", marginBottom: 14, textAlign: "center" }}>
            <p style={{ color: GOLD, fontSize: 11, letterSpacing: "0.2em", textTransform: "uppercase", marginBottom: 8 }}>✦ Nova Concierge</p>
            <p style={{ color: "#9CA3AF", fontSize: 14, lineHeight: 1.65, marginBottom: 16 }}>
              Need restaurant recommendations, theater instructions, or anything else? Your concierge is here.
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

        {/* Full guide link */}
        <div className="anim-fade-up anim-delay" style={D("0.48s")}>
          <button
            className="btn-press"
            onClick={onEnterGuide}
            style={{ width: "100%", background: "transparent", border: "1px solid #222", borderRadius: 12, padding: "14px", color: "#6B7280", fontSize: 13, cursor: "pointer", letterSpacing: "0.05em" }}
          >
            View Full Guest Guide →
          </button>
        </div>

      </div>
    </div>
  );
}
