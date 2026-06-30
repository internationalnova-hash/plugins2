"use client";

import { useState, useEffect } from "react";
import { getBookings, addBooking, deleteBooking, loginHost, isHostLoggedIn, logoutHost, type Booking } from "@/lib/bookingsStore";

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

const inputStyle: React.CSSProperties = {
  width: "100%",
  background: "#0d0d0d",
  border: "1px solid #2a2a2a",
  borderRadius: 8,
  padding: "10px 12px",
  color: "#fff",
  fontSize: 13,
  marginBottom: 8,
  outline: "none",
};

const EMPTY_FORM = { guestName: "", confirmationCode: "", checkIn: "", checkOut: "", nights: "1", bookedGuests: "1" };

function HostLogin({ onSuccess }: { onSuccess: () => void }) {
  const [password, setPassword] = useState("");
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

  const submit = async () => {
    if (!password.trim() || loading) return;
    setLoading(true);
    const ok = await loginHost(password.trim());
    setLoading(false);
    if (ok) {
      setError(null);
      onSuccess();
    } else {
      setError("Incorrect password.");
    }
  };

  return (
    <div>
      <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 12 }}>
        Host Login Required
      </p>
      <input
        style={inputStyle}
        type="password"
        placeholder="Admin Password"
        value={password}
        onChange={(e) => setPassword(e.target.value)}
        onKeyDown={(e) => e.key === "Enter" && submit()}
      />
      {error && <p style={{ color: "#d96b6b", fontSize: 12, marginBottom: 8 }}>{error}</p>}
      <button
        onClick={submit}
        disabled={loading}
        style={{
          width: "100%",
          background: `linear-gradient(135deg, ${GOLD} 0%, ${GOLD_LIGHT} 50%, ${GOLD} 100%)`,
          color: BG,
          border: "none",
          borderRadius: 8,
          padding: "12px",
          fontSize: 12,
          fontWeight: 700,
          letterSpacing: "0.1em",
          textTransform: "uppercase",
          cursor: "pointer",
          marginTop: 4,
        }}
      >
        {loading ? "Checking…" : "Log In"}
      </button>
    </div>
  );
}

