"use client";

import { useState } from "react";
import { findBooking, type Booking } from "@/lib/bookingsStore";

const GOLD = "#C9A84C";
const GOLD_LIGHT = "#E8C97A";
const BG = "#0A0A0A";
const CARD = "#111111";

// ─── Shared primitives ───────────────────────────────────────────────────────

function ProgressBar({ step, total }: { step: number; total: number }) {
  const pct = Math.round((step / total) * 100);
  return (
    <div style={{ marginBottom: 36 }}>
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline", marginBottom: 10 }}>
        <span style={{ color: "#4B5563", fontSize: 10, letterSpacing: "0.2em", textTransform: "uppercase" }}>
          Guest Readiness
        </span>
        <span style={{ color: GOLD, fontSize: 13, fontWeight: 700, letterSpacing: "0.05em" }}>{pct}%</span>
      </div>
      <div style={{ width: "100%", height: 4, background: "#1a1a1a", borderRadius: 2, overflow: "hidden" }}>
        <div
          style={{
            height: "100%",
            width: `${pct}%`,
            background: `linear-gradient(90deg, ${GOLD}, ${GOLD_LIGHT})`,
            borderRadius: 2,
            transition: "width 0.5s cubic-bezier(0.22, 1, 0.36, 1)",
            boxShadow: `0 0 8px ${GOLD}66`,
          }}
        />
      </div>
      <p style={{ color: "#2d2d2d", fontSize: 11, marginTop: 8, letterSpacing: "0.1em" }}>
        Step {step} of {total}
      </p>
    </div>
  );
}

function StepLabel({ step, total }: { step: number; total: number }) {
  return (
    <p style={{ color: GOLD, fontSize: 11, letterSpacing: "0.2em", textTransform: "uppercase", marginBottom: 28 }}>
      Step {step} of {total}
    </p>
  );
}

function Heading({ children }: { children: React.ReactNode }) {
  return (
    <h2 style={{ fontFamily: "Georgia, serif", fontSize: "clamp(1.5rem, 5vw, 2rem)", color: "#fff", fontWeight: 700, lineHeight: 1.25, marginBottom: 8 }}>
      {children}
    </h2>
  );
}

function Sub({ children }: { children: React.ReactNode }) {
  return <p style={{ color: "#9CA3AF", fontSize: 14, lineHeight: 1.65, marginBottom: 0 }}>{children}</p>;
}

function GoldBtn({ onClick, disabled, children }: { onClick: () => void; disabled?: boolean; children: React.ReactNode }) {
  return (
    <button
      onClick={onClick}
      disabled={disabled}
      style={{
        background: disabled ? "#1e1e1e" : `linear-gradient(135deg, ${GOLD} 0%, ${GOLD_LIGHT} 50%, ${GOLD} 100%)`,
        color: disabled ? "#555" : BG,
        border: "none",
        borderRadius: 8,
        padding: "16px 32px",
        fontSize: 13,
        fontWeight: 700,
        letterSpacing: "0.18em",
        textTransform: "uppercase" as const,
        cursor: disabled ? "default" : "pointer",
        width: "100%",
        minHeight: 52,
        boxShadow: disabled ? "none" : "0 4px 20px rgba(201,168,76,0.3)",
        transition: "opacity 0.2s",
      }}
    >
      {children}
    </button>
  );
}

function BackBtn({ onClick }: { onClick: () => void }) {
  return (
    <button
      onClick={onClick}
      style={{
        background: "transparent",
        color: "#6B7280",
        border: "1px solid #222",
        borderRadius: 8,
        padding: "13px 24px",
        fontSize: 13,
        fontWeight: 500,
        cursor: "pointer",
        width: "100%",
        marginTop: 10,
      }}
    >
      ← Back
    </button>
  );
}

