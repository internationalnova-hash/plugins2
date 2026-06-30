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
  Sunset,
  Bell,
  MessageSquareText,
  Lightbulb,
  Megaphone,
  Cloud,
  Sun,
  CloudSun,
  CloudRain,
  MessageCircle,
  Sparkles,
  Volume2,
  VolumeX,
  type LucideIcon,
} from "lucide-react";
import { getBookings, addBooking, deleteBooking, loginHost, isHostLoggedIn, logoutHost, type Booking } from "@/lib/bookingsStore";
import { getJourneyStage } from "@/lib/novaJourney";
import {
  getPropertyState,
  updatePropertyState,
  getGuestRequests,
  markRequestRead,
  sendWelcomeMessage,
  getHostBriefing,
  type PropertyState,
  type GuestRequest,
} from "@/lib/propertyStore";
import type { WeatherInfo } from "@/lib/weather";
import SNMonogram from "@/components/SNMonogram";
import theme from "@/config/theme";

const WEATHER_ICONS: Partial<Record<WeatherInfo["icon"], LucideIcon>> = {
  Sun, CloudSun, Cloud, CloudRain,
};

const GOLD = theme.gold;
const GOLD_LIGHT = theme.goldLight;
const BG = theme.bg;

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

function QuickAction({ Icon, label, onClick }: { Icon: LucideIcon; label: string; onClick: () => void }) {
  return (
    <button
      onClick={onClick}
      className="lux-glass btn-press"
      style={{
        display: "flex",
        flexDirection: "column",
        alignItems: "center",
        gap: 6,
        padding: "12px 8px",
        border: "none",
        cursor: "pointer",
        color: "#E5E7EB",
        fontSize: 10.5,
        textAlign: "center",
      }}
    >
      <Icon size={16} strokeWidth={1.8} style={{ color: GOLD }} />
      {label}
    </button>
  );
}

