"use client";

import { useState } from "react";
import { Copy, Check, Wifi } from "lucide-react";
import Section from "./Section";
import { propertyConfig } from "@/data/config";

export default function WifiCard() {
  const [copied, setCopied] = useState<string | null>(null);

  const copy = (text: string, key: string) => {
    navigator.clipboard.writeText(text);
    setCopied(key);
    setTimeout(() => setCopied(null), 2000);
  };

  return (
    <Section title="Wi-Fi Access" icon="📶">
      <div className="space-y-4">
        {propertyConfig.wifi.networks.map((net, i) => (
          <div key={i} className="bg-nova-dark rounded-xl p-4 border border-nova-border">
            <div className="flex items-center justify-between mb-3">
              <div className="flex items-center gap-2">
                <Wifi size={14} className="gold-text" style={{ color: "#C9A84C" }} />
                <span className="text-xs text-gray-400 uppercase tracking-widest">Network</span>
              </div>
            </div>

            <button
              onClick={() => copy(net.name, `name-${i}`)}
              className="w-full flex items-center justify-between bg-nova-card rounded-lg px-4 py-3 mb-2 border border-nova-border active:opacity-70 transition-opacity"
            >
              <span className="text-white font-medium">{net.name}</span>
              {copied === `name-${i}`
                ? <Check size={16} style={{ color: "#C9A84C" }} />
                : <Copy size={16} className="text-gray-500" />}
            </button>

            <button
              onClick={() => copy(net.password, `pw-${i}`)}
              className="w-full flex items-center justify-between bg-nova-card rounded-lg px-4 py-3 border border-nova-border active:opacity-70 transition-opacity"
            >
              <span className="text-white font-mono">{net.password}</span>
              {copied === `pw-${i}`
                ? <Check size={16} style={{ color: "#C9A84C" }} />
                : <Copy size={16} className="text-gray-500" />}
            </button>

            <p className="text-xs text-gray-600 mt-2 text-center">Tap password to copy</p>
          </div>
        ))}
      </div>
    </Section>
  );
}
