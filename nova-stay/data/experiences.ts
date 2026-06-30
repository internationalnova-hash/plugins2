// ─────────────────────────────────────────────────────────────────────────────
// StayByNova — Experience Marketplace
//
// Config-driven placeholder catalog. Each entry is purely presentational
// until booking/payment is wired up — adding a property's real experiences
// later is a matter of editing this array, not the UI.
// ─────────────────────────────────────────────────────────────────────────────

export interface Experience {
  id: string;
  icon: "Mic2" | "ChefHat" | "Car" | "Thermometer" | "Clock" | "Camera" | "Video" | "Flower2";
  title: string;
  description: string;
  priceFrom: string;
}

export const EXPERIENCES: Experience[] = [
  { id: "recording-studio", icon: "Mic2",        title: "Recording Studio",  description: "Book private studio time during your stay.",     priceFrom: "From $75/hr" },
  { id: "private-chef",     icon: "ChefHat",      title: "Private Chef",     description: "A curated in-home dining experience.",            priceFrom: "From $150"   },
  { id: "airport-pickup",   icon: "Car",          title: "Airport Pickup",   description: "Arrive in comfort with a private driver.",        priceFrom: "From $60"    },
  { id: "pool-heating",     icon: "Thermometer",  title: "Pool Heating",     description: "Warm the pool for your dates, on request.",       priceFrom: "From $40"    },
  { id: "late-checkout",    icon: "Clock",        title: "Late Checkout",    description: "Extend your stay through the afternoon.",         priceFrom: "From $50"    },
  { id: "photography",      icon: "Camera",       title: "Photography",     description: "Professional photos around Casanova ATL.",        priceFrom: "From $200"   },
  { id: "content-creation", icon: "Video",        title: "Content Creation", description: "Studio-ready filming for creators.",              priceFrom: "From $250"   },
  { id: "luxury-decor",     icon: "Flower2",      title: "Luxury Decor",     description: "Custom florals and decor for the occasion.",      priceFrom: "From $120"   },
];
