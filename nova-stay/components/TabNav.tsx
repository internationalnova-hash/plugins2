"use client";

const GOLD = "#C9A84C";

export type TabKey = "stay" | "entertainment" | "explore" | "dining" | "support";

const TABS: { key: TabKey; icon: string; label: string }[] = [
  { key: "stay",          icon: "🏡",  label: "Stay"       },
  { key: "entertainment", icon: "🎬",  label: "Experience" },
  { key: "explore",       icon: "📍",  label: "Explore"    },
  { key: "dining",        icon: "🍽️", label: "Dining"     },
  { key: "support",       icon: "✦",   label: "Nova"       },
];

export default function TabNav({
  active,
  onChange,
}: {
  active: TabKey;
  onChange: (tab: TabKey) => void;
}) {
  return (
    <div
      style={{
        position: "fixed",
        bottom: 0,
        left: 0,
        right: 0,
        zIndex: 100,
        background: "rgba(10,10,10,0.94)",
        backdropFilter: "blur(16px)",
        borderTop: "1px solid #1a1a1a",
        paddingBottom: "env(safe-area-inset-bottom, 0px)",
      }}
    >
      <div
        style={{
          maxWidth: 480,
          margin: "0 auto",
          display: "grid",
          gridTemplateColumns: "1fr 1fr 1fr 1fr 1fr",
        }}
      >
        {TABS.map((t) => {
          const isActive = t.key === active;
          return (
            <button
              key={t.key}
              className="btn-press"
              onClick={() => onChange(t.key)}
              style={{
                background: "transparent",
                border: "none",
                padding: "10px 0 8px",
                display: "flex",
                flexDirection: "column",
                alignItems: "center",
                gap: 3,
                cursor: "pointer",
              }}
            >
              <span
                style={{
                  fontSize: 19,
                  lineHeight: 1,
                  opacity: isActive ? 1 : 0.45,
                  transform: isActive ? "translateY(-1px)" : "none",
                  transition: "opacity 0.2s ease, transform 0.2s ease",
                }}
              >
                {t.icon}
              </span>
              <span
                style={{
                  fontSize: 10,
                  fontWeight: isActive ? 700 : 500,
                  letterSpacing: "0.04em",
                  color: isActive ? GOLD : "#4B5563",
                  transition: "color 0.2s ease",
                }}
              >
                {t.label}
              </span>
            </button>
          );
        })}
      </div>
    </div>
  );
}
