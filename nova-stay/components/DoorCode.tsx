"use client";

import { useState } from "react";
import { Copy, Check } from "lucide-react";
import Section from "./Section";
import { propertyConfig } from "@/data/config";

export default function DoorCode() {
  const [copied, setCopied] = useState(false);
  const { code, note } = propertyConfig.doorCode;

  const copy = () => {
    navigator.clipboard.writeText(code);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <Section title="Door Code" icon="🔐">
      <button
        onClick={copy}
        className="w-full flex items-center justify-between bg-nova-dark rounded-xl px-5 py-4 border border-nova-border active:opacity-70 transition-opacity mb-3"
      >
        <div>
          <p className="text-xs text-gray-500 uppercase tracking-widest mb-1">Access Code</p>
          <p className="text-3xl font-mono font-bold tracking-[0.2em] gold-text" style={{ color: "#C9A84C" }}>
            {code}
          </p>
        </div>
        <div className="flex flex-col items-center gap-1">
          {copied
            ? <Check size={20} style={{ color: "#C9A84C" }} />
            : <Copy size={20} className="text-gray-500" />}
          <span className="text-xs text-gray-600">{copied ? "Copied!" : "Copy"}</span>
        </div>
      </button>
      <p className="text-xs text-gray-500 leading-relaxed">{note}</p>
    </Section>
  );
}
