"use client";

import { useState, useEffect, useRef } from "react";
import GuestReadinessFlow from "@/components/GuestReadinessFlow";
import HostPreview from "@/components/HostPreview";
import ConciergeHome from "@/components/ConciergeHome";
import AppShell from "@/components/AppShell";
import SplashScreen from "@/components/SplashScreen";
import type { TabKey } from "@/components/TabNav";

type Screen = "loading" | "onboarding" | "concierge" | "guide";

export default function Home() {
  const [screen, setScreen] = useState<Screen>("loading");
  const [initialTab, setInitialTab] = useState<TabKey>("stay");
  const [isHost, setIsHost] = useState(false);
  const nextScreenRef = useRef<Screen>("onboarding");

  useEffect(() => {
    try {
      const params = new URLSearchParams(window.location.search);
      if (params.get("host") === "1") {
        localStorage.setItem("stayByNova_isHost", "1");
      }
      setIsHost(localStorage.getItem("stayByNova_isHost") === "1");
    } catch { /* ignore */ }

    try {
      const raw = localStorage.getItem("stayByNova_guestInfo");
      if (raw) {
        const parsed = JSON.parse(raw);
        if (parsed?.readinessPercent === 100) {
          nextScreenRef.current = "concierge";
        }
      }
    } catch { /* ignore */ }
  }, []);

  if (screen === "loading") {
    return <SplashScreen onDone={() => setScreen(nextScreenRef.current)} />;
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
