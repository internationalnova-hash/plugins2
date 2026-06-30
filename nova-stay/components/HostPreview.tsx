"use client";

import { useState, useEffect } from "react";
import {
  LayoutDashboard,
  CalendarCheck,
  UserRound,
  KeyRound,
  Settings as SettingsIcon,
  Wifi,
  Waves,
  Clapperboard,
  DoorOpen,
  Users,
  PlaneLanding,
  TrendingUp,
  LogOut,
  Trash2,
  type LucideIcon,
} from "lucide-react";
import { getBookings, addBooking, deleteBooking, loginHost, isHostLoggedIn, logoutHost, type Booking } from "@/lib/bookingsStore";
import { getJourneyStage } from "@/lib/novaJourney";
import SNMonogram from "@/components/SNMonogram";

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

function StatCard({ Icon, label, value }: { Icon: LucideIcon; label: string; value: string | number }) {
  return (
    <div className="lux-glass" style={{ padding: "14px 14px", display: "flex", flexDirection: "column", gap: 8 }}>
      <Icon size={16} strokeWidth={1.8} style={{ color: GOLD }} />
      <p style={{ color: "#fff", fontSize: 22, fontWeight: 700, fontFamily: "Georgia, serif", lineHeight: 1 }}>{value}</p>
      <p style={{ color: "#6B7280", fontSize: 10.5, letterSpacing: "0.06em", textTransform: "uppercase" }}>{label}</p>
    </div>
  );
}

function Overview() {
  const [bookings, setBookings] = useState<Booking[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    getBookings().then((b) => {
      setBookings(b);
      setLoading(false);
    });
  }, []);

  if (loading) {
    return <p style={{ color: "#4B5563", fontSize: 13, textAlign: "center", padding: "20px 0" }}>Loading…</p>;
  }

  const stages = bookings.map((b) => getJourneyStage(b.checkIn, b.checkOut));
  const arrivingToday = stages.filter((s) => s === "checkedIn").length;
  const currentlyHosted = stages.filter((s) => s === "checkedIn" || s === "enjoying" || s === "checkout").length;
  const avgNights = bookings.length ? Math.round((bookings.reduce((sum, b) => sum + b.nights, 0) / bookings.length) * 10) / 10 : 0;

  return (
    <div>
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 10, marginBottom: 16 }}>
        <StatCard Icon={CalendarCheck} label="Total Reservations" value={bookings.length} />
        <StatCard Icon={PlaneLanding} label="Arriving Today" value={arrivingToday} />
        <StatCard Icon={Users} label="Currently Hosted" value={currentlyHosted} />
        <StatCard Icon={TrendingUp} label="Avg. Nights" value={avgNights} />
      </div>

      <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 10 }}>
        Upcoming Stays
      </p>
      {bookings.length === 0 ? (
        <p style={{ color: "#4B5563", fontSize: 13, textAlign: "center", padding: "12px 0" }}>No reservations yet</p>
      ) : (
        bookings.slice(0, 4).map((b) => (
          <div key={b.confirmationCode} className="lux-glass" style={{ padding: "10px 14px", marginBottom: 8, display: "flex", justifyContent: "space-between", alignItems: "center" }}>
            <div>
              <p style={{ color: "#fff", fontSize: 13, fontWeight: 600 }}>{b.guestName}</p>
              <p style={{ color: "#6B7280", fontSize: 11, marginTop: 2 }}>{b.checkIn} – {b.checkOut}</p>
            </div>
            <span style={{ color: GOLD, fontSize: 10, fontWeight: 700, textTransform: "uppercase", letterSpacing: "0.05em" }}>
              {b.nights}n
            </span>
          </div>
        ))
      )}
    </div>
  );
}

const PROPERTY_STATUS: { Icon: LucideIcon; label: string; status: string }[] = [
  { Icon: Wifi,         label: "Wi-Fi Network",  status: "Online"  },
  { Icon: Waves,        label: "Pool & Spa",     status: "Ready"   },
  { Icon: Clapperboard, label: "Theater & Studio", status: "Ready" },
  { Icon: DoorOpen,     label: "Smart Door Lock", status: "Online" },
];

function PropertyStatus() {
  return (
    <div>
      <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 12 }}>
        Property Status
      </p>
      {PROPERTY_STATUS.map((p) => (
        <div key={p.label} className="lux-glass" style={{ padding: "12px 14px", marginBottom: 8, display: "flex", alignItems: "center", gap: 12 }}>
          <p.Icon size={18} strokeWidth={1.7} style={{ color: GOLD, flexShrink: 0 }} />
          <span style={{ color: "#E5E7EB", fontSize: 13, flex: 1 }}>{p.label}</span>
          <span className="status-pill ready" style={{ fontSize: 10 }}>● {p.status}</span>
        </div>
      ))}
    </div>
  );
}

