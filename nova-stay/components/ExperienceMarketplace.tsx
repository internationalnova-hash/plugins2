"use client";

import { Mic2, ChefHat, Car, Thermometer, Clock, Camera, Video, Flower2, type LucideIcon } from "lucide-react";
import { EXPERIENCES, type Experience } from "@/data/experiences";

const GOLD = "#C9A84C";

const ICONS: Record<Experience["icon"], LucideIcon> = {
  Mic2, ChefHat, Car, Thermometer, Clock, Camera, Video, Flower2,
};

export default function ExperienceMarketplace() {
  return (
    <div className="anim-fade-up" style={{ marginBottom: 24 }}>
      <p style={{ color: "#4B5563", fontSize: 10, letterSpacing: "0.2em", textTransform: "uppercase", marginBottom: 4 }}>
        Experience Marketplace
      </p>
      <p style={{ color: "#6B7280", fontSize: 13, lineHeight: 1.6, marginBottom: 14 }}>
        Elevate your stay with curated add-ons. Booking opens soon.
      </p>

      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 10 }}>
        {EXPERIENCES.map((exp) => {
          const Icon = ICONS[exp.icon];
          return (
            <div
              key={exp.id}
              className="lux-glass card-hover"
              style={{ padding: "18px 14px", display: "flex", flexDirection: "column", gap: 10 }}
            >
              <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between" }}>
                <Icon size={20} strokeWidth={1.6} style={{ color: GOLD }} />
                <span className="status-pill available" style={{ fontSize: 8.5 }}>Coming Soon</span>
              </div>
              <div>
                <p style={{ color: "#fff", fontSize: 13.5, fontWeight: 700, marginBottom: 4 }}>{exp.title}</p>
                <p style={{ color: "#6B7280", fontSize: 11.5, lineHeight: 1.5 }}>{exp.description}</p>
              </div>
              <p style={{ color: GOLD, fontSize: 11, fontWeight: 600, marginTop: "auto" }}>{exp.priceFrom}</p>
              <button
                disabled
                className="btn-press"
                style={{
                  width: "100%",
                  background: "rgba(255,255,255,0.03)",
                  border: "1px solid rgba(201,168,76,0.18)",
                  borderRadius: "var(--radius-sm)",
                  padding: "8px 0",
                  color: "#6B7280",
                  fontSize: 10.5,
                  fontWeight: 700,
                  letterSpacing: "0.08em",
                  textTransform: "uppercase",
                  cursor: "default",
                  opacity: 0.7,
                }}
              >
                Request
              </button>
            </div>
          );
        })}
      </div>
    </div>
  );
}
