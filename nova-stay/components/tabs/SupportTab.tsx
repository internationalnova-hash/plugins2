"use client";

import { useState } from "react";
import EmergencyContacts from "@/components/EmergencyContacts";
import FAQ from "@/components/FAQ";
import QRCodeSection from "@/components/QRCode";
import Section from "@/components/Section";
import ExperienceMarketplace from "@/components/ExperienceMarketplace";
import { propertyConfig } from "@/data/config";
import { submitGuestRequest } from "@/lib/propertyStore";
import theme from "@/config/theme";

const GOLD = theme.gold;
const GOLD_LIGHT = theme.goldLight;

function SendRequest() {
  const [message, setMessage] = useState("");
  const [status, setStatus] = useState<"idle" | "sending" | "sent" | "error">("idle");

  const send = async () => {
    if (!message.trim() || status === "sending") return;
    setStatus("sending");
    let guestName = "Guest";
    try {
      const raw = localStorage.getItem("stayByNova_guestInfo");
      if (raw) guestName = JSON.parse(raw).guestName || guestName;
    } catch { /* ignore */ }

    const result = await submitGuestRequest(guestName, message.trim());
    if (result.ok) {
      setStatus("sent");
      setMessage("");
      setTimeout(() => setStatus("idle"), 3000);
    } else {
      setStatus("error");
    }
  };

  return (
    <div style={{ marginTop: 12 }}>
      <textarea
        value={message}
        onChange={(e) => setMessage(e.target.value)}
        placeholder="Send a message directly to your host…"
        rows={3}
        style={{
          width: "100%",
          background: "#0d0d0d",
          border: "1px solid #2a2a2a",
          borderRadius: 8,
          padding: "10px 12px",
          color: "#fff",
          fontSize: 13,
          outline: "none",
          resize: "none",
          marginBottom: 8,
        }}
      />
      <button
        onClick={send}
        disabled={!message.trim() || status === "sending"}
        style={{
          width: "100%",
          background: status === "sent" ? "transparent" : `linear-gradient(135deg, ${GOLD}, ${GOLD_LIGHT})`,
          color: status === "sent" ? GOLD : "#0A0A0A",
          border: status === "sent" ? `1px solid ${GOLD}55` : "none",
          borderRadius: 8,
          padding: "11px",
          fontSize: 12,
          fontWeight: 700,
          letterSpacing: "0.1em",
          textTransform: "uppercase",
          cursor: message.trim() ? "pointer" : "default",
          opacity: status === "sending" ? 0.6 : 1,
        }}
      >
        {status === "sending" ? "Sending…" : status === "sent" ? "✓ Sent to host" : "Send Request"}
      </button>
      {status === "error" && <p style={{ color: "#d96b6b", fontSize: 12, marginTop: 8 }}>Failed to send. Please try again.</p>}
    </div>
  );
}

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

      <ExperienceMarketplace />

      {/* Contact Host */}
      <Section title="Contact Host" icon="🛎️">
        <p className="text-gray-400 text-sm leading-relaxed mb-4">
          For any questions, issues, or special requests, your host is available via the Airbnb app.
        </p>
        <div className="bg-nova-dark border border-nova-border rounded-xl px-4 py-3">
          <p className="text-white text-sm font-semibold">{propertyConfig.welcome.hostName}</p>
          <p className="text-gray-500 text-xs mt-0.5">Contact via Airbnb app</p>
        </div>
        <SendRequest />
      </Section>

      <EmergencyContacts />

      {/* Report Issue */}
      <Section title="Report an Issue" icon="⚠️">
        <p className="text-gray-400 text-sm leading-relaxed">
          If something is broken, not working, or needs attention, please contact the host immediately via the Airbnb app so we can resolve it as quickly as possible.
        </p>
      </Section>

      <FAQ />
      <QRCodeSection />
    </div>
  );
}
