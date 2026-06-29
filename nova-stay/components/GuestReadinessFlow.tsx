"use client";

import { useState } from "react";

interface GuestInfo {
  name: string;
  checkIn: string;
  checkOut: string;
  overnightGuests: number;
  daytimeVisitors: number;
  vehicles: number;
  parkingAck: boolean;
  quietHoursAck: boolean;
  registeredGuestsAck: boolean;
  completedAt: string;
  readinessPercent: number;
}

interface Props {
  onComplete: () => void;
}

const GOLD = "#C9A84C";
const GOLD_LIGHT = "#E8C97A";
const BG = "#0A0A0A";
const CARD = "#111111";

function StepHeader({
  stepNum,
  title,
  subtitle,
}: {
  stepNum: number;
  title: string;
  subtitle?: string;
}) {
  return (
    <div data-step={stepNum}>
      <h2
        style={{
          fontFamily: "Georgia, serif",
          fontSize: "clamp(1.4rem, 5vw, 1.9rem)",
          color: "#fff",
          fontWeight: 700,
          lineHeight: 1.25,
          marginBottom: subtitle ? 10 : 0,
        }}
      >
        {title}
      </h2>
      {subtitle && (
        <p style={{ color: "#9CA3AF", fontSize: 14, lineHeight: 1.6 }}>{subtitle}</p>
      )}
    </div>
  );
}

function Stepper({
  value,
  min,
  max,
  onChange,
}: {
  value: number;
  min: number;
  max: number;
  onChange: (v: number) => void;
}) {
  return (
    <div style={{ display: "flex", alignItems: "center", justifyContent: "center", gap: 24, marginTop: 24 }}>
      <button
        onClick={() => onChange(Math.max(min, value - 1))}
        style={{
          width: 52,
          height: 52,
          border: `1.5px solid ${GOLD}`,
          borderRadius: 8,
          background: "transparent",
          color: GOLD,
          fontSize: 24,
          cursor: "pointer",
          display: "flex",
          alignItems: "center",
          justifyContent: "center",
        }}
        aria-label="Decrease"
      >
        −
      </button>
      <span
        style={{
          color: "#fff",
          fontSize: 36,
          fontWeight: 600,
          minWidth: 48,
          textAlign: "center",
          fontFamily: "Georgia, serif",
        }}
      >
        {value}
      </span>
      <button
        onClick={() => onChange(Math.min(max, value + 1))}
        style={{
          width: 52,
          height: 52,
          border: `1.5px solid ${GOLD}`,
          borderRadius: 8,
          background: "transparent",
          color: GOLD,
          fontSize: 24,
          cursor: "pointer",
          display: "flex",
          alignItems: "center",
          justifyContent: "center",
        }}
        aria-label="Increase"
      >
        +
      </button>
    </div>
  );
}

function AckCheckbox({
  checked,
  onChange,
  label,
}: {
  checked: boolean;
  onChange: (v: boolean) => void;
  label: string;
}) {
  return (
    <button
      onClick={() => onChange(!checked)}
      style={{
        display: "flex",
        alignItems: "center",
        gap: 16,
        marginTop: 28,
        background: "transparent",
        border: "none",
        cursor: "pointer",
        padding: "12px 0",
        minHeight: 52,
        width: "100%",
      }}
      aria-pressed={checked}
    >
      <div
        style={{
          width: 28,
          height: 28,
          border: `2px solid ${GOLD}`,
          borderRadius: 6,
          flexShrink: 0,
          display: "flex",
          alignItems: "center",
          justifyContent: "center",
          background: checked ? GOLD : "transparent",
          transition: "background 0.2s",
        }}
      >
        {checked && (
          <svg width="16" height="12" viewBox="0 0 16 12" fill="none">
            <path d="M1.5 6L6 10.5L14.5 1.5" stroke="#0A0A0A" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round" />
          </svg>
        )}
      </div>
      <span style={{ color: checked ? GOLD : "#E5E7EB", fontSize: 15, textAlign: "left", fontWeight: 500 }}>
        {label}
      </span>
    </button>
  );
}

