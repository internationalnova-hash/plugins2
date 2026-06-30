"use client";

const GOLD = "#C9A84C";
const GOLD_LIGHT = "#E8C97A";

export default function SplashScreen() {
  return (
    <div
      style={{
        minHeight: "100vh",
        background: "#0A0A0A",
        display: "flex",
        alignItems: "center",
        justifyContent: "center",
      }}
    >
      <style>{`
        @keyframes splashRing { to { transform: rotate(360deg); } }
        @keyframes splashPulse {
          0%, 100% { opacity: 0.55; transform: scale(1); }
          50%      { opacity: 1;    transform: scale(1.04); }
        }
        @keyframes splashFade {
          from { opacity: 0; transform: translateY(8px); }
          to   { opacity: 1; transform: translateY(0); }
        }
        .splash-mark  { animation: splashPulse 2.2s ease-in-out infinite; }
        .splash-word  { animation: splashFade 0.7s cubic-bezier(0.22,1,0.36,1) both; animation-delay: 0.15s; }
      `}</style>

      <div style={{ textAlign: "center" }}>
        <div
          className="splash-mark"
          style={{
            width: 56,
            height: 56,
            margin: "0 auto 18px",
            position: "relative",
          }}
        >
          <div
            style={{
              position: "absolute",
              inset: 0,
              borderRadius: "50%",
              border: `1.5px solid ${GOLD}`,
              borderTopColor: "transparent",
              animation: "splashRing 1.1s linear infinite",
            }}
          />
          <div
            style={{
              position: "absolute",
              inset: 0,
              display: "flex",
              alignItems: "center",
              justifyContent: "center",
              fontFamily: "Georgia, 'Times New Roman', serif",
              fontWeight: 700,
              fontSize: 22,
              color: GOLD,
            }}
          >
            S
          </div>
        </div>

        <div className="splash-word" style={{ display: "flex", alignItems: "center", gap: 10, justifyContent: "center" }}>
          <div style={{ height: 1, width: 20, background: `linear-gradient(to right, transparent, ${GOLD})` }} />
          <p
            style={{
              fontSize: 13,
              letterSpacing: "0.3em",
              textTransform: "uppercase",
              fontWeight: 600,
              background: `linear-gradient(135deg, ${GOLD} 0%, ${GOLD_LIGHT} 50%, ${GOLD} 100%)`,
              WebkitBackgroundClip: "text",
              backgroundClip: "text",
              color: "transparent",
            }}
          >
            StayByNova
          </p>
          <div style={{ height: 1, width: 20, background: `linear-gradient(to left, transparent, ${GOLD})` }} />
        </div>
      </div>
    </div>
  );
}
