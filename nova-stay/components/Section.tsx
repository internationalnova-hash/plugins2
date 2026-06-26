import { ReactNode } from "react";

interface SectionProps {
  title: string;
  icon?: string;
  children: ReactNode;
  className?: string;
}

export default function Section({ title, icon, children, className = "" }: SectionProps) {
  return (
    <section className={`mb-6 ${className}`}>
      <div className="bg-nova-card border border-nova-border rounded-2xl overflow-hidden">
        <div className="flex items-center gap-3 px-5 py-4 border-b border-nova-border">
          {icon && <span className="text-xl">{icon}</span>}
          <h2 className="text-sm font-semibold tracking-[0.15em] uppercase gold-text">
            {title}
          </h2>
        </div>
        <div className="px-5 py-4">{children}</div>
      </div>
    </section>
  );
}