export default function GuestReadinessFlow({ onComplete }: Props) {
  const [step, setStep] = useState(0);
  const [name, setName] = useState("");
  const [checkIn, setCheckIn] = useState("");
  const [checkOut, setCheckOut] = useState("");
  const [overnightGuests, setOvernightGuests] = useState(2);
  const [daytimeVisitors, setDaytimeVisitors] = useState(0);
  const [vehicles, setVehicles] = useState(1);
  const [parkingAck, setParkingAck] = useState(false);
  const [quietHoursAck, setQuietHoursAck] = useState(false);
  const [registeredGuestsAck, setRegisteredGuestsAck] = useState(false);

  const TOTAL_STEPS = 9;
  const progressPercent = step === 0 ? 0 : step >= 10 ? 100 : Math.round((step / TOTAL_STEPS) * 100);

  const canContinue = () => {
    if (step === 1) return name.trim().length > 0;
    if (step === 2) return checkIn !== "" && checkOut !== "";
    if (step === 6) return parkingAck;
    if (step === 7) return quietHoursAck;
    if (step === 8) return registeredGuestsAck;
    return true;
  };

  const handleNext = () => {
    if (step === TOTAL_STEPS) {
      const info: GuestInfo = {
        name: name.trim(),
        checkIn,
        checkOut,
        overnightGuests,
        daytimeVisitors,
        vehicles,
        parkingAck,
        quietHoursAck,
        registeredGuestsAck,
        completedAt: new Date().toISOString(),
        readinessPercent: 100,
      };
      localStorage.setItem("novaStay_guestInfo", JSON.stringify(info));
      setStep(10);
    } else {
      setStep((s) => s + 1);
    }
  };

  const handleBack = () => setStep((s) => s - 1);

  const goldBtnStyle: React.CSSProperties = {
    background: `linear-gradient(135deg, ${GOLD} 0%, ${GOLD_LIGHT} 50%, ${GOLD} 100%)`,
    color: BG,
    border: "none",
    borderRadius: 6,
    padding: "16px 32px",
    fontSize: 14,
    fontWeight: 700,
    letterSpacing: "0.2em",
    textTransform: "uppercase",
    cursor: canContinue() ? "pointer" : "default",
    width: "100%",
    minHeight: 52,
    boxShadow: "0 4px 24px rgba(201,168,76,0.35)",
    opacity: canContinue() ? 1 : 0.4,
    transition: "opacity 0.2s",
  };

  const backBtnStyle: React.CSSProperties = {
    background: "transparent",
    color: "#9CA3AF",
    border: "1px solid #2a2a2a",
    borderRadius: 6,
    padding: "14px 24px",
    fontSize: 13,
    fontWeight: 500,
    cursor: "pointer",
    minHeight: 48,
  };

  const fmt = (d: string) =>
    d
      ? new Date(d + "T12:00:00").toLocaleDateString("en-US", {
          month: "short",
          day: "numeric",
          year: "numeric",
        })
      : "—";

  // Welcome
  if (step === 0) {
    return (
      <div style={{ minHeight: "100vh", background: BG, display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", padding: "32px 24px" }}>
        <div style={{ textAlign: "center", maxWidth: 400, width: "100%" }}>
          <div style={{ fontSize: 52, marginBottom: 20, color: GOLD }}>♛</div>
          <h1 style={{ fontFamily: "Georgia, serif", fontSize: "clamp(1.8rem, 6vw, 2.4rem)", color: "#fff", fontWeight: 700, marginBottom: 12, lineHeight: 1.2 }}>
            Before You Arrive
          </h1>
          <p style={{ color: "#9CA3AF", fontSize: 15, marginBottom: 48, lineHeight: 1.6 }}>
            A quick moment to personalize your stay
          </p>
          <button
            onClick={() => setStep(1)}
            style={{
              background: `linear-gradient(135deg, ${GOLD} 0%, ${GOLD_LIGHT} 50%, ${GOLD} 100%)`,
              color: BG,
              border: "none",
              borderRadius: 6,
              padding: "16px 32px",
              fontSize: 14,
              fontWeight: 700,
              letterSpacing: "0.2em",
              textTransform: "uppercase",
              cursor: "pointer",
              width: "100%",
              minHeight: 52,
              boxShadow: "0 4px 24px rgba(201,168,76,0.35)",
            }}
          >
            Begin Check-In
          </button>
        </div>
      </div>
    );
  }

  // Completion
  if (step === 10) {
    return (
      <div style={{ minHeight: "100vh", background: BG, display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", padding: "32px 24px" }}>
        <div style={{ position: "fixed", top: 0, left: 0, right: 0, height: 3, background: "#1a1a1a", zIndex: 100 }}>
          <div style={{ height: "100%", width: "100%", background: `linear-gradient(90deg, ${GOLD}, ${GOLD_LIGHT})` }} />
        </div>
        <div style={{ maxWidth: 420, width: "100%", textAlign: "center" }}>
          <div style={{ color: GOLD, fontSize: 32, marginBottom: 8 }}>✦</div>
          <h2 style={{ fontFamily: "Georgia, serif", fontSize: "clamp(1.5rem, 5vw, 2rem)", color: GOLD, fontWeight: 700, marginBottom: 8 }}>
            Welcome, {name}!
          </h2>
          <p style={{ color: GOLD, fontSize: 16, marginBottom: 32 }}>✦ You&apos;re all set.</p>
          <div style={{ background: CARD, border: "1px solid #2a2a2a", borderRadius: 12, padding: 24, marginBottom: 32, textAlign: "left" }}>
            <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 16 }}>
              Stay Summary
            </p>
            {[
              { label: "Check-In", value: fmt(checkIn) },
              { label: "Check-Out", value: fmt(checkOut) },
              { label: "Overnight Guests", value: String(overnightGuests) },
              { label: "Daytime Visitors", value: String(daytimeVisitors) },
              { label: "Vehicles", value: String(vehicles) },
            ].map(({ label, value }) => (
              <div key={label} style={{ display: "flex", justifyContent: "space-between", paddingBottom: 10, marginBottom: 10, borderBottom: "1px solid #1e1e1e" }}>
                <span style={{ color: "#9CA3AF", fontSize: 13 }}>{label}</span>
                <span style={{ color: "#fff", fontSize: 13, fontWeight: 600 }}>{value}</span>
              </div>
            ))}
          </div>
          <button
            onClick={onComplete}
            style={{
              background: `linear-gradient(135deg, ${GOLD} 0%, ${GOLD_LIGHT} 50%, ${GOLD} 100%)`,
              color: BG,
              border: "none",
              borderRadius: 6,
              padding: "16px 32px",
              fontSize: 14,
              fontWeight: 700,
              letterSpacing: "0.2em",
              textTransform: "uppercase",
              cursor: "pointer",
              width: "100%",
              minHeight: 52,
              boxShadow: "0 4px 24px rgba(201,168,76,0.35)",
            }}
          >
            Enter Your Guide →
          </button>
        </div>
      </div>
    );
  }

  // Steps 1-9
  const renderContent = () => {
    switch (step) {
      case 1:
        return (
          <>
            <StepHeader stepNum={step} title="What's your name?" subtitle="So we can welcome you personally" />
            <input
              type="text"
              value={name}
              onChange={(e) => setName(e.target.value)}
              placeholder="Your first name"
              autoFocus
              style={{
                width: "100%",
                background: "#1a1a1a",
                border: `1.5px solid ${name ? GOLD : "#2a2a2a"}`,
                borderRadius: 8,
                padding: "16px 20px",
                color: "#fff",
                fontSize: 16,
                marginTop: 24,
                outline: "none",
              }}
            />
          </>
        );
      case 2:
        return (
          <>
            <StepHeader stepNum={step} title="Reservation Dates" subtitle="When are you arriving and departing?" />
            <div style={{ marginTop: 24 }}>
              <label style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", display: "block", marginBottom: 8 }}>Check-In</label>
              <input
                type="date"
                value={checkIn}
                onChange={(e) => setCheckIn(e.target.value)}
                style={{ width: "100%", background: "#1a1a1a", border: `1.5px solid ${checkIn ? GOLD : "#2a2a2a"}`, borderRadius: 8, padding: "14px 16px", color: "#fff", fontSize: 15, marginBottom: 16, outline: "none", colorScheme: "dark" }}
              />
              <label style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", display: "block", marginBottom: 8 }}>Check-Out</label>
              <input
                type="date"
                value={checkOut}
                onChange={(e) => setCheckOut(e.target.value)}
                min={checkIn}
                style={{ width: "100%", background: "#1a1a1a", border: `1.5px solid ${checkOut ? GOLD : "#2a2a2a"}`, borderRadius: 8, padding: "14px 16px", color: "#fff", fontSize: 15, outline: "none", colorScheme: "dark" }}
              />
            </div>
          </>
        );
      case 3:
        return (
          <>
            <StepHeader stepNum={step} title="Overnight Guests" subtitle="How many guests will be staying overnight?" />
            <Stepper value={overnightGuests} min={1} max={16} onChange={setOvernightGuests} />
          </>
        );
      case 4:
        return (
          <>
            <StepHeader stepNum={step} title="Daytime Visitors" subtitle="Visitors who won't be staying overnight" />
            <Stepper value={daytimeVisitors} min={0} max={20} onChange={setDaytimeVisitors} />
          </>
        );
      case 5:
        return (
          <>
            <StepHeader stepNum={step} title="Vehicles" subtitle="How many vehicles will you have?" />
            <Stepper value={vehicles} min={0} max={8} onChange={setVehicles} />
          </>
        );
      case 6:
        return (
          <>
            <StepHeader stepNum={step} title="Parking Policy" />
            <p style={{ color: "#D1D5DB", fontSize: 15, lineHeight: 1.7, marginTop: 20 }}>
              Parking is available in the designated areas only. Please do not block neighboring driveways or park on the grass.
            </p>
            <AckCheckbox checked={parkingAck} onChange={setParkingAck} label="Got it, we'll park responsibly" />
          </>
        );
      case 7:
        return (
          <>
            <StepHeader stepNum={step} title="Quiet Hours" />
            <p style={{ color: "#D1D5DB", fontSize: 15, lineHeight: 1.7, marginTop: 20 }}>
              Quiet hours are observed from 10 PM to 9 AM. We ask that all guests respect our neighbors and keep noise levels low during these hours.
            </p>
            <AckCheckbox checked={quietHoursAck} onChange={setQuietHoursAck} label="Got it, we'll keep it peaceful" />
          </>
        );
      case 8:
        return (
          <>
            <StepHeader stepNum={step} title="Registered Guests" />
            <p style={{ color: "#D1D5DB", fontSize: 15, lineHeight: 1.7, marginTop: 20 }}>
              Only registered overnight guests are permitted to sleep at the property. All daytime visitors must depart by midnight.
            </p>
            <AckCheckbox checked={registeredGuestsAck} onChange={setRegisteredGuestsAck} label="Got it, we understand the policy" />
          </>
        );
      default:
        return null;
    }
  };

  return (
    <div style={{ minHeight: "100vh", background: BG, display: "flex", flexDirection: "column" }}>
      {/* Progress bar */}
      <div style={{ position: "fixed", top: 0, left: 0, right: 0, height: 3, background: "#1a1a1a", zIndex: 100 }}>
        <div
          style={{
            height: "100%",
            width: `${progressPercent}%`,
            background: `linear-gradient(90deg, ${GOLD}, ${GOLD_LIGHT})`,
            transition: "width 0.4s ease",
          }}
        />
      </div>

      <div style={{ flex: 1, display: "flex", flexDirection: "column", justifyContent: "center", padding: "60px 24px 40px", maxWidth: 480, margin: "0 auto", width: "100%" }}>
        <p style={{ color: GOLD, fontSize: 11, letterSpacing: "0.2em", textTransform: "uppercase", marginBottom: 32 }}>
          Step {step} of {TOTAL_STEPS}
        </p>

        {renderContent()}

        <div style={{ marginTop: 40, display: "flex", flexDirection: "column", gap: 12 }}>
          <button onClick={handleNext} disabled={!canContinue()} style={goldBtnStyle}>
            {step === TOTAL_STEPS ? "Complete Check-In" : "Continue →"}
          </button>
          <button onClick={handleBack} style={backBtnStyle}>
            ← Back
          </button>
        </div>
      </div>
    </div>
  );
}
