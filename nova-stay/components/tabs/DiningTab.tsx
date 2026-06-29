"use client";

import AmenityList from "@/components/AmenityList";
import { propertyConfig } from "@/data/config";

export default function DiningTab() {
  return <AmenityList title="Dining & Nightlife" icon="🍽️" categories={propertyConfig.dining} />;
}
