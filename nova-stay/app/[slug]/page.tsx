"use client";

import { useParams } from "next/navigation";
import StayApp from "@/components/StayApp";

export default function PropertyPage() {
  const params = useParams<{ slug: string }>();
  const slug = typeof params?.slug === "string" ? params.slug : Array.isArray(params?.slug) ? params.slug[0] : undefined;

  return <StayApp propertySlug={slug} />;
}