function Stepper({ value, min, max, onChange, label }: { value: number; min: number; max: number; onChange: (v: number) => void; label: string }) {
  return (
    <div>
      <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", textAlign: "center", marginBottom: 24 }}>{label}</p>
      <div style={{ display: "flex", alignItems: "center", justifyContent: "center", gap: 28 }}>
        <button
          onClick={() => onChange(Math.max(min, value - 1))}
          style={{ width: 56, height: 56, border: `1.5px solid ${value > min ? GOLD : "#333"}`, borderRadius: 10, background: "transparent", color: value > min ? GOLD : "#444", fontSize: 26, cursor: value > min ? "pointer" : "default", display: "flex", alignItems: "center", justifyContent: "center", transition: "all 0.2s" }}
        >
          −
        </button>
        <span style={{ color: "#fff", fontSize: 48, fontWeight: 700, minWidth: 64, textAlign: "center", fontFamily: "Georgia, serif", lineHeight: 1 }}>
          {value}
        </span>
        <button
          onClick={() => onChange(Math.min(max, value + 1))}
          style={{ width: 56, height: 56, border: `1.5px solid ${value < max ? GOLD : "#333"}`, borderRadius: 10, background: "transparent", color: value < max ? GOLD : "#444", fontSize: 26, cursor: value < max ? "pointer" : "default", display: "flex", alignItems: "center", justifyContent: "center", transition: "all 0.2s" }}
        >
          +
        </button>
      </div>
    </div>
  );
}

// ─── Got It card (acknowledgment step) ──────────────────────────────────────

