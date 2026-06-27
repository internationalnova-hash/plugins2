"use client";

import { useEffect, useRef } from "react";
import property from "@/config/property";

export default function Hero() {
  const bgRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const el = bgRef.current;
    if (!el) return;
    const onScroll = () => {
      el.style.transform = `translateY(${window.scrollY * 0.25}px)`;
    };
    window.addEventListener("scroll", onScroll, { passive: true });
    return () => window.removeEventListener("scroll", onScroll);
  }, []);

  const scrollToGuide = () => {
    document.getElementById("guest-guide")?.scrollIntoView({ behavior: "smooth" });
  };

  const navItems = [
    { icon: "📶", label: "Wi-Fi",         id: "wifi" },
    { icon: "🔐", label: "Door Code",     id: "door-code" },
    { icon: "🎬", label: "Movie Theater", id: "theater" },
    { icon: "🎙️", label: "Studio",        id: "studio" },
    { icon: "🏊", label: "Pool",          id: "pool" },
    { icon: "🎮", label: "Game Room",     id: "game-room" },
    { icon: "🧳", label: "Checkout",      id: "checkout" },
  ];

  return (
    <>
      <style>{`
        @keyframes fadeUp {
          from { opacity: 0; transform: translateY(20px); }
          to   { opacity: 1; transform: translateY(0); }
        }
        @keyframes fadeIn {
          from { opacity: 0; }
          to   { opacity: 1; }
        }
        @keyframes bounce {
          0%, 100% { transform: translateY(0); }
          50%       { transform: translateY(6px); }
        }
        .h-logo    { animation: fadeUp 0.8s ease both; animation-delay: 0.1s; }
        .h-nav     { animation: fadeIn 0.8s ease both; animation-delay: 0.2s; }
        .h-title   { animation: fadeUp 1s ease both;   animation-delay: 0.4s; }
        .h-sub     { animation: fadeUp 0.9s ease both; animation-delay: 0.6s; }
        .h-tag     { animation: fadeUp 0.9s ease both; animation-delay: 0.75s; }
        .h-cta     { animation: fadeUp 0.9s ease both; animation-delay: 0.9s; }
        .h-chevron { animation: fadeIn 1s ease both;   animation-delay: 1.2s; }
        .chevron-bounce { animation: bounce 1.8s ease-in-out infinite; }
      `}</style>

      <section
        className="relative w-full flex flex-col"
        style={{ height: "100vh", minHeight: "600px", overflow: "hidden" }}
      >
        {/* Parallax background */}
        <div
          ref={bgRef}
          className="absolute will-change-transform"
          style={{
            inset: "-12% 0",
            backgroundImage: `url('${property.heroImage}')`,
            backgroundSize: "cover",
            backgroundPosition: "center 65%",
            zIndex: 0,
          }}
        />

        {/* Fallback gradient */}
        <div className="absolute inset-0" style={{ background: "linear-gradient(160deg,#1c1005,#0d0d0d 50%,#060f18)", zIndex: 0 }} />

        {/* Dark overlay */}
        <div className="absolute inset-0" style={{ backgroundColor: "rgba(0,0,0,0.42)", zIndex: 1 }} />

        {/* Bottom vignette */}
        <div className="absolute bottom-0 left-0 right-0 pointer-events-none" style={{ height: "220px", background: "linear-gradient(to bottom,transparent,rgba(10,10,10,0.7) 60%,#0A0A0A 100%)", zIndex: 2 }} />

        {/* ── TOP BAR: Logo center + Nav right ── */}
        <div className="relative z-10 flex items-start justify-between px-6 pt-6">

          {/* Spacer left (balances nav on right) */}
          <div className="hidden md:block" style={{ minWidth: "200px" }} />

          {/* Center: Logo + wordmark */}
          <div className="h-logo opacity-0 flex flex-col items-center gap-1 flex-1 md:flex-none">
            <div
              className="w-14 h-14 rounded-full flex items-center justify-center"
              style={{ border: "2px solid #C9A84C", boxShadow: "0 0 32px rgba(201,168,76,0.2)" }}
            >
              <span className="font-serif text-2xl font-bold" style={{ color: "#C9A84C" }}>N</span>
            </div>
            <div className="flex items-center gap-2 mt-1">
              <div className="h-px w-6" style={{ backgroundColor: "#C9A84C" }} />
              <p className="text-xs tracking-[0.35em] uppercase text-white font-light">Nova Stay</p>
              <div className="h-px w-6" style={{ backgroundColor: "#C9A84C" }} />
            </div>
          </div>

          {/* Right: Nav icons */}
          <nav className="h-nav opacity-0 hidden md:flex items-start gap-5 pt-1" style={{ minWidth: "200px", justifyContent: "flex-end" }}>
            {navItems.map((item) => (
              <button
                key={item.id}
                onClick={() => document.getElementById(item.id)?.scrollIntoView({ behavior: "smooth" })}
                className="flex flex-col items-center gap-1 group"
              >
                <span className="text-lg">{item.icon}</span>
                <span className="text-xs text-white opacity-70 group-hover:opacity-100 transition-opacity tracking-wide" style={{ fontSize: "10px" }}>
                  {item.label}
                </span>
              </button>
            ))}
          </nav>
        </div>

        {/* ── HERO CENTER CONTENT ── */}
        <div className="relative z-10 flex flex-col items-center justify-center flex-1 px-6 text-center -mt-8">

          {/* Property name */}
          <h1
            className="h-title opacity-0 font-serif font-bold text-white leading-none tracking-wide mb-4"
            style={{ fontSize: "clamp(2.8rem, 8vw, 5.5rem)", textShadow: "0 2px 24px rgba(0,0,0,0.5)" }}
          >
            {property.name.toUpperCase()}
          </h1>

          {/* Amenities */}
          <p className="h-sub opacity-0 text-xs tracking-[0.2em] uppercase text-white mb-4" style={{ opacity: 0.8 }}>
            {property.amenities.join("  •  ")}
          </p>

          {/* Italic tagline */}
          <p
            className="h-tag opacity-0 mb-2 font-serif italic text-white"
            style={{ fontSize: "clamp(1.1rem, 3vw, 1.5rem)", opacity: 0.9, textShadow: "0 1px 12px rgba(0,0,0,0.6)" }}
          >
            {property.tagline}
          </p>

          {/* Gold ornament */}
          <div className="h-tag opacity-0 flex items-center gap-3 mb-7">
            <div className="h-px w-12" style={{ backgroundColor: "#C9A84C" }} />
            <span style={{ color: "#C9A84C", fontSize: "12px" }}>★</span>
            <div className="h-px w-12" style={{ backgroundColor: "#C9A84C" }} />
          </div>

          {/* CTA button */}
          <button
            onClick={scrollToGuide}
            className="h-cta opacity-0 flex items-center gap-3 px-10 py-4 text-sm font-bold tracking-[0.25em] uppercase transition-all hover:brightness-110 active:scale-95 mb-8"
            style={{
              background: "linear-gradient(135deg, #C9A84C 0%, #E8C97A 50%, #C9A84C 100%)",
              color: "#0A0A0A",
              borderRadius: "4px",
              minWidth: "280px",
              justifyContent: "center",
              boxShadow: "0 8px 40px rgba(201,168,76,0.45)",
            }}
          >
            Enter Guest Guide
            <span className="ml-1">→</span>
          </button>

          {/* Scroll chevron */}
          <div className="h-chevron opacity-0 chevron-bounce flex flex-col items-center gap-0.5">
            <svg width="24" height="14" viewBox="0 0 24 14" fill="none">
              <path d="M2 2L12 11L22 2" stroke="#C9A84C" strokeWidth="2.5" strokeLinecap="round"/>
            </svg>
            <svg width="24" height="14" viewBox="0 0 24 14" fill="none" style={{ opacity: 0.5 }}>
              <path d="M2 2L12 11L22 2" stroke="#C9A84C" strokeWidth="2.5" strokeLinecap="round"/>
            </svg>
          </div>
        </div>

        {/* Mobile nav strip — shown only on small screens */}
        <div className="relative z-10 flex md:hidden overflow-x-auto px-4 pb-4 gap-4 no-scrollbar">
          {navItems.map((item) => (
            <button
              key={item.id}
              onClick={() => document.getElementById(item.id)?.scrollIntoView({ behavior: "smooth" })}
              className="flex flex-col items-center gap-1 flex-shrink-0"
            >
              <span className="text-xl">{item.icon}</span>
              <span className="text-xs text-white opacity-70 whitespace-nowrap" style={{ fontSize: "10px", color: "#C9A84C" }}>
                {item.label}
              </span>
            </button>
          ))}
        </div>
      </section>
    </>
  );
}
