import Section from "./Section";
import { propertyConfig } from "@/data/config";

export default function StudioAddon() {
  const { description, packages, bookingNote } = propertyConfig.studio;

  return (
    <Section title="Recording Studio Add-On" icon="🎙️">
      <p className="text-gray-400 text-sm leading-relaxed mb-4">{description}</p>
      <div className="space-y-3">
        {packages.map((pkg, i) => (
          <div
            key={i}
            className="flex items-center justify-between bg-nova-dark rounded-xl px-4 py-4 border border-nova-border"
          >
            <div>
              <p className="text-white font-semibold text-sm">{pkg.duration}</p>
              <p className="text-gray-500 text-xs mt-0.5">{pkg.includes}</p>
            </div>
            <span
              className="text-xl font-bold font-serif"
              style={{ color: "#C9A84C" }}
            >
              {pkg.price}
            </span>
          </div>
        ))}
      </div>
      <p className="text-xs text-gray-500 mt-4 text-center">{bookingNote}</p>
    </Section>
  );
}
