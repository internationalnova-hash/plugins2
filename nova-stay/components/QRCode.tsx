"use client";

import { QRCodeSVG } from "qrcode.react";
import Section from "./Section";
import { propertyConfig } from "@/data/config";

export default function QRCodeSection() {
  const { guideUrl, property } = propertyConfig.brand;

  return (
    <Section title="Share This Guide" icon="📱">
      <div className="flex flex-col items-center gap-4">
        <div className="bg-white rounded-2xl p-4">
          <QRCodeSVG
            value={guideUrl}
            size={160}
            bgColor="#ffffff"
            fgColor="#0A0A0A"
            level="M"
          />
        </div>
        <div className="text-center">
          <p className="text-gray-400 text-sm">Scan to open the guest guide</p>
          <p className="text-gray-600 text-xs mt-1">{guideUrl}</p>
        </div>
      </div>
    </Section>
  );
}
