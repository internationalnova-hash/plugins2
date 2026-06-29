"use client";

import TheaterGuide from "@/components/TheaterGuide";
import StudioAddon from "@/components/StudioAddon";
import GameRoom from "@/components/GameRoom";
import PoolRules from "@/components/PoolRules";
import Section from "@/components/Section";
import { propertyConfig } from "@/data/config";

export default function EntertainmentTab() {
  const { grill } = propertyConfig;
  return (
    <div className="space-y-0">
      <TheaterGuide />
      <StudioAddon />
      <PoolRules />
      <GameRoom />
      <Section title="Outdoor Lighting" icon="💡">
        <div className="space-y-2">
          {grill.instructions.map((step, i) => (
            <div key={i} className="flex items-start gap-3">
              <span className="text-xs font-bold mt-0.5" style={{ color: "#C9A84C", minWidth: 16 }}>
                {i + 1}
              </span>
              <p className="text-gray-400 text-sm leading-relaxed">{step}</p>
            </div>
          ))}
        </div>
      </Section>
    </div>
  );
}
