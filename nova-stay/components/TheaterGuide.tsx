import Section from "./Section";
import { propertyConfig } from "@/data/config";

export default function TheaterGuide() {
  const { steps } = propertyConfig.theater;

  return (
    <Section title="Movie Theater" icon="🎬">
      <ol className="space-y-3">
        {steps.map((step, i) => (
          <li key={i} className="flex gap-4">
            <span
              className="flex-shrink-0 w-7 h-7 rounded-full flex items-center justify-center text-xs font-bold text-nova-black"
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
