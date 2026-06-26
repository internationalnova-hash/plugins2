import { propertyConfig } from "@/data/config";

export default function Header() {
  const { brand, welcome } = propertyConfig;

  return (
    <header className="relative text-center px-6 pt-12 pb-8">
      {/* Logo mark */}
      <div className="inline-flex items-center justify-center w-16 h-16 rounded-full border border-gold mb-4"
           style={{ borderColor: "#C9A84C" }}>
        <span className="text-2xl font-serif gold-text font-bold">N</span>
      </div>

      <p className="text-xs tracking-[0.3em] uppercase text-gray-400 mb-1">
        {brand.name}
      </p>

      <h1 className="text-3xl font-serif font-bold text-white mb-2">
        {brand.property}
      </h1>

      <div className="divider-gold my-4 mx-auto w-32" />

      <p className="text-gray-400 text-sm leading-relaxed max-w-xs mx-auto">
        {brand.tagline}
      </p>
    </header>
  );
}
