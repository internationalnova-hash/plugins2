import Section from "./Section";
import { propertyConfig } from "@/data/config";

export default function HouseRules() {
  const { quietHours, driveway, grill } = propertyConfig;

  return (
    <>
      {/* Quiet Hours */}
      <Section title="Quiet Hours" icon="🌙">
        <div className="flex items-center gap-4 mb-3">
          <div
            className="rounded-xl px-5 py-3 text-center"
            style={{ backgroundColor: "rgba(201,168,76,0.1)", border: "1px solid #C9A84C33" }}
          >
            <p className="text-xs text-gray-400 uppercase tracking-widest mb-1">Outdoor Cutoff</p>
            <p className="text-2xl font-bold" style={{ color: "#C9A84C" }}>
              {quietHours.outdoorCutoff}
            </p>
          </div>
          <p className="text-gray-400 text-sm leading-relaxed flex-1">{quietHours.note}</p>
        </div>
      </Section>

      {/* Driveway */}
      <Section title="Driveway & Front of Home" icon="🚗">
        <p className="text-gray-300 text-sm leading-relaxed">{driveway.note}</p>
      </Section>

      {/* Grill & Lights */}
      <Section title="Grill & Outdoor Lights" icon="🔥">
        <ul className="space-y-3">
          {grill.instructions.map((item, i) => (
            <li key={i} className="flex items-start gap-3">
              <span style={{ color: "#C9A84C" }} className="mt-0.5 flex-shrink-0">◆</span>
              <p className="text-gray-300 text-sm leading-relaxed">{item}</p>
            </li>
          ))}
        </ul>
      </Section>
    </>
  );
}
