"use client";

import WifiCard from "@/components/WifiCard";
import DoorCode from "@/components/DoorCode";
import HouseRules from "@/components/HouseRules";
import Checkout from "@/components/Checkout";
import Section from "@/components/Section";
import { propertyConfig } from "@/data/config";

export default function StayTab() {
  const { quietHours } = propertyConfig;
  return (
    <div className="space-y-0">
      <WifiCard />
      <DoorCode />
      <HouseRules />
      <Checkout />
      <Section title="Quiet Hours" icon="🌙">
        <p className="text-gray-400 text-sm leading-relaxed">{quietHours.note}</p>
        <div className="mt-3 flex items-center gap-2">
          <span className="status-pill available">Outdoor cutoff {quietHours.outdoorCutoff}</span>
        </div>
      </Section>
      <Section title="Parking" icon="🅿️">
        <p className="text-gray-400 text-sm leading-relaxed">{propertyConfig.driveway.note}</p>
      </Section>
    </div>
  );
}
