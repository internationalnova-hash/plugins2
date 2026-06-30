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
  const [fading, setFading] = useState(false);
  const nextScreenRef = useRef<Screen>("onboarding");

  const goTo = (next: Screen) => {
    setFading(true);
    setTimeout(() => {
      setScreen(next);
      setFading(false);
      window.scrollTo({ top: 0 });
    }, 220);
  };

  useEffect(() => {
    try {
      const params = new URLSearchParams(window.location.search);
      if (params.get("host") === "1") {
        localStorage.setItem("stayByNova_isHost", "1");
      }
      setIsHost(localStorage.getItem("stayByNova_isHost") === "1");
    } catch { /* ignore */ }

    // Sync host-edited property identity (name, hero image, brand color, etc.)
    // from the database into localStorage, which config/property.ts and
    // config/theme.ts read synchronously. Reload once if anything changed so
    // every component picks up the new values immediately.
    fetch("/api/property")
      .then((res) => (res.ok ? res.json() : null))
      .then((data) => {
        if (!data) return;
        const overrides = {
          propertyName: data.propertyName,
          hostName: data.hostName,
          city: data.city,
          tagline: data.tagline,
          heroImageUrl: data.heroImageUrl,
          goldColor: data.goldColor,
          amenities: data.amenities,
        };
        const prev = localStorage.getItem("stayByNova_propertyOverrides");
        const next = JSON.stringify(overrides);
        if (prev !== next) {
          localStorage.setItem("stayByNova_propertyOverrides", next);
          if (prev !== null) window.location.reload();
        }
      })
      .catch(() => { /* ignore — fall back to hardcoded defaults */ });

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
        <div className={`screen-dissolve ${fading ? "fading" : ""}`}>
          <GuestReadinessFlow onComplete={() => goTo("concierge")} />
        </div>
        {isHost && <HostPreview />}
      </>
    );
  }

  if (screen === "concierge") {
    return (
      <>
        <div className={`screen-dissolve ${fading ? "fading" : ""}`}>
          <ConciergeHome onEnterGuide={(tab) => { setInitialTab(tab ?? "stay"); goTo("guide"); }} />
        </div>
        {isHost && <HostPreview />}
      </>
    );
  }

  return (
    <>
      <div className={`screen-dissolve ${fading ? "fading" : ""}`}>
        <AppShell onBack={() => goTo("concierge")} initialTab={initialTab} />
      </div>
      {isHost && <HostPreview />}
    </>
  );
}
