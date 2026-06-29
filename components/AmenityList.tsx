import Section from "./Section";

interface Category {
  category: string;
  places: { name: string; note: string }[];
}

export default function AmenityList({
  title,
  icon,
  categories,
}: {
  title: string;
  icon: string;
  categories: Category[];
}) {
  return (
    <Section title={title} icon={icon}>
      <div className="space-y-5">
        {categories.map((cat, i) => (
          <div key={i}>
            <p
              className="text-xs font-semibold uppercase tracking-[0.15em] mb-3"
              style={{ color: "#C9A84C" }}
            >
              {cat.category}
            </p>
            <div className="space-y-2">
              {cat.places.map((place, j) => (
                <div
                  key={j}
                  className="flex items-start justify-between bg-nova-dark rounded-xl px-4 py-3 border border-nova-border"
                >
                  <div>
                    <p className="text-white text-sm font-medium">{place.name}</p>
                    <p className="text-gray-500 text-xs mt-0.5">{place.note}</p>
                  </div>
                </div>
              ))}
            </div>
          </div>
        ))}
      </div>
    </Section>
  );
}