function ReservationManager() {
  const [loggedIn, setLoggedIn] = useState(false);
  const [checked, setChecked] = useState(false);
  const [bookings, setBookings] = useState<Booking[]>([]);
  const [form, setForm] = useState(EMPTY_FORM);
  const [error, setError] = useState<string | null>(null);

  const refresh = () => {
    getBookings().then(setBookings);
  };

  useEffect(() => {
    setLoggedIn(isHostLoggedIn());
    setChecked(true);
  }, []);

  useEffect(() => {
    if (loggedIn) refresh();
  }, [loggedIn]);

  const setField = (key: keyof typeof EMPTY_FORM) => (e: React.ChangeEvent<HTMLInputElement>) =>
    setForm((f) => ({ ...f, [key]: e.target.value }));

  const handleAdd = async () => {
    const { guestName, confirmationCode, checkIn, checkOut, nights, bookedGuests } = form;
    if (!guestName.trim() || !confirmationCode.trim() || !checkIn.trim() || !checkOut.trim()) {
      setError("Guest name, confirmation code, and dates are required.");
      return;
    }
    if (bookings.some((b) => b.confirmationCode.toUpperCase() === confirmationCode.trim().toUpperCase())) {
      setError("A reservation with that confirmation code already exists.");
      return;
    }
    const result = await addBooking({
      guestName: guestName.trim(),
      confirmationCode: confirmationCode.trim().toUpperCase(),
      checkIn: checkIn.trim(),
      checkOut: checkOut.trim(),
      nights: Number(nights) || 1,
      bookedGuests: Number(bookedGuests) || 1,
    });
    if (!result.ok) {
      setError(result.error || "Failed to add reservation.");
      return;
    }
    refresh();
    setForm(EMPTY_FORM);
    setError(null);
  };

  const handleDelete = async (code: string) => {
    await deleteBooking(code);
    refresh();
  };

  if (!checked) return null;

  if (!loggedIn) {
    return <HostLogin onSuccess={() => setLoggedIn(true)} />;
  }

  return (
    <div>
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 12 }}>
        <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase" }}>
          Active Reservations
        </p>
        <button
          onClick={() => {
            logoutHost();
            setLoggedIn(false);
          }}
          style={{ background: "transparent", border: "none", color: "#6B7280", fontSize: 11, cursor: "pointer", letterSpacing: "0.05em" }}
        >
          Log Out
        </button>
      </div>

      {bookings.length === 0 ? (
        <p style={{ color: "#4B5563", fontSize: 13, textAlign: "center", padding: "12px 0" }}>No reservations yet</p>
      ) : (
        <div style={{ marginBottom: 16 }}>
          {bookings.map((b) => (
            <div
              key={b.confirmationCode}
              style={{ background: "#0d0d0d", border: "1px solid #1e1e1e", borderRadius: 10, padding: "12px 14px", marginBottom: 8, display: "flex", justifyContent: "space-between", alignItems: "flex-start" }}
            >
              <div>
                <p style={{ color: "#fff", fontSize: 13, fontWeight: 600 }}>{b.guestName}</p>
                <p style={{ color: "#6B7280", fontSize: 11, marginTop: 2 }}>
                  {b.checkIn} – {b.checkOut} · {b.bookedGuests} guests
                </p>
                <p style={{ color: GOLD, fontSize: 11, marginTop: 2, fontFamily: "monospace" }}>{b.confirmationCode}</p>
              </div>
              <button
                onClick={() => handleDelete(b.confirmationCode)}
                style={{ background: "transparent", border: "none", color: "#6B7280", cursor: "pointer", fontSize: 16, lineHeight: 1, padding: 0 }}
              >
                ×
              </button>
            </div>
          ))}
        </div>
      )}

      <div style={{ height: 1, background: "#1e1e1e", margin: "16px 0" }} />

      <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 12 }}>
        New Reservation
      </p>

      <input style={inputStyle} placeholder="Guest Name" value={form.guestName} onChange={setField("guestName")} />
      <input style={inputStyle} placeholder="Confirmation Code" value={form.confirmationCode} onChange={setField("confirmationCode")} />
      <div style={{ display: "flex", gap: 8 }}>
        <input style={inputStyle} placeholder="Check-In (e.g. July 24)" value={form.checkIn} onChange={setField("checkIn")} />
        <input style={inputStyle} placeholder="Check-Out" value={form.checkOut} onChange={setField("checkOut")} />
      </div>
      <div style={{ display: "flex", gap: 8 }}>
        <input style={inputStyle} type="number" min={1} placeholder="Nights" value={form.nights} onChange={setField("nights")} />
        <input style={inputStyle} type="number" min={1} placeholder="Guests" value={form.bookedGuests} onChange={setField("bookedGuests")} />
      </div>

      {error && <p style={{ color: "#d96b6b", fontSize: 12, marginBottom: 8 }}>{error}</p>}

      <button
        onClick={handleAdd}
        style={{
          width: "100%",
          background: `linear-gradient(135deg, ${GOLD} 0%, ${GOLD_LIGHT} 50%, ${GOLD} 100%)`,
          color: BG,
          border: "none",
          borderRadius: 8,
          padding: "12px",
          fontSize: 12,
          fontWeight: 700,
          letterSpacing: "0.1em",
          textTransform: "uppercase",
          cursor: "pointer",
          marginTop: 4,
        }}
      >
        Add Reservation
      </button>
    </div>
  );
}

export default function HostPreview() {
  const [open, setOpen] = useState(false);
  const [tab, setTab] = useState<"guest" | "reservations">("guest");
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

  const fmt = (d: string) => d || "—";

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
            width: 340,
            maxHeight: "80vh",
            overflowY: "auto",
            boxShadow: "0 8px 40px rgba(0,0,0,0.8)",
            padding: 24,
          }}
        >
          {/* Header */}
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 16 }}>
            <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase" }}>Host View</p>
            <button
              onClick={() => setOpen(false)}
              style={{ background: "transparent", border: "none", color: "#6B7280", cursor: "pointer", fontSize: 18, lineHeight: 1 }}
            >
              ×
            </button>
          </div>

          {/* Tab switcher */}
          <div style={{ display: "flex", gap: 6, marginBottom: 20, background: "#0d0d0d", borderRadius: 8, padding: 4 }}>
            {(["guest", "reservations"] as const).map((t) => (
              <button
                key={t}
                onClick={() => setTab(t)}
                style={{
                  flex: 1,
                  background: tab === t ? "#1e1e1e" : "transparent",
                  border: "none",
                  borderRadius: 6,
                  padding: "8px 0",
                  color: tab === t ? GOLD : "#6B7280",
                  fontSize: 11,
                  fontWeight: 700,
                  letterSpacing: "0.08em",
                  textTransform: "uppercase",
                  cursor: "pointer",
                }}
              >
                {t === "guest" ? "Current Guest" : "Reservations"}
              </button>
            ))}
          </div>

          {tab === "reservations" ? (
            <ReservationManager />
          ) : !data ? (
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
    </>
  );
}
