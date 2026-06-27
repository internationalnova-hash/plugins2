"use client";

import { propertyConfig } from "@/data/config";

const guideCards = [
  { icon: "🚪", title: "Arrival & Access", desc: "Door code, check-in info, parking", id: "door-code" },
  { icon: "🎬", title: "Movie Theater", desc: "Projector, sound, controls", id: "theater" },
  { icon: "🎙️", title: "Recording Studio", desc: "Equipment, rules, setup", id: "studio" },
  { icon: "🏊", title: "Pool & Outdoors", desc: "Heated pool, deck, outdoor dining", id: "pool" },
  { icon: "🎮", title: "Game Room", desc: "Games, consoles, entertainment", id: "game-room" },
  { icon: "📶", title: "Wi-Fi", desc: "Network name, password", id: "wifi" },
  { icon: "🧳", title: "Checkout", desc: "Departure info, house rules", id: "checkout" },
];

export default function WelcomeSection() {
  const { welcome } = propertyConfig;
  return (
    <section className="w-full px-4 py-8" style={{ backgroundColor: "#0A0A0A" }}>
      <div className="max-w-lg mx-auto flex flex-col gap-6">
        <div className="rounded-2xl p-6" style={{ background: "linear-gradient(145deg,#1a1a1a,#111)", border: "1px solid #2a2a2a" }}>
          <span className="text-3xl block mb-4" style={{ color: "#C9A84C" }}>♛</span>
          <h2 className="text-3xl font-serif font-bold text-white mb-3">{welcome.heading}</h2>
          <div className="h-px w-10 mb-4" style={{ backgroundColor: "#C9A84C" }} />
          <p className="text-gray-400 text-sm leading-relaxed mb-6">{welcome.message}</p>
          <p className="text-sm font-serif italic" style={{ color: "#C9A84C" }}>— {welcome.hostName}</p>
        </div>
        <div>
          <p className="text-xs font-semibold tracking-[0.3em] uppercase mb-4" style={{ color: "#C9A84C" }}>Your Guide</p>
          <div className="grid grid-cols-2 gap-3">
            {guideCards.map((card) => (
              <button key={card.id} onClick={() => document.getElementById(card.id)?.scrollIntoView({ behavior: "smooth" })}
                className="rounded-2xl p-4 text-left active:scale-95 transition-all"
                style={{ background: "linear-gradient(145deg,#1a1a1a,#141414)", border: "1px solid #252525" }}>
                <div className="w-10 h-10 rounded-xl flex items-center justify-center mb-3 text-xl"
                  style={{ background: "rgba(201,168,76,0.1)", border: "1px solid rgba(201,168,76,0.2)" }}>
                  {card.icon}
                </div>
                <p className="text-xs font-bold uppercase tracking-wide mb-1" style={{ color: "#C9A84C" }}>{card.title}</p>
                <p className="text-gray-500 text-xs leading-snug mb-2">{card.desc}</p>
                <span className="text-xs" style={{ color: "#C9A84C" }}>→</span>
              </button>
            ))}
          </div>
        </div>
      </div>
    </section>
  );
}
