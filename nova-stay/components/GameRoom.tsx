import Section from "./Section";
import { propertyConfig } from "@/data/config";

export default function GameRoom() {
  const { details } = propertyConfig.gameRoom;

  return (
    <Section title="Game Room" icon="🎮">
      <ul className="space-y-3">
        {details.map((item, i) => (
          <li key={i} className="flex items-start gap-3">
            <span style={{ color: "#C9A84C" }} className="mt-0.5 flex-shrink-0">◆</span>
            <p className="text-gray-300 text-sm leading-relaxed">{item}</p>
          </li>
        ))}
      </ul>
    </Section>
  );
}
