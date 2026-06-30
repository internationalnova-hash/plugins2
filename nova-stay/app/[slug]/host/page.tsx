"use client";

import { useEffect } from "react";
import { useRouter, useParams } from "next/navigation";

// Clean alias for the old ?host=1 query trick, scoped to a specific
// property path (e.g. /casanova/host) — flags this device as a host and
// sends it back into that property's app.
export default function HostShortcut() {
  const router = useRouter();
  const params = useParams<{ slug: string }>();
  const slug = typeof params?.slug === "string" ? params.slug : Array.isArray(params?.slug) ? params.slug[0] : undefined;

  useEffect(() => {
    try {
      localStorage.setItem("stayByNova_isHost", "1");
    } catch { /* ignore */ }
    router.replace(slug ? `/${slug}` : "/");
  }, [router, slug]);

  return null;
}
