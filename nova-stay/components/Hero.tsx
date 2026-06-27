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
  const heroRef = useRef<HTMLDivElement>(null);

  // Subtle parallax on scroll
  useEffect(() => {
    const el = heroRef.current;
    if (!el) return;
    const onScroll = () => {
      const scrolled = window.scrollY;
      el.style.transform = `translateY(${scrolled * 0.3}px)`;
    };
    window.addEventListener("scroll", onScroll, { passive: true });
    return () => window.removeEventListener("scroll", onScroll);
  }, []);

  const scrollToGuide = () => {
    document.getElementById("guest-guide")?.scrollIntoView({ behavior: "smooth" });
  };

  const { social } = property;

  return (
    <section className="relative w-full min-h-screen flex flex-col overflow-hidden">
      {/* Parallax background — swappable via config/property.ts heroImage */}
      <div
        ref={heroRef}
        className="absolute inset-0 will-change-transform"
        style={{
          backgroundImage: `url('${property.heroImage}')`,
          backgroundSize: "cover",
          backgroundPosition: "center",
          // Extends past viewport so parallax doesn't show edges
          top: "-10%",
          bottom: "-10%",
        }}
      />

      {/* Fallback gradient (visible when no hero image present) */}
      <div
        className="absolute inset-0"
        style={{
          background:
            "linear-gradient(160deg, #1a1008 0%, #0d0d0d 40%, #0a1a1a 100%)",
        }}
      />

      {/* Dark overlay for readability */}
      <div className="absolute inset-0" style={{ backgroundColor: "rgba(0,0,0,0.50)" }} />

      {/* Vignette — bottom fade to page background */}
      <div
        className="absolute bottom-0 left-0 right-0 h-48 pointer-events-none"
        style={{
          background: "linear-gradient(to bottom, transparent, #0A0A0A)",
        }}
      />

      {/* Social icons — top right */}
      <div className="absolute top-5 right-4 flex items-center gap-4 z-20">
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

      {/* Hero content */}
      <div className="relative z-10 flex flex-col items-center justify-center flex-1 px-6 pt-20 pb-10 text-center">
        {/* Logo circle */}
        <div
          className="w-20 h-20 rounded-full flex items-center justify-center mb-4"
          style={{
            border: "2px solid #C9A84C",
            boxShadow: "0 0 40px rgba(201,168,76,0.2)",
          }}
        >
          <span className="font-serif text-3xl font-bold" style={{ color: "#C9A84C" }}>
            N
          </span>
        </div>

        {/* Brand name */}
        <p className="text-xs tracking-[0.45em] uppercase text-white opacity-85 mb-1 font-light">
          Nova Stay
        </p>

        {/* Gold ornament divider */}
        <div className="flex items-center gap-3 mb-5">
          <div className="h-px w-10" style={{ backgroundColor: "#C9A84C" }} />
          <span style={{ color: "#C9A84C", fontSize: "10px" }}>✦</span>
          <div className="h-px w-10" style={{ backgroundColor: "#C9A84C" }} />
        </div>

        {/* Property name — driven by config */}
        <h1
          className="font-serif font-bold text-white leading-none tracking-wide mb-4"
          style={{ fontSize: "clamp(2.6rem, 9vw, 5rem)" }}
        >
          {property.name.toUpperCase()}
        </h1>

        {/* Amenities — driven by config */}
        <p className="text-xs tracking-[0.18em] uppercase text-white opacity-70 mb-3">
          {property.amenities.join("  •  ")}
        </p>

        {/* Tagline — driven by config */}
        <p className="text-sm tracking-[0.15em] uppercase text-white opacity-55 mb-10 font-light">
          {property.tagline}
        </p>

        {/* CTA */}
        <button
          onClick={scrollToGuide}
          className="px-12 py-4 text-sm font-bold tracking-[0.28em] uppercase transition-all active:scale-95"
          style={{
            backgroundColor: "#C9A84C",
            color: "#0A0A0A",
            borderRadius: "4px",
            minWidth: "260px",
            boxShadow: "0 8px 32px rgba(201,168,76,0.35)",
          }}
        >
          Enter Guest Guide
        </button>
      </div>

      {/* Quick-access navigation cards */}
      <div className="relative z-10 w-full px-3 pb-6">
        <div
          className="rounded-2xl overflow-x-auto"
          style={{
            backgroundColor: "rgba(10,10,10,0.82)",
            backdropFilter: "blur(16px)",
            border: "1px solid rgba(201,168,76,0.12)",
          }}
        >
          <div className="flex min-w-max px-3 py-4 gap-1 mx-auto">
            {[
              { icon: "📶", label: "Wi-Fi", id: "wifi" },
              { icon: "🔐", label: "Door Code", id: "door-code" },
              { icon: "🎬", label: "Movie Theater", id: "theater" },
              { icon: "🎙️", label: "Studio", id: "studio" },
              { icon: "🏊", label: "Pool", id: "pool" },
              { icon: "🎮", label: "Game Room", id: "game-room" },
              { icon: "🧳", label: "Checkout", id: "checkout" },
            ].map((item) => (
              <button
                key={item.id}
                onClick={() =>
                  document
                    .getElementById(item.id)
                    ?.scrollIntoView({ behavior: "smooth" })
                }
                className="flex flex-col items-center gap-2 px-4 py-2 rounded-xl active:opacity-70 transition-opacity min-w-[72px]"
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
  );
}
