import Section from "./Section";
import { propertyConfig } from "@/data/config";

export default function Checkout() {
  const { time, steps } = propertyConfig.checkout;

  return (
    <Section title="Checkout Instructions" icon="🧳">
      <div
        className="rounded-xl px-4 py-3 mb-4 text-center"
        style={{ backgroundColor: "rgba(201,168,76,0.08)", border: "1px solid #C9A84C33" }}
      >
        <p className="text-xs text-gray-400 uppercase tracking-widest">Checkout Time</p>
        <p className="text-2xl font-bold mt-1" style={{ color: "#C9A84C" }}>{time}</p>
      </div>
      <ol className="space-y-3">
        {steps.map((step, i) => (
          <li key={i} className="flex gap-4">
            <span
              className="flex-shrink-0 w-6 h-6 rounded-full flex items-center justify-center text-xs font-bold text-nova-black"
              style={{ backgroundColor: "#C9A84C" }}
            >
              {i + 1}
            </span>
            <p className="text-gray-300 text-sm leading-relaxed pt-0.5">{step}</p>
          </li>
        ))}
      </ol>
    </Section>
  );
}
