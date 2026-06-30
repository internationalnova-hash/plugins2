"use client";

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
  const openInMaps = (name: string) => {
    window.open(`https://www.google.com/maps/search/?api=1&query=${encodeURIComponent(name)}`, "_blank");
  };

  return (
    <Section title={title} icon={icon}>
      <div className="space-y-5">
        {categories.map((cat, i) => (
          <div key={i} className="anim-fade-up anim-delay" style={{ "--delay": `${i * 0.05}s` } as React.CSSProperties}>
            <p
              className="text-xs font-semibold uppercase tracking-[0.15em] mb-3"
              style={{ color: "#C9A84C" }}
            >
              {cat.category}
            </p>
            <div className="space-y-2">
              {cat.places.map((place, j) => (
                <button
                  key={j}
                  className="btn-press card-hover w-full flex items-center justify-between bg-nova-dark rounded-xl px-4 py-3 border border-nova-border text-left"
                  onClick={() => openInMaps(place.name)}
                >
                  <div>
                    <p className="text-white text-sm font-medium">{place.name}</p>
                    <p className="text-gray-500 text-xs mt-0.5">{place.note}</p>
                  </div>
                  <span className="text-xs flex-shrink-0 ml-3" style={{ color: "#3a3a3a" }}>↗</span>
                </button>
              ))}
            </div>
          </div>
        ))}
      </div>
    </Section>
  );
}