function Overview({ onGoToReservations }: { onGoToReservations: () => void }) {
  const [bookings, setBookings] = useState<Booking[]>([]);
  const [property, setProperty] = useState<PropertyState | null>(null);
  const [requests, setRequests] = useState<GuestRequest[]>([]);
  const [weather, setWeather] = useState<WeatherInfo | null>(null);
  const [loading, setLoading] = useState(true);
  const [briefing, setBriefing] = useState<string | null>(null);
  const [briefingLoading, setBriefingLoading] = useState(false);
  const [briefingError, setBriefingError] = useState<string | null>(null);
  const [speaking, setSpeaking] = useState(false);

  const fetchBriefing = async () => {
    setBriefingLoading(true);
    setBriefingError(null);
    window.speechSynthesis?.cancel();
    setSpeaking(false);
    const result = await getHostBriefing();
    setBriefingLoading(false);
    if (!result.ok) {
      setBriefingError(result.error || "Failed to generate briefing.");
      return;
    }
    setBriefing(result.briefing || null);
  };

  const toggleSpeak = () => {
    if (!briefing || !("speechSynthesis" in window)) return;
    if (speaking) {
      window.speechSynthesis.cancel();
      setSpeaking(false);
      return;
    }
    const utterance = new SpeechSynthesisUtterance(briefing);
    utterance.rate = 1;
    utterance.onend = () => setSpeaking(false);
    utterance.onerror = () => setSpeaking(false);
    window.speechSynthesis.cancel();
    window.speechSynthesis.speak(utterance);
    setSpeaking(true);
  };

  useEffect(() => {
    return () => {
      window.speechSynthesis?.cancel();
    };
  }, []);

  const refresh = () => {
    Promise.all([getBookings(), getPropertyState(), getGuestRequests()]).then(([b, p, r]) => {
      setBookings(b);
      setProperty(p);
      setRequests(r);
      setLoading(false);
    });
  };

  useEffect(() => {
    refresh();
    fetch("/api/weather").then((res) => (res.ok ? res.json() : null)).then((data) => {
      if (data && !data.error) setWeather(data);
    }).catch(() => {});
  }, []);

  const togglePoolLights = async () => {
    if (!property) return;
    const next = !property.poolLightsOn;
    setProperty({ ...property, poolLightsOn: next });
    await updatePropertyState({ poolLightsOn: next });
  };

  const updateDoorCode = async () => {
    const next = window.prompt("New door code:", property?.doorCode || "");
    if (!next || !next.trim()) return;
    const result = await updatePropertyState({ doorCode: next.trim() });
    if (result.ok) refresh();
  };

  const broadcastNotice = async () => {
    const next = window.prompt("Broadcast a notice to all current guests:", property?.hostNotice || "");
    if (next === null) return;
    const result = await updatePropertyState({ hostNotice: next.trim() });
    if (result.ok) refresh();
  };

  if (loading) {
    return <p style={{ color: "#4B5563", fontSize: 13, textAlign: "center", padding: "20px 0" }}>Loading…</p>;
  }

  const stages = bookings.map((b) => getJourneyStage(b.checkIn, b.checkOut));
  const arrivingToday = stages.filter((s) => s === "checkedIn").length;
  const currentlyHosted = stages.filter((s) => s === "checkedIn" || s === "enjoying" || s === "checkout").length;
  const upcomingCheckouts = stages.filter((s) => s === "checkout").length;
  const unreadRequests = requests.filter((r) => !r.isRead).length;
  const WeatherIcon = (weather && WEATHER_ICONS[weather.icon]) || CloudSun;

  return (
    <div>
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 10, marginBottom: 16 }}>
        <StatCard Icon={PlaneLanding} label="Today's Arrivals" value={arrivingToday} />
        <StatCard Icon={Users} label="Current Guests" value={currentlyHosted} />
        <StatCard Icon={Sunset} label="Upcoming Checkouts" value={upcomingCheckouts} />
        <StatCard Icon={Bell} label="Unread Requests" value={unreadRequests} />
        <StatCard Icon={Waves} label="Pool Status" value={property?.poolLightsOn ? "Lit" : "Off"} />
        <StatCard Icon={WeatherIcon} label="Weather" value={weather ? `${weather.tempF}°` : "—"} />
        <StatCard Icon={CalendarCheck} label="Total Reservations" value={bookings.length} />
        <StatCard Icon={TrendingUp} label="Avg. Nights" value={bookings.length ? Math.round((bookings.reduce((s, b) => s + b.nights, 0) / bookings.length) * 10) / 10 : 0} />
      </div>

      <div className="lux-glass" style={{ padding: "14px 16px", marginBottom: 16 }}>
        <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: briefing || briefingError ? 10 : 0 }}>
          <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
            <Sparkles size={16} strokeWidth={1.7} style={{ color: GOLD }} />
            <span style={{ color: "#fff", fontSize: 13, fontWeight: 600 }}>AI Host Assistant</span>
          </div>
          <button
            onClick={fetchBriefing}
            disabled={briefingLoading}
            style={{
              background: "transparent",
              border: `1px solid ${GOLD}55`,
              borderRadius: 6,
              color: GOLD,
              fontSize: 11,
              fontWeight: 600,
              padding: "5px 10px",
              cursor: briefingLoading ? "default" : "pointer",
              opacity: briefingLoading ? 0.6 : 1,
            }}
          >
            {briefingLoading ? "Thinking…" : "Get Briefing"}
          </button>
        </div>
        {briefingError && <p style={{ color: "#EF4444", fontSize: 12, marginTop: 4 }}>{briefingError}</p>}
        {briefing && (
          <div style={{ display: "flex", gap: 10, alignItems: "flex-start" }}>
            <button
              onClick={toggleSpeak}
              title={speaking ? "Stop reading" : "Read briefing aloud"}
              style={{ background: "transparent", border: "none", color: speaking ? GOLD : "#6B7280", cursor: "pointer", padding: 0, marginTop: 1, flexShrink: 0, display: "flex" }}
            >
              {speaking ? <VolumeX size={15} /> : <Volume2 size={15} />}
            </button>
            <p style={{ color: "#D1D5DB", fontSize: 12.5, lineHeight: 1.7, whiteSpace: "pre-wrap", flex: 1 }}>{briefing}</p>
          </div>
        )}
      </div>

      <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 10 }}>
        Quick Actions
      </p>
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: 8, marginBottom: 20 }}>
        <QuickAction Icon={CalendarCheck} label="Add Reservation" onClick={onGoToReservations} />
        <QuickAction Icon={Lightbulb} label={property?.poolLightsOn ? "Pool Lights Off" : "Pool Lights On"} onClick={togglePoolLights} />
        <QuickAction Icon={DoorOpen} label="Update Door Code" onClick={updateDoorCode} />
        <QuickAction Icon={Megaphone} label="Broadcast Notice" onClick={broadcastNotice} />
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

