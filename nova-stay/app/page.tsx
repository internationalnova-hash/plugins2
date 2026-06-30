"use client";

import { useState, useEffect } from "react";
import GuestReadinessFlow from "@/components/GuestReadinessFlow";
import HostPreview from "@/components/HostPreview";
import ConciergeHome from "@/components/ConciergeHome";
import AppShell from "@/components/AppShell";
import type { TabKey } from "@/components/TabNav";

type Screen = "loading" | "onboarding" | "concierge" | "guide";

export default function Home() {
  const [screen, setScreen] = useState<Screen>("loading");
  const [initialTab, setInitialTab] = useState<TabKey>("stay");
  const [isHost, setIsHost] = useState(false);

  useEffect(() => {
    try {
      const params = new URLSearchParams(window.location.search);
      if (params.get("host") === "1") {
        localStorage.setItem("novaStay_isHost", "1");
      }
      setIsHost(localStorage.getItem("novaStay_isHost") === "1");
    } catch { /* ignore */ }

    try {
      const raw = localStorage.getItem("novaStay_guestInfo");
      if (raw) {
        const parsed = JSON.parse(raw);
        if (parsed?.readinessPercent === 100) {
          setScreen("concierge");
          return;
        }
      }
    } catch { /* ignore */ }
    setScreen("onboarding");
  }, []);

  if (screen === "loading") {
    return (
      <div style={{ minHeight: "100vh", background: "#0A0A0A", display: "flex", alignItems: "center", justifyContent: "center" }}>
        <div style={{ textAlign: "center" }}>
          <div style={{ width: 40, height: 40, borderRadius: "50%", border: "1.5px solid #C9A84C", borderTopColor: "transparent", animation: "spin 0.8s linear infinite", margin: "0 auto 16px" }} />
          <style>{`@keyframes spin { to { transform: rotate(360deg); } }`}</style>
          <p style={{ color: "#2d2d2d", fontSize: 11, letterSpacing: "0.2em", textTransform: "uppercase" }}>Nova Stay</p>
        </div>
      </div>
    );
  }

  if (screen === "onboarding") {
    return (
      <>
        <GuestReadinessFlow onComplete={() => setScreen("concierge")} />
        {isHost && <HostPreview />}
      </>
    );
  }

  if (screen === "concierge") {
    return (
      <>
        <ConciergeHome onEnterGuide={(tab) => { setInitialTab(tab ?? "stay"); setScreen("guide"); }} />
        {isHost && <HostPreview />}
      </>
    );
  }

  return (
    <>
      <AppShell onBack={() => setScreen("concierge")} initialTab={initialTab} />
      {isHost && <HostPreview />}
    </>
  );
}
