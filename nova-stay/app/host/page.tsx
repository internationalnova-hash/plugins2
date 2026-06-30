"use client";

import { useEffect } from "react";
import { useRouter } from "next/navigation";

// Clean alias for the old ?host=1 query trick — visiting /host flags this
// device as a host and sends it into the normal app, where StayApp picks
// the flag up from localStorage and shows the Host View button.
export default function HostShortcut() {
  const router = useRouter();

  useEffect(() => {
    try {
      localStorage.setItem("stayByNova_isHost", "1");
    } catch { /* ignore */ }
    router.replace("/");
  }, [router]);

  return null;
}
