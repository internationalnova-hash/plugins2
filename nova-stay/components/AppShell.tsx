"use client";

import { useState, useRef } from "react";
import TabNav, { TabKey } from "@/components/TabNav";
import StayTab from "@/components/tabs/StayTab";
import EntertainmentTab from "@/components/tabs/EntertainmentTab";
import ExploreTab from "@/components/tabs/ExploreTab";
import DiningTab from "@/components/tabs/DiningTab";
import SupportTab from "@/components/tabs/SupportTab";
import theme from "@/config/theme";

const GOLD = theme.gold;

const TAB_TITLES: Record<TabKey, string> = {
  stay: "Your Stay",
  entertainment: "Entertainment",
  explore: "Explore",
  dining: "Dining",
  support: "Support",
};

function TabContent({ active }: { active: TabKey }) {
  switch (active) {
    case "stay":          return <StayTab />;
    case "entertainment": return <EntertainmentTab />;
    case "explore":       return <ExploreTab />;
    case "dining":        return <DiningTab />;
    case "support":       return <SupportTab />;
  }
}

const TAB_ORDER: TabKey[] = ["stay", "entertainment", "explore", "dining", "support"];

export default function AppShell({ onBack, initialTab = "stay" }: { onBack: () => void; initialTab?: TabKey }) {
  const [active, setActive] = useState<TabKey>(initialTab);
  const [direction, setDirection] = useState<"left" | "right">("right");
  const prevIndexRef = useRef(TAB_ORDER.indexOf(initialTab));

  const handleChange = (tab: TabKey) => {
    const newIndex = TAB_ORDER.indexOf(tab);
    setDirection(newIndex >= prevIndexRef.current ? "right" : "left");
    prevIndexRef.current = newIndex;
    setActive(tab);
    window.scrollTo({ top: 0, behavior: "smooth" });
  };

  return (
    <div style={{ minHeight: "100vh", background: "#0A0A0A", paddingBottom: 80 }}>

      {/* Top bar */}
      <div
        className="lux-glass-bar"
        style={{
          position: "sticky",
          top: 0,
          zIndex: 50,
          borderBottom: "1px solid rgba(201,168,76,0.14)",
          padding: "14px 20px",
          display: "flex",
          alignItems: "center",
          justifyContent: "space-between",
        }}
      >
        <button
          onClick={onBack}
          className="btn-press"
          style={{
            background: "transparent",
            border: "none",
            color: "#6B7280",
            fontSize: 13,
            cursor: "pointer",
            padding: "2px 0",
            display: "flex",
            alignItems: "center",
            gap: 4,
          }}
        >
          ← Home
        </button>
        <span
          style={{
            color: GOLD,
            fontSize: 11,
            fontWeight: 700,
            letterSpacing: "0.25em",
            textTransform: "uppercase",
          }}
        >
          {TAB_TITLES[active]}
        </span>
        <span style={{ width: 52 }} />
      </div>

      {/* Tab content */}
      <div
        key={active}
        className={`tab-slide-${direction} max-w-lg mx-auto px-4 pt-4`}
      >
        <TabContent active={active} />
      </div>

      <TabNav active={active} onChange={handleChange} />
    </div>
  );
}