function HostSettings({ onLogout }: { onLogout: () => void }) {
  return (
    <div>
      <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 12 }}>
        Settings
      </p>
      <button
        onClick={() => {
          localStorage.removeItem("stayByNova_guestInfo");
        }}
        className="lux-glass btn-press"
        style={{ width: "100%", display: "flex", alignItems: "center", gap: 10, padding: "12px 14px", marginBottom: 8, color: "#9CA3AF", fontSize: 13, cursor: "pointer", border: "none" }}
      >
        <Trash2 size={16} strokeWidth={1.8} style={{ color: GOLD }} />
        Clear Current Guest Data
      </button>
      <button
        onClick={onLogout}
        className="lux-glass btn-press"
        style={{ width: "100%", display: "flex", alignItems: "center", gap: 10, padding: "12px 14px", color: "#9CA3AF", fontSize: 13, cursor: "pointer", border: "none" }}
      >
        <LogOut size={16} strokeWidth={1.8} style={{ color: GOLD }} />
        Log Out of Host Dashboard
      </button>
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
  const [bookings, setBookings] = useState<Booking[]>([]);
  const [form, setForm] = useState(EMPTY_FORM);
  const [error, setError] = useState<string | null>(null);

  const refresh = () => {
    getBookings().then(setBookings);
  };

  useEffect(() => {
    refresh();
  }, []);

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

  return (
    <div>
      <div style={{ marginBottom: 12 }}>
        <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase" }}>
          Active Reservations
        </p>
      </div>

      {bookings.length === 0 ? (
        <p style={{ color: "#4B5563", fontSize: 13, textAlign: "center", padding: "12px 0" }}>No reservations yet</p>
      ) : (
        <div style={{ marginBottom: 16 }}>
          {bookings.map((b) => (
            <div
              key={b.confirmationCode}
              className="lux-glass"
              style={{ padding: "12px 14px", marginBottom: 8, display: "flex", justifyContent: "space-between", alignItems: "flex-start" }}
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

type DashTab = "overview" | "reservations" | "guest" | "property" | "settings";

const DASH_TABS: { key: DashTab; label: string; Icon: LucideIcon }[] = [
  { key: "overview",     label: "Overview",     Icon: LayoutDashboard },
  { key: "reservations", label: "Reservations", Icon: CalendarCheck   },
  { key: "guest",        label: "Guest",        Icon: UserRound       },
  { key: "property",     label: "Property",     Icon: KeyRound        },
  { key: "settings",     label: "Settings",     Icon: SettingsIcon    },
];

export default function HostPreview() {
  const [open, setOpen] = useState(false);
  const [tab, setTab] = useState<DashTab>("overview");
  const [data, setData] = useState<GuestInfo | null>(null);
  const [loggedIn, setLoggedIn] = useState(false);
  const [checked, setChecked] = useState(false);

  const load = () => {
    try {
      const raw = localStorage.getItem("stayByNova_guestInfo");
      setData(raw ? (JSON.parse(raw) as GuestInfo) : null);
    } catch {
      setData(null);
    }
  };

  useEffect(() => {
    load();
    setLoggedIn(isHostLoggedIn());
    setChecked(true);
  }, [open]);

  const fmt = (d: string) => d || "—";

  const clearData = () => {
    localStorage.removeItem("stayByNova_guestInfo");
    setData(null);
  };

  return (
    <>
      {/* Trigger button */}
      <button
        onClick={() => setOpen((o) => !o)}
        className="lux-glass btn-press"
        style={{
          position: "fixed",
          bottom: 24,
          right: 24,
          zIndex: 200,
          padding: "10px 16px",
          color: GOLD,
          fontSize: 12,
          fontWeight: 600,
          letterSpacing: "0.1em",
          textTransform: "uppercase",
          cursor: "pointer",
        }}
      >
        Host View
      </button>

      {/* Panel */}
      {open && (
        <div
          className="lux-glass-strong"
          style={{
            position: "fixed",
            bottom: 72,
            right: 16,
            zIndex: 199,
            width: 380,
            maxHeight: "82vh",
            overflowY: "auto",
            padding: 24,
          }}
        >
          {/* Header */}
          <div style={{ marginBottom: 16 }}>
            <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 4 }}>
              <SNMonogram size={20} id="hostpreview" />
              <span style={{ color: GOLD, fontSize: 10, fontWeight: 700, letterSpacing: "0.2em", textTransform: "uppercase" }}>
                StayByNova
              </span>
            </div>
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
            <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase" }}>Host Dashboard</p>
            <button
              onClick={() => setOpen(false)}
              style={{ background: "transparent", border: "none", color: "#6B7280", cursor: "pointer", fontSize: 18, lineHeight: 1 }}
            >
              ×
            </button>
          </div>
          </div>

          {!checked ? null : !loggedIn ? (
            <HostLogin onSuccess={() => setLoggedIn(true)} />
          ) : (
            <>
              {/* Tab switcher */}
              <div style={{ display: "flex", gap: 4, marginBottom: 20, background: "#0d0d0d", borderRadius: 8, padding: 4, overflowX: "auto" }} className="no-scrollbar">
                {DASH_TABS.map((t) => (
                  <button
                    key={t.key}
                    onClick={() => setTab(t.key)}
                    className="btn-press"
                    style={{
                      flex: "1 0 auto",
                      display: "flex",
                      flexDirection: "column",
                      alignItems: "center",
                      gap: 4,
                      background: tab === t.key ? "#1e1e1e" : "transparent",
                      border: "none",
                      borderRadius: 6,
                      padding: "8px 10px",
                      color: tab === t.key ? GOLD : "#6B7280",
                      fontSize: 9.5,
                      fontWeight: 700,
                      letterSpacing: "0.04em",
                      textTransform: "uppercase",
                      cursor: "pointer",
                      whiteSpace: "nowrap",
                    }}
                  >
                    <t.Icon size={14} strokeWidth={1.8} />
                    {t.label}
                  </button>
                ))}
              </div>

              {tab === "overview" && <Overview />}
              {tab === "property" && <PropertyStatus />}
              {tab === "settings" && (
                <HostSettings
                  onLogout={() => {
                    logoutHost();
                    setLoggedIn(false);
                  }}
                />
              )}
              {tab === "reservations" && <ReservationManager />}
              {tab === "guest" && (!data ? (
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
          ))}
            </>
          )}
        </div>
      )}
    </>
  );
}
