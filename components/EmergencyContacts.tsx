import Section from "./Section";
import { propertyConfig } from "@/data/config";

export default function EmergencyContacts() {
  const { contacts } = propertyConfig.emergency;

  return (
    <Section title="Emergency Contacts" icon="📞">
      <div className="space-y-2">
        {contacts.map((c, i) => (
          <div
            key={i}
            className="flex items-center justify-between bg-nova-dark rounded-xl px-4 py-3 border border-nova-border"
          >
            <p className="text-gray-400 text-sm">{c.label}</p>
            <p className="text-white font-semibold text-sm">{c.value}</p>
          </div>
        ))}
      </div>
    </Section>
  );
}
