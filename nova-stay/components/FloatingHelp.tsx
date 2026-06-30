"use client";

import { useState } from "react";
import { MessageCircle, X } from "lucide-react";
import { propertyConfig } from "@/data/config";

export default function FloatingHelp() {
  const [open, setOpen] = useState(false);
  const { contacts } = propertyConfig.emergency;

  return (
    <>
      {/* Panel */}
      {open && (
        <div
          className="lux-glass-strong fixed bottom-24 right-4 z-50 p-5 w-72"
        >
          <p
            className="text-xs font-semibold uppercase tracking-widest mb-4"
            style={{ color: "#C9A84C" }}
          >
            Need Help?
          </p>
          <div className="space-y-3">
            {contacts.map((c, i) => (
              <div key={i} className="border-b border-nova-border pb-3 last:border-0 last:pb-0">
                <p className="text-xs text-gray-500">{c.label}</p>
                <p className="text-white text-sm font-medium">{c.value}</p>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Button */}
      <button
        onClick={() => setOpen(!open)}
        className="fixed bottom-6 right-4 z-50 flex flex-col items-center gap-1 active:opacity-80 transition-opacity"
      >
        <div
          className="w-14 h-14 rounded-full flex items-center justify-center shadow-xl"
          style={{
            backgroundColor: "#C9A84C",
            border: "2px solid #E8C97A",
            boxShadow: "0 0 20px rgba(201,168,76,0.4)",
          }}
        >
          {open ? (
            <X size={22} color="#0A0A0A" />
          ) : (
            <MessageCircle size={22} color="#0A0A0A" />
          )}
        </div>
        {!open && (
          <span className="text-xs font-semibold" style={{ color: "#C9A84C" }}>
            NEED HELP?
          </span>
        )}
      </button>
    </>
  );
}
