"use client";

import { useState, useEffect } from "react";
import { propertyConfig } from "@/data/config";
import theme from "@/config/theme";
import { trackAmenityView } from "@/lib/tracking";

const GOLD = theme.gold;

/* ── Expandable info card — preview + chevron, animates open ─────────────── */

function ExpandCard({
  icon,
  label,
  preview,
  children,
  delay = 0,
  amenity,
}: {
  icon: string;
  label: string;
  preview: string;
  children: React.ReactNode;
  delay?: number;
  amenity?: string;
}) {
  const [open, setOpen] = useState(false);
  return (
    <div className="anim-fade-up anim-delay" style={{ "--delay": `${delay}s` } as React.CSSProperties}>
      <div style={{ background: "#111", border: "1px solid #1e1e1e", borderRadius: 20, marginBottom: 12, overflow: "hidden" }}>
        <button
          className="btn-press"
          onClick={() =>
            setOpen((o) => {
              if (!o && amenity) trackAmenityView(amenity);
              return !o;
            })
          }
          style={{ width: "100%", background: "transparent", border: "none", padding: "28px 24px", textAlign: "left", cursor: "pointer", display: "flex", alignItems: "center", justifyContent: "space-between" }}
        >
          <div style={{ display: "flex", alignItems: "center", gap: 12, flex: 1 }}>
            <span style={{ fontSize: 22 }}>{icon}</span>
            <div>
              <p style={{ color: "#6B7280", fontSize: 11, fontWeight: 600, letterSpacing: "0.18em", textTransform: "uppercase", marginBottom: 4 }}>{label}</p>
              <p style={{ color: "#fff", fontSize: 20, fontWeight: 400 }}>{preview}</p>
            </div>
          </div>
          <span style={{ color: "#2d2d2d", fontSize: 18, transform: open ? "rotate(180deg)" : "none", transition: "transform 0.25s ease" }}>›</span>
        </button>
        {open && (
          <div style={{ padding: "0 24px 24px", borderTop: "1px solid #1a1a1a" }}>
            <div style={{ paddingTop: 20 }}>{children}</div>
          </div>
        )}
      </div>
    </div>
  );
}

function StepList({ steps }: { steps: string[] }) {
  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 14 }}>
      {steps.map((step, i) => (
        <div key={i} style={{ display: "flex", alignItems: "flex-start", gap: 14 }}>
          <span style={{ color: GOLD, fontSize: 12, fontWeight: 700, minWidth: 18, marginTop: 2 }}>{i + 1}</span>
          <p style={{ color: "#9CA3AF", fontSize: 14, lineHeight: 1.6 }}>{step}</p>
        </div>
      ))}
    </div>
  );
}

function BulletList({ items }: { items: string[] }) {
  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 12 }}>
      {items.map((item, i) => (
        <div key={i} style={{ display: "flex", alignItems: "flex-start", gap: 10 }}>
          <span style={{ color: GOLD, fontSize: 10, marginTop: 5 }}>◆</span>
          <p style={{ color: "#9CA3AF", fontSize: 14, lineHeight: 1.6 }}>{item}</p>
        </div>
      ))}
    </div>
  );
}

/* ── Studio package — tap to copy a ready-to-send booking request ────────── */

function StudioPackageCard({ pkg, delay }: { pkg: { duration: string; price: string; includes: string }; delay: number }) {
  const [copied, setCopied] = useState(false);

  const handleCopy = () => {
    trackAmenityView("studio-request");
    const message = `Hi! I'd like to book the ${pkg.duration} (${pkg.price}) recording studio add-on for my stay.`;
    navigator.clipboard.writeText(message).then(() => {
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    });
  };

  return (
    <button
      className="btn-press card-hover anim-fade-up anim-delay"
      onClick={handleCopy}
      style={{
        "--delay": `${delay}s`,
        width: "100%",
        background: "#0d0d0d",
        border: `1px solid ${copied ? GOLD + "55" : "#1e1e1e"}`,
        borderRadius: 14,
        padding: "18px 20px",
        marginBottom: 10,
        display: "flex",
        alignItems: "center",
        justifyContent: "space-between",
        cursor: "pointer",
        textAlign: "left",
        transition: "border-color 0.25s ease",
      } as React.CSSProperties}
    >
      <div>
        <p style={{ color: "#fff", fontSize: 16, fontWeight: 600 }}>{pkg.duration}</p>
        <p style={{ color: "#6B7280", fontSize: 12, marginTop: 2 }}>{pkg.includes}</p>
      </div>
      <div style={{ textAlign: "right" }}>
        <p style={{ color: GOLD, fontSize: 20, fontWeight: 700, fontFamily: "Georgia, serif" }}>{pkg.price}</p>
        <p style={{ color: copied ? GOLD : "#2d2d2d", fontSize: 10, fontWeight: 700, letterSpacing: "0.08em", textTransform: "uppercase", marginTop: 2 }}>
          {copied ? "✓ Copied" : "Tap to request"}
        </p>
      </div>
    </button>
  );
}

export default function EntertainmentTab() {
  const { theater, studio, pool, gameRoom, grill } = propertyConfig;

  useEffect(() => {
    trackAmenityView("studio");
  }, []);

  return (
    <div style={{ paddingTop: 4 }}>
      <ExpandCard icon="🎬" label="Movie Theater" preview="Tap for setup steps" delay={0} amenity="theater">
        <StepList steps={theater.steps} />
      </ExpandCard>

      <div className="anim-fade-up anim-delay" style={{ "--delay": "0.05s" } as React.CSSProperties}>
        <div style={{ background: "#111", border: "1px solid #1e1e1e", borderRadius: 20, padding: "28px 24px", marginBottom: 12 }}>
          <div style={{ display: "flex", alignItems: "center", gap: 10, marginBottom: 16 }}>
            <span style={{ fontSize: 22 }}>🎙️</span>
            <span style={{ color: "#6B7280", fontSize: 11, fontWeight: 600, letterSpacing: "0.18em", textTransform: "uppercase" }}>{studio.heading}</span>
          </div>
          <p style={{ color: "#9CA3AF", fontSize: 14, lineHeight: 1.6, marginBottom: 18 }}>{studio.description}</p>
          {studio.packages.map((pkg, i) => (
            <StudioPackageCard key={i} pkg={pkg} delay={0.05 + i * 0.05} />
          ))}
          <p style={{ color: "#4B5563", fontSize: 12, textAlign: "center", marginTop: 8 }}>{studio.bookingNote}</p>
        </div>
      </div>

      <ExpandCard icon="🏊" label="Pool & Backyard" preview="Tap for pool rules" delay={0.1} amenity="pool">
        <BulletList items={pool.rules} />
      </ExpandCard>

      <ExpandCard icon="🎮" label="Game Room" preview="Available 24 hours" delay={0.15} amenity="game-room">
        <BulletList items={gameRoom.details} />
      </ExpandCard>

      <ExpandCard icon="💡" label="Outdoor Lighting & Grill" preview="Tap for instructions" delay={0.2} amenity="grill">
        <StepList steps={grill.instructions} />
      </ExpandCard>
    </div>
  );
}
