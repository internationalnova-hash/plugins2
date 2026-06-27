"use client";

import { propertyConfig } from "@/data/config";

const guideCards = [
  {
    icon: "🚪",
    title: "Arrival & Access",
    desc: "Door code, check-in info, parking",
    id: "door-code",
  },
  {
    icon: "🎬",
    title: "Movie Theater",
    desc: "Projector, sound, controls",
    id: "theater",
  },
  {
    icon: "🎙️",
    title: "Recording Studio",
    desc: "Equipment, rules, setup",
    id: "studio",
  },
  {
    icon: "🏊",
    title: "Pool & Outdoors",
    desc: "Heated pool, deck, outdoor dining",
    id: "pool",
  },
  {
    icon: "🎮",
    title: "Game Room",
    desc: "Games, consoles, entertainment",
    id: "game-room",
  },
  {
    icon: "📶",
    title: "Wi-Fi",
    desc: "Network name, password",
    id: "wifi",
  },
  {
    icon: "🧳",
    title: "Checkout",
    desc: "Departure info, house rules",
    id: "checkout",
  },
];

export default function WelcomeSection() {
  const { welcome } = propertyConfig;

  return (
    <section
      className="w-full px-4 py-8 md:px-8"
      style={{ backgroundColor: "#0A0A0A" }}
    >
      <div className="max-w-6xl mx-auto flex flex-col md:flex-row gap-6 items-start">

        {/* ── Left: Welcome card ── */}
        <div
          className="w-full md:w-72 flex-shrink-0 rounded-2xl p-6 flex flex-col justify-between"
          style={{
            background: "linear-gradient(145deg, #1a1a1a, #111)",
            border: "1px solid #2a2a2a",
            boxShadow: "0 0 40px rgba(201,168,76,0.06)",
            minHeight: "320px",
          }}
        >
          {/* Crown */}
          <div>
            <span className="text-3xl block mb-4" style={{ color: "#C9A84C" }}>♛</span>
            <h2 className="text-3xl font-serif font-bold text-white mb-3 leading-tight">
              {welcome.heading}
            </h2>
            <div className="h-px w-10 mb-4" style={{ backgroundColor: "#C9A84C" }} />
            <p className="text-gray-400 text-sm leading-relaxed">
              {welcome.message}
            </p>
          </div>
          <p className="text-sm mt-6 font-serif italic" style={{ color: "#C9A84C" }}>
            — {welcome.hostName}
          </p>
        </div>

        {/* ── Right: Guide grid ── */}
        <div className="flex-1">
          <p
            className="text-xs font-semibold tracking-[0.3em] uppercase mb-4"
            style={{ color: "#C9A84C" }}
          >
            Your Guide
          </p>

          <div className="grid grid-cols-2 sm:grid-cols-3 lg:grid-cols-4 gap-3">
            {guideCards.map((card) => (
              <button
                key={card.id}
                onClick={() =>
                  document.getElementById(card.id)?.scrollIntoView({ behavior: "smooth" })
                }
                className="group rounded-2xl p-4 text-left transition-all active:scale-95"
                style={{
                  background: "linear-gradient(145deg, #1a1a1a, #141414)",
                  border: "1px solid #252525",
                }}
              >
                {/* Icon box */}
                <div
                  className="w-12 h-12 rounded-xl flex items-center justify-center mb-3 text-2xl"
                  style={{
                    background: "linear-gradient(135deg, rgba(201,168,76,0.15), rgba(201,168,76,0.05))",
                    border: "1px solid rgba(201,168,76,0.2)",
                  }}
                >
                  {card.icon}
                </div>

                <p
                  className="text-xs font-bold uppercase tracking-wider mb-1 leading-tight"
                  style={{ color: "#C9A84C" }}
                >
                  {card.title}
                </p>
                <p className="text-gray-500 text-xs leading-snug mb-3">
                  {card.desc}
                </p>

                {/* Arrow */}
                <span className="text-xs" style={{ color: "#C9A84C" }}>→</span>
              </button>
            ))}
          </div>
        </div>
      </div>
    </section>
  );
}
