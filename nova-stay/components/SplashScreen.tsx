"use client";

import { useEffect, useState } from "react";
import SNMonogram from "@/components/SNMonogram";
import theme from "@/config/theme";

const GOLD = theme.gold;
const GOLD_LIGHT = theme.goldBright;

type Stage = "fadeIn" | "shimmer" | "text" | "scaleOut";

export default function SplashScreen({ onDone }: { onDone?: () => void }) {
  const [stage, setStage] = useState<Stage>("fadeIn");

  useEffect(() => {
    const t1 = setTimeout(() => setStage("shimmer"), 500);
    const t2 = setTimeout(() => setStage("text"), 1100);
    const t3 = setTimeout(() => setStage("scaleOut"), 3400);
    const t4 = setTimeout(() => onDone?.(), 4000);
    return () => { clearTimeout(t1); clearTimeout(t2); clearTimeout(t3); clearTimeout(t4); };
  }, [onDone]);

  return (
    <div
      style={{
        minHeight: "100vh",
        background: "#0A0A0A",
        display: "flex",
        alignItems: "center",
        justifyContent: "center",
        opacity: stage === "scaleOut" ? 0 : 1,
        transition: "opacity 0.6s ease",
      }}
    >
      <style>{`
        @keyframes snFadeIn   { from { opacity: 0; transform: scale(0.92); } to { opacity: 1; transform: scale(1); } }
        @keyframes snShimmer  { 0% { background-position: -150% 0; } 100% { background-position: 250% 0; } }
        @keyframes snTextFade { from { opacity: 0; transform: translateY(6px); } to { opacity: 1; transform: translateY(0); } }
        @keyframes snScaleUp  { to { transform: scale(1.08); } }
        .sn-logo-wrap { animation: snFadeIn 0.7s cubic-bezier(0.22,1,0.36,1) both; }
        .sn-logo-scale { animation: snScaleUp 0.6s ease both; }
        .sn-shimmer-overlay {
          position: absolute;
          inset: 0;
          background: linear-gradient(100deg, transparent 35%, rgba(255,255,255,0.55) 50%, transparent 65%);
          background-size: 250% 100%;
          animation: snShimmer 1.1s ease;
          mix-blend-mode: overlay;
        }
        .sn-prep-text { animation: snTextFade 0.6s cubic-bezier(0.22,1,0.36,1) both; }
      `}</style>

      <div style={{ textAlign: "center" }}>
        <div
          className={`sn-logo-wrap ${stage === "scaleOut" ? "sn-logo-scale" : ""}`}
          style={{ position: "relative", width: 72, height: 72, margin: "0 auto 22px", overflow: "hidden", borderRadius: 18 }}
        >
          <div style={{ position: "absolute", inset: 0, display: "flex", alignItems: "center", justifyContent: "center" }}>
            <SNMonogram size={56} id="splash" />
          </div>
          {(stage === "shimmer" || stage === "text" || stage === "scaleOut") && <div className="sn-shimmer-overlay" />}
        </div>

        <div style={{ minHeight: 18 }}>
          {(stage === "text" || stage === "scaleOut") && (
            <p
              className="sn-prep-text"
              style={{
                fontSize: 12,
                letterSpacing: "0.2em",
                textTransform: "uppercase",
                fontWeight: 600,
                background: `linear-gradient(135deg, ${GOLD} 0%, ${GOLD_LIGHT} 50%, ${GOLD} 100%)`,
                WebkitBackgroundClip: "text",
                backgroundClip: "text",
                color: "transparent",
              }}
            >
              Preparing your luxury experience…
            </p>
          )}
        </div>
      </div>
    </div>
  );
}
