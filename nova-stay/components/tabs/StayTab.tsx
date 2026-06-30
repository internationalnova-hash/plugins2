"use client";

import { useState } from "react";
import HouseRules from "@/components/HouseRules";
import Checkout from "@/components/Checkout";
import { propertyConfig } from "@/data/config";

const GOLD = "#C9A84C";
const GOLD_LIGHT = "#E8C97A";

/* ── Premium copy card ─────────────────────────────────────────────────────
   Large, tappable, breathing-room cards — Apple Wallet style.            */

function CopyCard({
  icon,
  label,
  value,
  mono = false,
  hint,
  copyValue,
}: {
  icon: string;
  label: string;
  value: string;
  mono?: boolean;
  hint?: string;
  copyValue?: string;
}) {
  const [copied, setCopied] = useState(false);

  const handleCopy = () => {
    navigator.clipboard.writeText(copyValue ?? value).then(() => {
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    });
  };

  return (
    <button
      className="btn-press card-hover"
      onClick={handleCopy}
      style={{
        width: "100%",
        background: "#111",
        border: `1px solid ${copied ? GOLD + "55" : "#1e1e1e"}`,
        borderRadius: 20,
        padding: "28px 24px",
        textAlign: "left",
        cursor: "pointer",
        marginBottom: 12,
        transition: "border-color 0.25s ease",
        display: "block",
      }}
    >
      {/* Icon + label */}
      <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: 20 }}>
        <div style={{ display: "flex", alignItems: "center", gap: 10 }}>
          <span style={{ fontSize: 22 }}>{icon}</span>
          <span style={{ color: "#6B7280", fontSize: 11, fontWeight: 600, letterSpacing: "0.18em", textTransform: "uppercase" }}>{label}</span>
        </div>
        <span style={{ fontSize: 11, fontWeight: 700, color: copied ? GOLD : "#2d2d2d", letterSpacing: "0.1em", textTransform: "uppercase", transition: "color 0.2s" }}>
          {copied ? "✓ Copied" : "Tap to copy"}
        </span>
      </div>

      {/* Value — large */}
      <p style={{
        color: "#fff",
        fontSize: mono ? 40 : 34,
        fontWeight: 300,
        letterSpacing: mono ? "0.18em" : "-0.01em",
        lineHeight: 1,
        marginBottom: hint ? 14 : 0,
        fontFamily: mono ? "monospace" : "inherit",
      }}>
        {value}
      </p>

      {/* Hint */}
      {hint && <p style={{ color: "#4B5563", fontSize: 12, lineHeight: 1.5 }}>{hint}</p>}
    </button>
  );
}

/* ── WiFi multi-network card ─────────────────────────────────────────────── */

function WifiSection() {
  const [copiedKey, setCopiedKey] = useState<string | null>(null);

  const copy = (val: string, key: string) => {
    navigator.clipboard.writeText(val).then(() => {
      setCopiedKey(key);
      setTimeout(() => setCopiedKey(null), 2000);
    });
  };

  return (
    <div style={{ marginBottom: 12 }}>
      {propertyConfig.wifi.networks.map((net, i) => (
        <div
          key={i}
          style={{ background: "#111", border: "1px solid #1e1e1e", borderRadius: 20, padding: "28px 24px", marginBottom: 12 }}
        >
          <div style={{ display: "flex", alignItems: "center", gap: 10, marginBottom: 22 }}>
            <span style={{ fontSize: 22 }}>📶</span>
            <span style={{ color: "#6B7280", fontSize: 11, fontWeight: 600, letterSpacing: "0.18em", textTransform: "uppercase" }}>
              Wi-Fi · Network {propertyConfig.wifi.networks.length > 1 ? i + 1 : ""}
            </span>
          </div>

          {/* Network name */}
          <button
            className="btn-press"
            onClick={() => copy(net.name, `name-${i}`)}
            style={{ width: "100%", background: "#0d0d0d", border: "1px solid #1e1e1e", borderRadius: 12, padding: "16px 18px", marginBottom: 10, display: "flex", alignItems: "center", justifyContent: "space-between", cursor: "pointer" }}
          >
            <div>
              <p style={{ color: "#4B5563", fontSize: 10, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 4 }}>Network</p>
              <p style={{ color: "#fff", fontSize: 22, fontWeight: 500 }}>{net.name}</p>
            </div>
            <span style={{ fontSize: 11, fontWeight: 700, color: copiedKey === `name-${i}` ? GOLD : "#2d2d2d", letterSpacing: "0.08em", textTransform: "uppercase" }}>
              {copiedKey === `name-${i}` ? "✓ Copied" : "Copy"}
            </span>
          </button>

          {/* Password */}
          <button
            className="btn-press"
            onClick={() => copy(net.password, `pw-${i}`)}
            style={{ width: "100%", background: "#0d0d0d", border: "1px solid #1e1e1e", borderRadius: 12, padding: "16px 18px", display: "flex", alignItems: "center", justifyContent: "space-between", cursor: "pointer" }}
          >
            <div>
              <p style={{ color: "#4B5563", fontSize: 10, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 4 }}>Password</p>
              <p style={{ color: GOLD, fontSize: 22, fontFamily: "monospace", fontWeight: 600 }}>{net.password}</p>
            </div>
            <span style={{ fontSize: 11, fontWeight: 700, color: copiedKey === `pw-${i}` ? GOLD : "#2d2d2d", letterSpacing: "0.08em", textTransform: "uppercase" }}>
              {copiedKey === `pw-${i}` ? "✓ Copied" : "Copy"}
            </span>
          </button>
        </div>
      ))}
    </div>
  );
}

