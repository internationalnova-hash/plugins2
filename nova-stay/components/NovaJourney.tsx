"use client";

import { Plane, Home, Sunrise, PartyPopper, Sparkles, Heart, type LucideIcon } from "lucide-react";
import { JOURNEY_STAGES, type JourneyStageKey } from "@/lib/novaJourney";
import type { TabKey } from "@/components/TabNav";

const GOLD = "#C9A84C";

const ICONS: Record<JourneyStageKey, LucideIcon> = {
  before: Plane,
  checkedIn: Home,
  enjoying: Sunrise,
  experiences: PartyPopper,
  checkout: Sparkles,
  review: Heart,
};

const STAGE_TAB: Record<JourneyStageKey, TabKey | undefined> = {
  before: "stay",
  checkedIn: "stay",
  enjoying: "entertainment",
  experiences: "entertainment",
  checkout: "stay",
  review: undefined,
};

export default function NovaJourney({
  current,
  onSelectTab,
}: {
  current: JourneyStageKey;
  onSelectTab?: (tab?: TabKey) => void;
}) {
  const currentIndex = JOURNEY_STAGES.findIndex((s) => s.key === current);

  return (
    <div style={{ overflowX: "auto" }} className="no-scrollbar">
      <div style={{ display: "flex", gap: 6, paddingBottom: 4, minWidth: "max-content" }}>
        {JOURNEY_STAGES.map((stage, i) => {
          const Icon = ICONS[stage.key];
          const isActive = i === currentIndex;
          const isPast = i < currentIndex;
          return (
            <button
              key={stage.key}
              className="btn-press"
              onClick={() => onSelectTab?.(STAGE_TAB[stage.key])}
              style={{
                display: "flex",
                flexDirection: "column",
                alignItems: "center",
                gap: 6,
                background: "transparent",
                border: "none",
                cursor: "pointer",
                padding: "6px 10px",
                minWidth: 76,
                opacity: isActive ? 1 : isPast ? 0.55 : 0.4,
                transition: "opacity 0.25s ease",
              }}
            >
              <div
                style={{
                  width: 34,
                  height: 34,
                  borderRadius: "50%",
                  display: "flex",
                  alignItems: "center",
                  justifyContent: "center",
                  border: `1.5px solid ${isActive ? GOLD : "#2a2a2a"}`,
                  background: isActive ? "rgba(201,168,76,0.12)" : "transparent",
                  boxShadow: isActive ? "0 0 14px rgba(201,168,76,0.3)" : "none",
                  transition: "all 0.25s ease",
                }}
              >
                <Icon size={16} strokeWidth={1.7} style={{ color: isActive ? GOLD : "#6B7280" }} />
              </div>
              <span
                style={{
                  fontSize: 9.5,
                  letterSpacing: "0.04em",
                  textAlign: "center",
                  color: isActive ? GOLD : "#6B7280",
                  fontWeight: isActive ? 700 : 500,
                  whiteSpace: "nowrap",
                }}
              >
                {stage.label}
              </span>
            </button>
          );
        })}
      </div>
    </div>
  );
}
