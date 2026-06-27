"use client";

import { propertyConfig } from "@/data/config";

export default function WelcomeSection() {
  const { welcome } = propertyConfig;

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
      desc: "Projector, sound, and controls",
      id: "theater",
    },
    {
      icon: "🎙️",
      title: "Recording Studio",
      desc: "Studio access and equipment",
      id: "studio",
    },
    {
      icon: "🏊",
      title: "Pool & Outdoors",
      desc: "Pool rules, lights, and amenities",
      id: "pool",
    },
  ];

  return (
    <section className="px-4 py-6">
      <div className="grid grid-cols-2 gap-3">
        {/* Welcome card */}
        <div
          className="col-span-2 sm:col-span-1 rounded-2xl p-6 flex flex-col justify-between"
          style={{ backgroundColor: "#1A1A1A", border: "1px solid #2A2A2A", minHeight: "180px" }}
        >
          <div>
            <span className="text-2xl mb-3 block" style={{ color: "#C9A84C" }}>♛</span>
            <h2 className="text-2xl font-serif font-bold text-white mb-1">
              {welcome.heading}
            </h2>
            <div className="h-px w-12 mb-3" style={{ backgroundColor: "#C9A84C" }} />
            <p className="text-gray-400 text-sm leading-relaxed">{welcome.message}</p>
          </div>
          <p className="text-sm mt-4" style={{ color: "#C9A84C" }}>— {welcome.hostName}</p>
        </div>

        {/* YOUR GUIDE label + cards */}
        <div className="col-span-2">
          <p
            className="text-xs tracking-[0.25em] uppercase font-semibold mb-3"
            style={{ color: "#C9A84C" }}
          >
            Your Guide
          </p>
          <div className="grid grid-cols-2 gap-3">
            {guideCards.map((card) => (
              <button
                key={card.id}
                onClick={() =>
                  document.getElementById(card.id)?.scrollIntoView({ behavior: "smooth" })
                }
                className="rounded-2xl p-4 text-left active:opacity-70 transition-opacity"
                style={{ backgroundColor: "#1A1A1A", border: "1px solid #2A2A2A" }}
              >
                <span className="text-2xl mb-2 block">{card.icon}</span>
                <p
                  className="text-xs font-semibold uppercase tracking-wide mb-1"
                  style={{ color: "#C9A84C" }}
                >
                  {card.title}
                </p>
                <p className="text-gray-500 text-xs leading-snug">{card.desc}</p>
              </button>
            ))}
          </div>
        </div>
      </div>
    </section>
  );
}
