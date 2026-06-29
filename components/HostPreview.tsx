"use client";

import { useState, useEffect } from "react";

const GOLD = "#C9A84C";
const GOLD_LIGHT = "#E8C97A";
const BG = "#0A0A0A";

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

function Checkmark({ ok, label }: { ok: boolean; label: string }) {
  return (
    <div style={{ display: "flex", alignItems: "center", gap: 10, marginBottom: 10 }}>
      <div
        style={{
          width: 20,
          height: 20,
          borderRadius: 4,
          border: `1.5px solid ${ok ? GOLD : "#3a3a3a"}`,
          background: ok ? GOLD : "transparent",
          display: "flex",
          alignItems: "center",
          justifyContent: "center",
          flexShrink: 0,
        }}
      >
        {ok && (
          <svg width="12" height="9" viewBox="0 0 12 9" fill="none">
            <path d="M1 4.5L4.5 8L11 1" stroke="#0A0A0A" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
          </svg>
        )}
      </div>
      <span style={{ color: ok ? "#E5E7EB" : "#4B5563", fontSize: 13 }}>{label}</span>
    </div>
  );
}

export default function HostPreview() {
  const [open, setOpen] = useState(false);
  const [data, setData] = useState<GuestInfo | null>(null);

  const load = () => {
    try {
      const raw = localStorage.getItem("novaStay_guestInfo");
      setData(raw ? (JSON.parse(raw) as GuestInfo) : null);
    } catch {
      setData(null);
    }
  };

  useEffect(() => {
    load();
  }, [open]);

  const fmt = (d: string) =>
    d
      ? new Date(d + "T12:00:00").toLocaleDateString("en-US", { month: "short", day: "numeric", year: "numeric" })
      : "—";

  const clearData = () => {
    localStorage.removeItem("novaStay_guestInfo");
    setData(null);
  };

  return (
    <>
      {/* Trigger button */}
      <button
        onClick={() => setOpen((o) => !o)}
        style={{
          position: "fixed",
          bottom: 24,
          right: 24,
          zIndex: 200,
          background: "#111",
          border: `1px solid ${GOLD}`,
          borderRadius: 8,
          padding: "10px 16px",
          color: GOLD,
          fontSize: 12,
          fontWeight: 600,
          letterSpacing: "0.1em",
          textTransform: "uppercase",
          cursor: "pointer",
          boxShadow: "0 4px 20px rgba(0,0,0,0.5)",
        }}
      >
        Host View
      </button>

      {/* Panel */}
      {open && (
        <div
          style={{
            position: "fixed",
            bottom: 72,
            right: 16,
            zIndex: 199,
            background: "#111",
            border: `1px solid #2a2a2a`,
            borderRadius: 12,
            width: 320,
            maxHeight: "80vh",
            overflowY: "auto",
            boxShadow: "0 8px 40px rgba(0,0,0,0.8)",
            padding: 24,
          }}
        >
          {/* Header */}
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 20 }}>
            <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase" }}>Host View</p>
            <button
              onClick={() => setOpen(false)}
              style={{ background: "transparent", border: "none", color: "#6B7280", cursor: "pointer", fontSize: 18, lineHeight: 1 }}
            >
              ×
            </button>
          </div>

          {!data ? (
            <p style={{ color: "#4B5563", fontSize: 14, textAlign: "center", padding: "20px 0" }}>
              No check-in data yet
            </p>
          ) : (
            <>
              {/* Readiness % */}
              <div style={{ textAlign: "center", marginBottom: 20 }}>
                <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 4 }}>
                  Guest Readiness
                </p>
                <p style={{ color: GOLD, fontSize: 48, fontWeight: 700, fontFamily: "Georgia, serif", lineHeight: 1 }}>
                  {data.readinessPercent}%
                </p>
              </div>

              <div style={{ height: 1, background: "#1e1e1e", marginBottom: 16 }} />

              {/* Info rows */}
              {[
                { label: "Guest", value: data.name },
                { label: "Check-In", value: fmt(data.checkIn) },
                { label: "Check-Out", value: fmt(data.checkOut) },
                { label: "Overnight Guests", value: String(data.overnightGuests) },
                { label: "Daytime Visitors", value: String(data.daytimeVisitors) },
                { label: "Vehicles", value: String(data.vehicles) },
              ].map(({ label, value }) => (
                <div key={label} style={{ display: "flex", justifyContent: "space-between", marginBottom: 10 }}>
                  <span style={{ color: "#6B7280", fontSize: 13 }}>{label}</span>
                  <span style={{ color: "#fff", fontSize: 13, fontWeight: 600 }}>{value}</span>
                </div>
              ))}

              <div style={{ height: 1, background: "#1e1e1e", margin: "16px 0" }} />

              {/* Acknowledgments */}
              <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 12 }}>
                Acknowledgments
              </p>
              <Checkmark ok={data.parkingAck} label="Parking" />
              <Checkmark ok={data.quietHoursAck} label="Quiet Hours" />
              <Checkmark ok={data.registeredGuestsAck} label="Registered Guests" />

              <div style={{ height: 1, background: "#1e1e1e", margin: "16px 0" }} />

              {/* Clear */}
              <button
                onClick={clearData}
                style={{
                  width: "100%",
                  background: "transparent",
                  border: "1px solid #3a3a3a",
                  borderRadius: 6,
                  padding: "10px",
                  color: "#9CA3AF",
                  fontSize: 12,
                  cursor: "pointer",
                  letterSpacing: "0.1em",
                  textTransform: "uppercase",
                }}
              >
                Clear Data
              </button>
            </>
          )}
        </div>
      )}

      {/* Unused variable to satisfy import */}
      <span style={{ display: "none" }}>{BG}{GOLD_LIGHT}</span>
    </>
  );
}