function GuidelineCard({ icon, title, body, onGotIt }: { icon: string; title: string; body: string; onGotIt: () => void }) {
  return (
    <div style={{ minHeight: "100vh", background: BG, display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", padding: "40px 24px" }}>
      <div
        style={{
          maxWidth: 420,
          width: "100%",
          background: CARD,
          border: `1px solid #222`,
          borderRadius: 20,
          padding: "48px 36px 40px",
          textAlign: "center",
          boxShadow: "0 24px 64px rgba(0,0,0,0.6)",
        }}
      >
        {/* Gold top accent */}
        <div style={{ width: 40, height: 3, background: `linear-gradient(90deg, ${GOLD}, ${GOLD_LIGHT})`, borderRadius: 2, margin: "0 auto 32px" }} />

        <div style={{ fontSize: 44, marginBottom: 24 }}>{icon}</div>

        <h2 style={{ fontFamily: "Georgia, serif", fontSize: "1.6rem", color: "#fff", fontWeight: 700, marginBottom: 20, lineHeight: 1.2 }}>
          {title}
        </h2>

        <p style={{ color: "#9CA3AF", fontSize: 15, lineHeight: 1.75, marginBottom: 40 }}>{body}</p>

        <button
          onClick={onGotIt}
          style={{
            background: `linear-gradient(135deg, ${GOLD} 0%, ${GOLD_LIGHT} 50%, ${GOLD} 100%)`,
            color: BG,
            border: "none",
            borderRadius: 8,
            padding: "16px 40px",
            fontSize: 13,
            fontWeight: 700,
            letterSpacing: "0.18em",
            textTransform: "uppercase" as const,
            cursor: "pointer",
            width: "100%",
            minHeight: 52,
            boxShadow: "0 4px 20px rgba(201,168,76,0.3)",
          }}
        >
          Got It
        </button>
      </div>
    </div>
  );
}

// ─── Local Preview teaser ─────────────────────────────────────────────────────

function LocalPreview({ onNext, onBack }: { onNext: () => void; onBack: () => void }) {
  const categories = [
    { emoji: "🍽️", label: "Best Restaurants", note: "From upscale steakhouses to ATL staples" },
    { emoji: "🎬", label: "Movie Studios", note: "Atlanta is the Hollywood of the South" },
    { emoji: "🎤", label: "Live Music", note: "Jazz, hip-hop & everything in between" },
    { emoji: "🏊", label: "Relax by the Pool", note: "Your private luxury pool awaits" },
    { emoji: "☕", label: "Coffee Nearby", note: "Specialty cafés minutes from the door" },
  ];
  return (
    <div style={{ minHeight: "100vh", background: BG, display: "flex", flexDirection: "column", justifyContent: "center", padding: "60px 24px 40px", maxWidth: 480, margin: "0 auto", width: "100%" }}>
      <ProgressBar step={9} total={10} />
      <StepLabel step={9} total={10} />
      <div style={{ marginBottom: 8 }}>
        <span style={{ fontSize: 11, letterSpacing: "0.25em", color: GOLD, textTransform: "uppercase" }}>✨ Curated Experiences</span>
      </div>
      <Heading>What&apos;s Waiting for You</Heading>
      <p style={{ color: "#9CA3AF", fontSize: 14, lineHeight: 1.65, marginBottom: 28 }}>
        Your full guide includes hand-picked Atlanta recommendations — here&apos;s a preview.
      </p>
      <div style={{ display: "flex", flexDirection: "column", gap: 10, marginBottom: 36 }}>
        {categories.map((c) => (
          <div
            key={c.label}
            style={{ background: CARD, border: "1px solid #1e1e1e", borderRadius: 12, padding: "14px 18px", display: "flex", gap: 16, alignItems: "center" }}
          >
            <span style={{ fontSize: 26, flexShrink: 0 }}>{c.emoji}</span>
            <div>
              <p style={{ color: "#fff", fontSize: 14, fontWeight: 600, marginBottom: 2 }}>{c.label}</p>
              <p style={{ color: "#6B7280", fontSize: 12 }}>{c.note}</p>
            </div>
            <span style={{ marginLeft: "auto", color: "#2a2a2a", fontSize: 16 }}>›</span>
          </div>
        ))}
      </div>
      <div style={{ display: "flex", flexDirection: "column", gap: 10 }}>
        <GoldBtn onClick={onNext}>Unlock Full Guide →</GoldBtn>
        <BackBtn onClick={onBack} />
      </div>
    </div>
  );
}

// ─── Animation style ─────────────────────────────────────────────────────────

const ANIM_STYLE = `
  @keyframes slideUpFade {
    from { opacity: 0; transform: translateY(28px); }
    to   { opacity: 1; transform: translateY(0);    }
  }
  .step-enter { animation: slideUpFade 0.45s cubic-bezier(0.22, 1, 0.36, 1) both; }
`;

function Animated({ stepKey, children }: { stepKey: number | string; children: React.ReactNode }) {
  return (
    <>
      <style>{ANIM_STYLE}</style>
      <div key={stepKey} className="step-enter" style={{ width: "100%" }}>
        {children}
      </div>
    </>
  );
}

// ─── Confirmation code lookup ────────────────────────────────────────────────

function CodeEntry({ onFound }: { onFound: (booking: Booking) => void }) {
  const [code, setCode] = useState("");
  const [error, setError] = useState<string | null>(null);

  const submit = () => {
    const match = findBooking(code);
    if (match) {
      setError(null);
      onFound(match);
    } else {
      setError("We couldn't find that confirmation code. Please double-check it or message your host.");
    }
  };

  return (
    <div style={{ minHeight: "100vh", background: BG, display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", padding: "40px 24px", textAlign: "center" }}>
      <div style={{ maxWidth: 400, width: "100%" }}>
        <div style={{ fontSize: 11, letterSpacing: "0.3em", color: GOLD, textTransform: "uppercase", marginBottom: 20 }}>Nova Stay</div>
        <h1 style={{ fontFamily: "Georgia, serif", fontSize: "clamp(1.8rem, 6vw, 2.4rem)", color: "#fff", fontWeight: 700, lineHeight: 1.2, marginBottom: 12 }}>
          Confirm Your<br /><span style={{ color: GOLD }}>Reservation</span>
        </h1>
        <p style={{ color: "#6B7280", fontSize: 14, marginBottom: 36, lineHeight: 1.6 }}>
          Enter the confirmation code from your Airbnb booking to unlock your personalized guide.
        </p>
        <input
          value={code}
          onChange={(e) => setCode(e.target.value)}
          onKeyDown={(e) => e.key === "Enter" && submit()}
          placeholder="Confirmation Code"
          autoCapitalize="characters"
          style={{
            width: "100%",
            background: CARD,
            border: `1.5px solid ${error ? "#7a2f2f" : "#222"}`,
            borderRadius: 10,
            padding: "16px 18px",
            color: "#fff",
            fontSize: 16,
            letterSpacing: "0.05em",
            textAlign: "center",
            marginBottom: error ? 12 : 28,
            outline: "none",
          }}
        />
        {error && <p style={{ color: "#d96b6b", fontSize: 13, lineHeight: 1.5, marginBottom: 28 }}>{error}</p>}
        <GoldBtn onClick={submit} disabled={!code.trim()}>Unlock My Guide →</GoldBtn>
      </div>
    </div>
  );
}

// ─── Main flow ───────────────────────────────────────────────────────────────

export default function GuestReadinessFlow({ onComplete }: { onComplete: () => void }) {
  const [booking, setBooking] = useState<Booking | null>(null);
  const [step, setStep] = useState(0);
  const [overnightGuests, setOvernightGuests] = useState(0);
  const [daytimeVisitors, setDaytimeVisitors] = useState(0);
  const [vehicles, setVehicles] = useState(1);

  const TOTAL = 10;
  const next = () => setStep((s) => s + 1);
  const back = () => setStep((s) => Math.max(0, s - 1));

  if (!booking) {
    return (
      <CodeEntry
        onFound={(match) => {
          setBooking(match);
          setOvernightGuests(match.bookedGuests);
        }}
      />
    );
  }

  const reservation = booking;

  const finish = () => {
    const data = {
      guestName: reservation.guestName,
      checkIn: reservation.checkIn,
      checkOut: reservation.checkOut,
      nights: reservation.nights,
      overnightGuests,
      daytimeVisitors,
      vehicles,
      parkingAck: true,
      quietHoursAck: true,
      registeredGuestsAck: true,
      completedAt: new Date().toISOString(),
      readinessPercent: 100,
    };
    localStorage.setItem("novaStay_guestInfo", JSON.stringify(data));
    onComplete();
  };

  const wrap = (stepNum: number, content: React.ReactNode, canContinue = true, onNext?: () => void) => (
    <Animated stepKey={stepNum}>
      <div style={{ minHeight: "100vh", background: BG, display: "flex", flexDirection: "column", justifyContent: "center", padding: "60px 24px 40px", maxWidth: 480, margin: "0 auto", width: "100%" }}>
        <ProgressBar step={stepNum} total={TOTAL} />
        {content}
        <div style={{ marginTop: 36, display: "flex", flexDirection: "column", gap: 10 }}>
          <GoldBtn onClick={onNext ?? next} disabled={!canContinue}>Continue →</GoldBtn>
          {stepNum > 1 && <BackBtn onClick={back} />}
        </div>
      </div>
    </Animated>
  );

  // Step 0 — Welcome
  if (step === 0) {
    return (
      <Animated stepKey={0}>
      <div style={{ minHeight: "100vh", background: BG, display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", padding: "40px 24px", textAlign: "center" }}>
        <div style={{ maxWidth: 400, width: "100%" }}>
          <div style={{ fontSize: 11, letterSpacing: "0.3em", color: GOLD, textTransform: "uppercase", marginBottom: 20 }}>Nova Stay</div>
          <h1 style={{ fontFamily: "Georgia, serif", fontSize: "clamp(2rem, 7vw, 2.8rem)", color: "#fff", fontWeight: 700, lineHeight: 1.15, marginBottom: 12 }}>
            Welcome,<br />
            <span style={{ color: GOLD }}>{reservation.guestName}</span>
          </h1>
          <p style={{ color: "#6B7280", fontSize: 14, marginBottom: 48, lineHeight: 1.6 }}>
            Before we open your luxury guide,<br />let&apos;s take 90 seconds to set up your stay.
          </p>
          <div style={{ display: "flex", flexDirection: "column", gap: 10, marginBottom: 48, textAlign: "left" }}>
            {["Confirm your reservation", "Tell us who's joining you", "Acknowledge house guidelines", "Unlock your full guide"].map((item, i) => (
              <div key={i} style={{ display: "flex", alignItems: "center", gap: 12 }}>
                <span style={{ color: GOLD, fontSize: 12 }}>✦</span>
                <span style={{ color: "#9CA3AF", fontSize: 14 }}>{item}</span>
              </div>
            ))}
          </div>
          <GoldBtn onClick={next}>Begin Check-In</GoldBtn>
        </div>
      </div>
      </Animated>
    );
  }

  // Step 1 — Reservation (pre-filled, guest confirms)
  if (step === 1) {
    return (
      <Animated stepKey={1}>
      <div style={{ minHeight: "100vh", background: BG, display: "flex", flexDirection: "column", justifyContent: "center", padding: "60px 24px 40px", maxWidth: 480, margin: "0 auto", width: "100%" }}>
        <ProgressBar step={1} total={TOTAL} />
        <StepLabel step={1} total={TOTAL} />
        <Heading>Your Reservation</Heading>
        <Sub>Pulled from your Airbnb booking — confirm everything looks right.</Sub>

        <div style={{ background: CARD, border: "1px solid #1e1e1e", borderRadius: 16, padding: "28px 24px", marginTop: 32, marginBottom: 40 }}>
          {/* Dates */}
          <div style={{ display: "flex", justifyContent: "space-between", paddingBottom: 20, marginBottom: 20, borderBottom: "1px solid #1e1e1e" }}>
            <div>
              <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.12em", textTransform: "uppercase", marginBottom: 6 }}>Check-In</p>
              <p style={{ color: "#fff", fontSize: 18, fontWeight: 600, fontFamily: "Georgia, serif" }}>{reservation.checkIn}</p>
            </div>
            <div style={{ color: "#333", fontSize: 20, alignSelf: "center" }}>→</div>
            <div style={{ textAlign: "right" }}>
              <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.12em", textTransform: "uppercase", marginBottom: 6 }}>Check-Out</p>
              <p style={{ color: "#fff", fontSize: 18, fontWeight: 600, fontFamily: "Georgia, serif" }}>{reservation.checkOut}</p>
            </div>
          </div>

          {/* Nights + guests */}
          <div style={{ display: "flex", justifyContent: "space-around" }}>
            <div style={{ textAlign: "center" }}>
              <p style={{ color: GOLD, fontSize: 28, fontWeight: 700, fontFamily: "Georgia, serif" }}>{reservation.nights}</p>
              <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.1em", textTransform: "uppercase" }}>Nights</p>
            </div>
            <div style={{ width: 1, background: "#1e1e1e" }} />
            <div style={{ textAlign: "center" }}>
              <p style={{ color: GOLD, fontSize: 28, fontWeight: 700, fontFamily: "Georgia, serif" }}>{reservation.bookedGuests}</p>
              <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.1em", textTransform: "uppercase" }}>Guests</p>
            </div>
          </div>
        </div>

        <div style={{ display: "flex", flexDirection: "column", gap: 10 }}>
          <GoldBtn onClick={next}>Looks Right — Continue</GoldBtn>
        </div>
      </div>
      </Animated>
    );
  }

  // Step 2 — Who's Staying (overnight)
  if (step === 2) {
    return wrap(2,
      <>
        <Heading>Who&apos;s Staying?</Heading>
        <Sub>Total guests sleeping overnight, including yourself.</Sub>
        <div style={{ marginTop: 40 }}>
          <Stepper value={overnightGuests} min={1} max={16} onChange={setOvernightGuests} label="Overnight Guests" />
        </div>
      </>
    );
  }

  // Step 3 — Who's Visiting (daytime)
  if (step === 3) {
    return wrap(3,
      <>
        <Heading>Who&apos;s Visiting?</Heading>
        <Sub>Guests who will visit but not sleep here. Enter 0 if none.</Sub>
        <div style={{ marginTop: 40 }}>
          <Stepper value={daytimeVisitors} min={0} max={20} onChange={setDaytimeVisitors} label="Daytime Visitors" />
        </div>
      </>
    );
  }

  // Step 4 — Vehicles
  if (step === 4) {
    return wrap(4,
      <>
        <Heading>Vehicles</Heading>
        <Sub>How many vehicles will be parked at the property?</Sub>
        <div style={{ marginTop: 40 }}>
          <Stepper value={vehicles} min={0} max={8} onChange={setVehicles} label="Vehicles" />
        </div>
      </>
    );
  }

  // Step 5 — Guideline: Parking
  if (step === 5) {
    return (
      <Animated stepKey={5}>
        <GuidelineCard icon="🅿️" title="Parking" body="Only registered vehicles may park in the driveway or designated spaces. Please do not block neighboring driveways or park on the lawn. Unauthorized parking may result in towing." onGotIt={next} />
      </Animated>
    );
  }

  // Step 6 — Guideline: Quiet Hours
  if (step === 6) {
    return (
      <Animated stepKey={6}>
        <GuidelineCard icon="🌙" title="Quiet Hours" body="Outdoor quiet hours begin at 8 PM. Indoor gatherings may continue at a respectful volume. Music outside, poolside noise, and large group conversations outdoors should wrap up by 8 PM nightly." onGotIt={next} />
      </Animated>
    );
  }

  // Step 7 — Guideline: Registered Guests
  if (step === 7) {
    return (
      <Animated stepKey={7}>
        <GuidelineCard icon="🏛️" title="Registered Guests Only" body="Private amenities — including the pool, studio, and game room — are reserved for registered overnight guests only. Additional visitors require prior host approval." onGotIt={next} />
      </Animated>
    );
  }

  // Step 8 — Guideline: Respecting the Home
  if (step === 8) {
    return (
      <Animated stepKey={8}>
        <GuidelineCard icon="🏡" title="Respecting the Home" body="We love hosting families and intimate gatherings. Please remember that unapproved parties and large events aren't permitted — keeping this a peaceful space means every guest gets the experience they came for." onGotIt={next} />
      </Animated>
    );
  }

  // Step 9 — Local Preview
  if (step === 9) {
    return (
      <Animated stepKey={9}>
        <LocalPreview onNext={next} onBack={back} />
      </Animated>
    );
  }

  // Step 10 — Unlock Guide
  return (
    <Animated stepKey={10}>
    <div style={{ minHeight: "100vh", background: BG, display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", padding: "40px 24px", textAlign: "center" }}>
      <ProgressBar step={10} total={TOTAL} />
      <div style={{ maxWidth: 420, width: "100%" }}>
        {/* Gold divider accent */}
        <div style={{ width: 32, height: 2, background: `linear-gradient(90deg, ${GOLD}, ${GOLD_LIGHT})`, borderRadius: 1, margin: "0 auto 32px" }} />

        <h2 style={{ fontFamily: "Georgia, serif", fontSize: "clamp(1.8rem, 6vw, 2.4rem)", color: "#fff", fontWeight: 700, lineHeight: 1.2, marginBottom: 12 }}>
          Welcome to<br />
          <span style={{ color: GOLD }}>Casanova ATL.</span>
        </h2>
        <p style={{ color: "#6B7280", fontSize: 14, lineHeight: 1.65, marginBottom: 32 }}>
          Everything is ready for your arrival.
        </p>

        {/* Hotel-style checklist */}
        <div style={{ background: CARD, border: "1px solid #1e1e1e", borderRadius: 16, padding: "24px 20px", marginBottom: 28, textAlign: "left" }}>
          {[
            { label: "Parking Registered", sub: `${vehicles} vehicle${vehicles !== 1 ? "s" : ""} noted` },
            { label: "Guest Details Confirmed", sub: `${overnightGuests} overnight · ${daytimeVisitors} daytime` },
            { label: "Luxury Guide Ready", sub: "Wi-Fi, door code, amenities & more" },
            { label: "Local Experiences Ready", sub: "Dining, music, studios & coffee" },
            { label: "Stay Personalized", sub: `${reservation.checkIn} – ${reservation.checkOut} · ${reservation.nights} nights` },
          ].map(({ label, sub }, i, arr) => (
            <div key={label} style={{ display: "flex", alignItems: "center", gap: 16, paddingBottom: i < arr.length - 1 ? 16 : 0, marginBottom: i < arr.length - 1 ? 16 : 0, borderBottom: i < arr.length - 1 ? "1px solid #161616" : "none" }}>
              <div style={{ width: 26, height: 26, borderRadius: "50%", background: "rgba(201,168,76,0.12)", border: `1px solid ${GOLD}44`, display: "flex", alignItems: "center", justifyContent: "center", flexShrink: 0 }}>
                <span style={{ color: GOLD, fontSize: 12 }}>✓</span>
              </div>
              <div>
                <p style={{ color: "#fff", fontSize: 14, fontWeight: 600, marginBottom: 2 }}>{label}</p>
                <p style={{ color: "#6B7280", fontSize: 12 }}>{sub}</p>
              </div>
            </div>
          ))}
        </div>

        {/* Personal message */}
        <div style={{ background: "rgba(201,168,76,0.04)", border: `1px solid ${GOLD}18`, borderRadius: 12, padding: "20px 20px 16px", marginBottom: 28, textAlign: "left" }}>
          <p style={{ color: "#9CA3AF", fontSize: 14, lineHeight: 1.75, fontStyle: "italic", marginBottom: 12, fontFamily: "Georgia, serif" }}>
            &ldquo;We&apos;re honored to host you and hope you create unforgettable memories during your stay.&rdquo;
          </p>
          <p style={{ color: GOLD, fontSize: 12, letterSpacing: "0.08em" }}>— Casanova ATL Team</p>
        </div>

        {/* Completion badge */}
        <div style={{ display: "flex", alignItems: "center", justifyContent: "center", gap: 10, marginBottom: 24 }}>
          <span style={{ fontSize: 20 }}>🏅</span>
          <div style={{ textAlign: "left" }}>
            <p style={{ color: GOLD, fontSize: 12, fontWeight: 700, letterSpacing: "0.1em", textTransform: "uppercase" }}>Casanova ATL Ready</p>
            <p style={{ color: "#4B5563", fontSize: 11 }}>Check-in complete</p>
          </div>
        </div>

        <GoldBtn onClick={finish}>Begin Your Stay →</GoldBtn>
      </div>
    </div>
    </Animated>
  );
}