function GuestRequests() {
  const [requests, setRequests] = useState<GuestRequest[]>([]);
  const [loading, setLoading] = useState(true);

  const refresh = () => {
    getGuestRequests().then((r) => {
      setRequests(r);
      setLoading(false);
    });
  };

  useEffect(() => {
    refresh();
  }, []);

  const handleMarkRead = async (id: number) => {
    await markRequestRead(id);
    refresh();
  };

  if (loading) {
    return <p style={{ color: "#4B5563", fontSize: 13, textAlign: "center", padding: "20px 0" }}>Loading…</p>;
  }

  return (
    <div>
      <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 12 }}>
        Guest Requests
      </p>
      {requests.length === 0 ? (
        <p style={{ color: "#4B5563", fontSize: 13, textAlign: "center", padding: "12px 0" }}>No requests yet</p>
      ) : (
        requests.map((r) => (
          <div key={r.id} className="lux-glass" style={{ padding: "12px 14px", marginBottom: 8, opacity: r.isRead ? 0.55 : 1 }}>
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-start", gap: 10 }}>
              <div style={{ flex: 1 }}>
                <p style={{ color: "#fff", fontSize: 13, fontWeight: 600, marginBottom: 4 }}>{r.guestName}</p>
                <p style={{ color: "#9CA3AF", fontSize: 13, lineHeight: 1.5 }}>{r.message}</p>
              </div>
              {!r.isRead && (
                <button
                  onClick={() => handleMarkRead(r.id)}
                  className="btn-press"
                  style={{ background: "transparent", border: `1px solid ${GOLD}55`, borderRadius: 6, color: GOLD, fontSize: 10, padding: "4px 8px", cursor: "pointer", flexShrink: 0, textTransform: "uppercase", letterSpacing: "0.05em" }}
                >
                  Mark Read
                </button>
              )}
            </div>
          </div>
        ))
      )}
    </div>
  );
}

