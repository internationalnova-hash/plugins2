"use client";

import { useState, useEffect } from "react";
import Hero from "@/components/Hero";
import WelcomeSection from "@/components/WelcomeSection";
import WifiCard from "@/components/WifiCard";
import DoorCode from "@/components/DoorCode";
import TheaterGuide from "@/components/TheaterGuide";
import StudioAddon from "@/components/StudioAddon";
import PoolRules from "@/components/PoolRules";
import GameRoom from "@/components/GameRoom";
import HouseRules from "@/components/HouseRules";
import Checkout from "@/components/Checkout";
import EmergencyContacts from "@/components/EmergencyContacts";
import LocalRecs from "@/components/LocalRecs";
import FAQ from "@/components/FAQ";
import QRCodeSection from "@/components/QRCode";
import FloatingHelp from "@/components/FloatingHelp";
import GuestReadinessFlow from "@/components/GuestReadinessFlow";
import HostPreview from "@/components/HostPreview";
import ConciergeHome from "@/components/ConciergeHome";

type Screen = "loading" | "onboarding" | "concierge" | "guide";

export default function Home() {
  const [screen, setScreen] = useState<Screen>("loading");

  useEffect(() => {
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
      <div
        style={{
          minHeight: "100vh",
          background: "#0A0A0A",
          display: "flex",
          alignItems: "center",
          justifyContent: "center",
        }}
      >
        <div style={{ textAlign: "center" }}>
          <div
            style={{
              width: 40,
              height: 40,
              borderRadius: "50%",
              border: "1.5px solid #C9A84C",
              borderTopColor: "transparent",
              animation: "spin 0.8s linear infinite",
              margin: "0 auto 16px",
            }}
          />
          <style>{`@keyframes spin { to { transform: rotate(360deg); } }`}</style>
          <p style={{ color: "#2d2d2d", fontSize: 11, letterSpacing: "0.2em", textTransform: "uppercase" }}>
            Nova Stay
          </p>
        </div>
      </div>
    );
  }

  if (screen === "onboarding") {
    return (
      <>
        <GuestReadinessFlow onComplete={() => setScreen("concierge")} />
        <HostPreview />
      </>
    );
  }

  if (screen === "concierge") {
    return (
      <>
        <ConciergeHome onEnterGuide={() => setScreen("guide")} />
        <HostPreview />
      </>
    );
  }

  // Full guide
  return (
    <>
      <main className="bg-nova-black min-h-screen anim-fade-in">
        <Hero />
        <div id="guest-guide" className="max-w-lg mx-auto px-4 pb-24">
          <WelcomeSection />
          <div className="divider-gold my-6 mx-4" />
          <div id="wifi"><WifiCard /></div>
          <div id="door-code"><DoorCode /></div>
          <div id="theater"><TheaterGuide /></div>
          <div id="studio"><StudioAddon /></div>
          <div id="pool"><PoolRules /></div>
          <div id="game-room"><GameRoom /></div>
          <HouseRules />
          <div id="checkout"><Checkout /></div>
          <EmergencyContacts />
          <LocalRecs />
          <FAQ />
          <QRCodeSection />
          <footer className="mt-8">
            <div className="divider-gold mb-6" />
            <div className="flex items-center justify-between mb-4">
              <div>
                <p className="text-xs font-semibold text-white tracking-widest uppercase">Casanova ATL</p>
                <p className="text-xs text-gray-600">Atlanta, Georgia</p>
              </div>
              <div className="text-center">
                <p className="text-xs text-gray-600 italic" style={{ fontFamily: "Georgia, serif" }}>Powered by</p>
                <p className="text-sm font-bold" style={{ color: "#C9A84C" }}>Nova Stay</p>
                <p className="text-xs text-gray-700 tracking-widest uppercase" style={{ fontSize: "9px" }}>A Nova Experience</p>
              </div>
              <div className="text-right">
                <p className="text-xs text-gray-600 uppercase tracking-widest">Property</p>
                <p className="text-xs text-gray-600 uppercase tracking-widest">Wi-Fi & Tech</p>
                <p className="text-xs text-gray-600 uppercase tracking-widest">Local Recs</p>
                <p className="text-xs text-gray-600 uppercase tracking-widest">Emergency</p>
              </div>
            </div>
            <p className="text-center text-xs text-gray-800 pb-4">© 2025 Nova Stay</p>
          </footer>
        </div>
        <FloatingHelp />
      </main>
      <HostPreview />
    </>
  );
}
