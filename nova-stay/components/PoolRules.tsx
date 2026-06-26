import Section from "./Section";
import { propertyConfig } from "@/data/config";

export default function PoolRules() {
  const { rules } = propertyConfig.pool;

  return (
    <Section title="Pool & Backyard" icon="🏊">
      <ul className="space-y-3">
        {rules.map((rule, i) => (
          <li key={i} className="flex items-start gap-3">
            <span style={{ color: "#C9A84C" }} className="mt-0.5 flex-shrink-0">◆</span>
            <p className="text-gray-300 text-sm leading-relaxed">{rule}</p>
          </li>
        ))}
      </ul>
    </Section>
  );
}
