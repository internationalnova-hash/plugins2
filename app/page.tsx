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

export default function Home() {
  return (
    <main className="bg-nova-black min-h-screen">
      {/* Full-screen hero landing */}
      <Hero />

      {/* Guest guide content */}
      <div id="guest-guide" className="max-w-lg mx-auto px-4 pb-24">

        {/* Welcome + quick-access guide cards */}
        <WelcomeSection />

        <div className="divider-gold my-6 mx-4" />

        {/* Essentials */}
        <div id="wifi"><WifiCard /></div>
        <div id="door-code"><DoorCode /></div>

        {/* Amenities */}
        <div id="theater"><TheaterGuide /></div>
        <div id="studio"><StudioAddon /></div>
        <div id="pool"><PoolRules /></div>
        <div id="game-room"><GameRoom /></div>

        {/* House Rules */}
        <HouseRules />

        {/* Checkout */}
        <div id="checkout"><Checkout /></div>

        {/* Emergency */}
        <EmergencyContacts />

        {/* Explore */}
        <LocalRecs />

        {/* FAQ */}
        <FAQ />

        {/* QR Code */}
        <QRCodeSection />

        {/* Footer */}
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

      {/* Floating help button */}
      <FloatingHelp />
    </main>
  );
}