function PropertyStatus() {
  const [property, setProperty] = useState<PropertyState | null>(null);

  const refresh = () => {
    getPropertyState().then(setProperty);
  };

  useEffect(() => {
    refresh();
  }, []);

  const togglePoolLights = async () => {
    if (!property) return;
    const next = !property.poolLightsOn;
    setProperty({ ...property, poolLightsOn: next });
    await updatePropertyState({ poolLightsOn: next });
  };

  const updateDoorCode = async () => {
    const next = window.prompt("New door code:", property?.doorCode || "");
    if (!next || !next.trim()) return;
    const result = await updatePropertyState({ doorCode: next.trim() });
    if (result.ok) refresh();
  };

  return (
    <div>
      <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 12 }}>
        Property Status
      </p>

      <div className="lux-glass" style={{ padding: "12px 14px", marginBottom: 8, display: "flex", alignItems: "center", gap: 12 }}>
        <Wifi size={18} strokeWidth={1.7} style={{ color: GOLD, flexShrink: 0 }} />
        <span style={{ color: "#E5E7EB", fontSize: 13, flex: 1 }}>Wi-Fi Network</span>
        <span className="status-pill ready" style={{ fontSize: 10 }}>● Online</span>
      </div>

      <button
        onClick={togglePoolLights}
        className="lux-glass btn-press"
        style={{ width: "100%", border: "none", cursor: "pointer", padding: "12px 14px", marginBottom: 8, display: "flex", alignItems: "center", gap: 12, textAlign: "left" }}
      >
        <Waves size={18} strokeWidth={1.7} style={{ color: GOLD, flexShrink: 0 }} />
        <span style={{ color: "#E5E7EB", fontSize: 13, flex: 1 }}>Pool & Spa</span>
        <span className="status-pill ready" style={{ fontSize: 10 }}>● {property?.poolLightsOn ? "Lit" : "Off"}</span>
      </button>

      <div className="lux-glass" style={{ padding: "12px 14px", marginBottom: 8, display: "flex", alignItems: "center", gap: 12 }}>
        <Clapperboard size={18} strokeWidth={1.7} style={{ color: GOLD, flexShrink: 0 }} />
        <span style={{ color: "#E5E7EB", fontSize: 13, flex: 1 }}>Theater & Studio</span>
        <span className="status-pill ready" style={{ fontSize: 10 }}>● Ready</span>
      </div>

      <button
        onClick={updateDoorCode}
        className="lux-glass btn-press"
        style={{ width: "100%", border: "none", cursor: "pointer", padding: "12px 14px", display: "flex", alignItems: "center", gap: 12, textAlign: "left" }}
      >
        <DoorOpen size={18} strokeWidth={1.7} style={{ color: GOLD, flexShrink: 0 }} />
        <span style={{ color: "#E5E7EB", fontSize: 13, flex: 1 }}>Smart Door Lock</span>
        <span style={{ color: GOLD, fontSize: 12, fontFamily: "monospace" }}>{property?.doorCode || "—"}</span>
      </button>
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

  const handleMessage = async (code: string, guestName: string) => {
    const text = window.prompt(`Welcome message for ${guestName}:`, "");
    if (!text || !text.trim()) return;
    const result = await sendWelcomeMessage(code, text.trim());
    if (!result.ok) {
      window.alert(result.error || "Failed to send message.");
    }
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
              <div style={{ display: "flex", gap: 10, alignItems: "center" }}>
                <button
                  onClick={() => handleMessage(b.confirmationCode, b.guestName)}
                  title="Send welcome message"
                  style={{ background: "transparent", border: "none", color: GOLD, cursor: "pointer", padding: 0, display: "flex" }}
                >
                  <MessageCircle size={15} />
                </button>
                <button
                  onClick={() => handleDelete(b.confirmationCode)}
                  style={{ background: "transparent", border: "none", color: "#6B7280", cursor: "pointer", fontSize: 16, lineHeight: 1, padding: 0 }}
                >
                  ×
                </button>
              </div>
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

type DashTab = "overview" | "reservations" | "requests" | "guest" | "property" | "settings";

const DASH_TABS: { key: DashTab; label: string; Icon: LucideIcon }[] = [
  { key: "overview",     label: "Overview",     Icon: LayoutDashboard   },
  { key: "reservations", label: "Reservations", Icon: CalendarCheck     },
  { key: "requests",     label: "Requests",     Icon: MessageSquareText },
  { key: "guest",        label: "Guest",        Icon: UserRound         },
  { key: "property",     label: "Property",     Icon: KeyRound          },
  { key: "settings",     label: "Settings",     Icon: SettingsIcon      },
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

              {tab === "overview" && <Overview onGoToReservations={() => setTab("reservations")} />}
              {tab === "requests" && <GuestRequests />}
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
