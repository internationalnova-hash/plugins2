"use client";

import EmergencyContacts from "@/components/EmergencyContacts";
import FAQ from "@/components/FAQ";
import Section from "@/components/Section";
import { propertyConfig } from "@/data/config";

const GOLD = "#C9A84C";
const GOLD_LIGHT = "#E8C97A";

export default function SupportTab() {
  return (
    <div className="space-y-0">
      {/* Ask Nova — AI placeholder */}
      <div
        style={{
          background: "rgba(201,168,76,0.04)",
          border: `1px solid ${GOLD}22`,
          borderRadius: 16,
          padding: "24px 20px",
          marginBottom: 6,
          textAlign: "center",
        }}
      >
        <p style={{ color: GOLD, fontSize: 11, letterSpacing: "0.2em", textTransform: "uppercase", marginBottom: 8 }}>
          ✦ Ask Nova
        </p>
        <p style={{ color: "#9CA3AF", fontSize: 14, lineHeight: 1.65, marginBottom: 16 }}>
          Need restaurant recommendations, theater instructions, or anything else? Your AI concierge is coming soon.
        </p>
        <button
          disabled
          style={{
            background: `linear-gradient(135deg, ${GOLD}, ${GOLD_LIGHT})`,
            color: "#0A0A0A",
            border: "none",
            borderRadius: 10,
            padding: "13px 28px",
            fontSize: 12,
            fontWeight: 700,
            letterSpacing: "0.15em",
            textTransform: "uppercase",
            cursor: "default",
            opacity: 0.5,
            width: "100%",
          }}
        >
          Ask Nova — Coming Soon
        </button>
      </div>

      {/* Contact Host */}
      <Section title="Contact Host" icon="🛎️">
        <p className="text-gray-400 text-sm leading-relaxed mb-4">
          For any questions, issues, or special requests, your host is available via the Airbnb app.
        </p>
        <div className="bg-nova-dark border border-nova-border rounded-xl px-4 py-3">
          <p className="text-white text-sm font-semibold">{propertyConfig.welcome.hostName}</p>
          <p className="text-gray-500 text-xs mt-0.5">Contact via Airbnb app</p>
        </div>
      </Section>

      <EmergencyContacts />

      {/* Report Issue */}
      <Section title="Report an Issue" icon="⚠️">
        <p className="text-gray-400 text-sm leading-relaxed">
          If something is broken, not working, or needs attention, please contact the host immediately via the Airbnb app so we can resolve it as quickly as possible.
        </p>
      </Section>

      <FAQ />
    </div>
  );
}
