import Header from "@/components/Header";
import WifiCard from "@/components/WifiCard";
import DoorCode from "@/components/DoorCode";
import TheaterGuide from "@/components/TheaterGuide";
import StudioAddon from "@/components/StudioAddon";
import PoolRules from "@/components/PoolRules";
import HouseRules from "@/components/HouseRules";
import Checkout from "@/components/Checkout";
import EmergencyContacts from "@/components/EmergencyContacts";
import LocalRecs from "@/components/LocalRecs";
import FAQ from "@/components/FAQ";
import QRCodeSection from "@/components/QRCode";
import { propertyConfig } from "@/data/config";

export default function Home() {
  const { welcome } = propertyConfig;

  return (
    <main className="max-w-lg mx-auto px-4 pb-16">
      {/* Header */}
      <Header />

      {/* Welcome */}
      <section className="mb-6">
        <div
          className="rounded-2xl px-5 py-6 text-center"
          style={{
            background: "linear-gradient(135deg, #1A1A1A 0%, #111111 100%)",
            border: "1px solid #2A2A2A",
          }}
        >
          <h2 className="text-lg font-serif font-bold text-white mb-3">
            {welcome.heading}
          </h2>
          <div className="divider-gold mx-auto w-16 mb-3" />
          <p className="text-gray-400 text-sm leading-relaxed">{welcome.message}</p>
          <p className="text-xs mt-4" style={{ color: "#C9A84C" }}>
            — {welcome.hostName}
          </p>
        </div>
      </section>

      {/* Essentials */}
      <WifiCard />
      <DoorCode />

      {/* Amenities */}
      <TheaterGuide />
      <StudioAddon />
      <PoolRules />

      {/* House Rules */}
      <HouseRules />

      {/* Checkout */}
      <Checkout />

      {/* Emergency */}
      <EmergencyContacts />

      {/* Explore */}
      <LocalRecs />

      {/* FAQ */}
      <FAQ />

      {/* QR Code */}
      <QRCodeSection />

      {/* Footer */}
      <footer className="text-center pt-4">
        <div className="divider-gold mx-auto w-24 mb-4" />
        <p className="text-xs text-gray-600 uppercase tracking-widest">
          Nova Stay · Casanova ATL
        </p>
        <p className="text-xs text-gray-700 mt-1">Luxury Guest Experience</p>
      </footer>
    </main>
  );
}
