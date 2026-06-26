"use client";

import { useState } from "react";
import { ChevronDown } from "lucide-react";
import Section from "./Section";
import { propertyConfig } from "@/data/config";

export default function FAQ() {
  const [open, setOpen] = useState<number | null>(null);

  return (
    <Section title="FAQ" icon="💬">
      <div className="space-y-2">
        {propertyConfig.faq.map((item, i) => (
          <div
            key={i}
            className="bg-nova-dark rounded-xl border border-nova-border overflow-hidden"
          >
            <button
              className="w-full flex items-center justify-between px-4 py-3 text-left active:opacity-70 transition-opacity"
              onClick={() => setOpen(open === i ? null : i)}
            >
              <p className="text-white text-sm font-medium pr-4">{item.q}</p>
              <ChevronDown
                size={16}
                className="flex-shrink-0 transition-transform duration-200"
                style={{
                  color: "#C9A84C",
                  transform: open === i ? "rotate(180deg)" : "rotate(0deg)",
                }}
              />
            </button>
            {open === i && (
              <div className="px-4 pb-4 border-t border-nova-border pt-3">
                <p className="text-gray-400 text-sm leading-relaxed">{item.a}</p>
              </div>
            )}
          </div>
        ))}
      </div>
    </Section>
  );
}
