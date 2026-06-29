"use client";

import AmenityList from "@/components/AmenityList";
import { propertyConfig } from "@/data/config";

export default function ExploreTab() {
  return <AmenityList title="Explore Atlanta" icon="📍" categories={propertyConfig.explore} />;
}
