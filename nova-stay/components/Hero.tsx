"use client";

import { useEffect, useRef } from "react";
import { Instagram, Facebook, Youtube } from "lucide-react";
import property from "@/config/property";

function TikTokIcon() {
  return (
    <svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor">
      <path d="M19.59 6.69a4.83 4.83 0 01-3.77-4.25V2h-3.45v13.67a2.89 2.89 0 01-2.88 2.5 2.89 2.89 0 01-2.89-2.89 2.89 2.89 0 012.89-2.89c.28 0 .54.04.79.1V9.01a6.33 6.33 0 00-.79-.05 6.34 6.34 0 00-6.34 6.34 6.34 6.34 0 006.34 6.34 6.34 6.34 0 006.33-6.34V8.69a8.18 8.18 0 004.79 1.54V6.78a4.85 4.85 0 01-1.02-.09z" />
    </svg>
  );
}

export default function Hero() {
  const bgRef = useRef<HTMLDivElement>(null);

  // Parallax on scroll
  useEffect(() => {
    const el = bgRef.current;
    if (!el) return;
    const onScroll = () => {
      el.style.transform = `translateY(${window.scrollY * 0.28}px)`;
    };
    window.addEventListener("scroll", onScroll, { passive: true });
    return () => window.removeEventListener("scroll", onScroll);
  }, []);

  const scrollToGuide = () => {
    document.getElementById("guest-guide")?.scrollIntoView({ behavior: "smooth" });
  };

  const { social } = property;

  return (
    <>
      {/* Keyframe animations injected once */}
      <style>{`
        @keyframes fadeUp {
          from { opacity: 0; transform: translateY(24px); }
          to   { opacity: 1; transform: translateY(0); }
        }
        .hero-logo    { animation: fadeUp 0.9s ease both; animation-delay: 0.2s; }
        .hero-brand   { animation: fadeUp 0.9s ease both; animation-delay: 0.45s; }
        .hero-divider { animation: fadeUp 0.9s ease both; animation-delay: 0.6s; }
        .hero-title   { animation: fadeUp 1s ease both;   animation-delay: 0.75s; }
        .hero-amenity { animation: fadeUp 0.9s ease both; animation-delay: 0.95s; }
        .hero-tagline { animation: fadeUp 0.9s ease both; animation-delay: 1.1s; }
        .hero-cta     { animation: fadeUp 1s ease both;   animation-delay: 1.3s; }
        .hero-nav     { animation: fadeUp 0.9s ease both; animation-delay: 1.5s; }
      `}</style>

      <section
        className="relative w-full flex flex-col"
        style={{ height: "100vh", maxHeight: "100dvh", overflow: "hidden" }}
      >
        {/* ── Background image with parallax ── */}
        <div
          ref={bgRef}
          className="absolute will-change-transform"
          style={{
            inset: "-12% 0",
            // Focal point: center the pool + retaining wall + house
            backgroundImage: `url('${property.heroImage}')`,
            backgroundSize: "cover",
            backgroundPosition: "center 55%",
          }}
        />

        {/* Fallback gradient (visible when image hasn't loaded yet) */}
        <div
          className="absolute inset-0"
          style={{
            background:
              "linear-gradient(160deg, #1c1005 0%, #0d0d0d 50%, #060f18 100%)",
          }}
        />

        {/* 47% dark overlay — preserves gold lighting + deep blue pool */}
        <div
          className="absolute inset-0"
          style={{ backgroundColor: "rgba(0,0,0,0.47)" }}
        />

        {/* Bottom vignette — hides content below fold */}
        <div
          className="absolute bottom-0 left-0 right-0 pointer-events-none"
          style={{
            height: "180px",
            background:
              "linear-gradient(to bottom, transparent 0%, rgba(10,10,10,0.85) 70%, #0A0A0A 100%)",
          }}
        />

        {/* Social icons — top right */}
        <div className="absolute top-5 right-4 flex items-center gap-4 z-20 opacity-0 hero-brand">
          {social.instagram && (
            <a href={social.instagram} target="_blank" rel="noopener noreferrer">
              <Instagram size={16} className="text-white opacity-70 hover:opacity-100 transition-opacity" />
            </a>
          )}
          {social.facebook && (
            <a href={social.facebook} target="_blank" rel="noopener noreferrer">
              <Facebook size={16} className="text-white opacity-70 hover:opacity-100 transition-opacity" />
            </a>
          )}
          {social.youtube && (
            <a href={social.youtube} target="_blank" rel="noopener noreferrer">
              <Youtube size={16} className="text-white opacity-70 hover:opacity-100 transition-opacity" />
            </a>
          )}
          {social.tiktok && (
            <a href={social.tiktok} target="_blank" rel="noopener noreferrer" className="text-white opacity-70 hover:opacity-100 transition-opacity">
              <TikTokIcon />
            </a>
          )}
        </div>

        {/* ── Hero content — centered, vertically balanced ── */}
        <div className="relative z-10 flex flex-col items-center justify-center flex-1 px-6 text-center">
          {/* Logo circle */}
          <div
            className="hero-logo opacity-0 w-20 h-20 rounded-full flex items-center justify-center mb-4"
            style={{
              border: "2px solid #C9A84C",
              boxShadow: "0 0 48px rgba(201,168,76,0.25), inset 0 0 24px rgba(201,168,76,0.05)",
            }}
          >
            <span className="font-serif text-3xl font-bold" style={{ color: "#C9A84C" }}>
              N
            </span>
          </div>

          {/* Nova Stay wordmark */}
          <p className="hero-brand opacity-0 text-xs tracking-[0.45em] uppercase text-white font-light mb-1">
            Nova Stay
          </p>

          {/* Gold ornament */}
          <div className="hero-divider opacity-0 flex items-center gap-3 mb-5">
            <div className="h-px w-10" style={{ backgroundColor: "#C9A84C" }} />
            <span style={{ color: "#C9A84C", fontSize: "10px" }}>✦</span>
            <div className="h-px w-10" style={{ backgroundColor: "#C9A84C" }} />
          </div>

          {/* Property name */}
          <h1
            className="hero-title opacity-0 font-serif font-bold text-white leading-none tracking-wide mb-4"
            style={{ fontSize: "clamp(2.6rem, 9vw, 5.5rem)" }}
          >
            {property.name.toUpperCase()}
          </h1>

          {/* Amenities */}
          <p className="hero-amenity opacity-0 text-xs tracking-[0.18em] uppercase text-white opacity-70 mb-3">
            {property.amenities.join("  •  ")}
          </p>

          {/* Tagline */}
          <p className="hero-tagline opacity-0 text-sm tracking-[0.15em] uppercase mb-10 font-light"
             style={{ color: "rgba(255,255,255,0.55)" }}>
            {property.tagline}
          </p>

          {/* CTA button */}
          <button
            onClick={scrollToGuide}
            className="hero-cta opacity-0 px-12 py-4 text-sm font-bold tracking-[0.28em] uppercase transition-all hover:brightness-110 active:scale-95"
            style={{
              backgroundColor: "#C9A84C",
              color: "#0A0A0A",
              borderRadius: "4px",
              minWidth: "260px",
              boxShadow: "0 8px 40px rgba(201,168,76,0.4)",
            }}
          >
            Enter Guest Guide
          </button>
        </div>

        {/* ── Quick-access navigation cards ── */}
        <div className="hero-nav opacity-0 relative z-10 w-full px-3 pb-4">
          <div
            className="rounded-2xl"
            style={{
              backgroundColor: "rgba(10,10,10,0.80)",
              backdropFilter: "blur(20px)",
              WebkitBackdropFilter: "blur(20px)",
              border: "1px solid rgba(201,168,76,0.14)",
            }}
          >
            <div className="flex overflow-x-auto px-3 py-4 gap-1 no-scrollbar">
              {[
                { icon: "📶", label: "Wi-Fi",         id: "wifi" },
                { icon: "🔐", label: "Door Code",     id: "door-code" },
                { icon: "🎬", label: "Movie Theater", id: "theater" },
                { icon: "🎙️", label: "Studio",        id: "studio" },
                { icon: "🏊", label: "Pool",          id: "pool" },
                { icon: "🎮", label: "Game Room",     id: "game-room" },
                { icon: "🧳", label: "Checkout",      id: "checkout" },
              ].map((item) => (
                <button
                  key={item.id}
                  onClick={() =>
                    document
                      .getElementById(item.id)
                      ?.scrollIntoView({ behavior: "smooth" })
                  }
                  className="flex flex-col items-center gap-2 px-4 py-2 rounded-xl active:opacity-70 transition-opacity flex-shrink-0 min-w-[72px]"
                >
                  <span className="text-2xl">{item.icon}</span>
                  <span
                    className="text-xs font-medium text-center leading-tight"
                    style={{ color: "#C9A84C" }}
                  >
                    {item.label}
                  </span>
                </button>
              ))}
            </div>
          </div>
        </div>
      </section>
    </>
  );
}