/* ── Expandable info card ───────────────────────────────────────────────── */

function ExpandCard({ icon, label, preview, children }: { icon: string; label: string; preview: string; children: React.ReactNode }) {
  const [open, setOpen] = useState(false);
  return (
    <div style={{ background: "#111", border: "1px solid #1e1e1e", borderRadius: 20, marginBottom: 12, overflow: "hidden" }}>
      <button
        className="btn-press"
        onClick={() => setOpen((o) => !o)}
        style={{ width: "100%", background: "transparent", border: "none", padding: "28px 24px", textAlign: "left", cursor: "pointer", display: "flex", alignItems: "center", justifyContent: "space-between" }}
      >
        <div style={{ display: "flex", alignItems: "center", gap: 12, flex: 1 }}>
          <span style={{ fontSize: 22 }}>{icon}</span>
          <div>
            <p style={{ color: "#6B7280", fontSize: 11, fontWeight: 600, letterSpacing: "0.18em", textTransform: "uppercase", marginBottom: 4 }}>{label}</p>
            <p style={{ color: "#fff", fontSize: 20, fontWeight: 400 }}>{preview}</p>
          </div>
        </div>
        <span style={{ color: "#2d2d2d", fontSize: 18, transform: open ? "rotate(180deg)" : "none", transition: "transform 0.25s ease" }}>›</span>
      </button>
      {open && (
        <div style={{ padding: "0 24px 24px", borderTop: "1px solid #1a1a1a" }}>
          <div style={{ paddingTop: 20 }}>{children}</div>
        </div>
      )}
    </div>
  );
}

/* ── Tab ───────────────────────────────────────────────────────────────── */

export default function StayTab() {
  const { quietHours, driveway, checkout } = propertyConfig;

  return (
    <div style={{ paddingTop: 4 }}>

      {/* Wi-Fi — split cards per network */}
      <WifiSection />

      {/* Door Code */}
      <CopyCard
        icon="🔑"
        label="Front Door"
        value={propertyConfig.doorCode.code}
        mono
        hint={propertyConfig.doorCode.note}
      />

      {/* Quiet Hours — expandable */}
      <ExpandCard icon="🌙" label="Quiet Hours" preview={`Starts at ${quietHours.outdoorCutoff}`}>
        <p style={{ color: "#9CA3AF", fontSize: 14, lineHeight: 1.75 }}>{quietHours.note}</p>
      </ExpandCard>

      {/* Parking — expandable */}
      <ExpandCard icon="🅿️" label="Parking" preview="Driveway available">
        <p style={{ color: "#9CA3AF", fontSize: 14, lineHeight: 1.75 }}>{driveway.note}</p>
      </ExpandCard>

      {/* House Rules — original component, below the premium cards */}
      <div style={{ marginTop: 4 }}>
        <HouseRules />
      </div>

      {/* Checkout */}
      <div style={{ background: "#111", border: "1px solid #1e1e1e", borderRadius: 20, padding: "28px 24px", marginBottom: 12 }}>
        <div style={{ display: "flex", alignItems: "center", gap: 10, marginBottom: 22 }}>
          <span style={{ fontSize: 22 }}>🧳</span>
          <span style={{ color: "#6B7280", fontSize: 11, fontWeight: 600, letterSpacing: "0.18em", textTransform: "uppercase" }}>Checkout</span>
        </div>
        <p style={{ color: "#fff", fontSize: 32, fontWeight: 300, marginBottom: 20 }}>By {checkout.time}</p>
        <div style={{ display: "flex", flexDirection: "column", gap: 12 }}>
          {checkout.steps.map((step, i) => (
            <div key={i} style={{ display: "flex", alignItems: "flex-start", gap: 14 }}>
              <span style={{ color: GOLD, fontSize: 12, fontWeight: 700, minWidth: 18, marginTop: 2 }}>{i + 1}</span>
              <p style={{ color: "#9CA3AF", fontSize: 14, lineHeight: 1.6 }}>{step}</p>
            </div>
          ))}
        </div>
      </div>

    </div>
  );
}
