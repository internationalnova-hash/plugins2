"use client";

import { useState, useEffect, useRef } from "react";
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
  Pencil,
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
  Wand2,
  Gift,
  type LucideIcon,
} from "lucide-react";
import StayBuilder from "@/components/StayBuilder";
import IntelligenceCenter from "@/components/IntelligenceCenter";
import { getBookings, addBooking, updateBooking, deleteBooking, loginHost, isHostLoggedIn, logoutHost, type Booking, type BookingSource } from "@/lib/bookingsStore";
import { getJourneyStage } from "@/lib/novaJourney";
import {
  getPropertyState,
  updatePropertyState,
  uploadHeroImage,
  getGuestRequests,
  markRequestRead,
  sendWelcomeMessage,
  getHostBriefing,
  getExperienceRequests,
  updateExperienceRequestStatus,
  getRecommendations,
  sendInboxMessage,
  type PropertyState,
  type GuestRequest,
  type RequestPriority,
  type RequestCategory,
  type ExperienceRequest,
  type ExperienceRequestStatus,
  type TodaysFocus,
  type AiRecommendation,
} from "@/lib/propertyStore";
import type { WeatherInfo } from "@/lib/weather";
import SNMonogram from "@/components/SNMonogram";
import theme from "@/config/theme";
import propertyConfig from "@/config/property";

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
  const [briefingAt, setBriefingAt] = useState<number | null>(null);
  const [now, setNow] = useState<number>(Date.now());
  const [speaking, setSpeaking] = useState(false);
  const [question, setQuestion] = useState("");
  const [conversation, setConversation] = useState<{ q: string; a: string }[]>([]);
  const [asking, setAsking] = useState(false);
  const [focus, setFocus] = useState<TodaysFocus | null>(null);
  const [recommendations, setRecommendations] = useState<AiRecommendation[]>([]);
  const [recsLoading, setRecsLoading] = useState(true);
  const [sentRecIds, setSentRecIds] = useState<Set<string>>(new Set());

  const sendRecommendation = async (rec: AiRecommendation) => {
    if (!rec.confirmationCode || !rec.suggestedMessage) return;
    setSentRecIds((prev) => new Set(prev).add(rec.id));
    const result = await sendInboxMessage(rec.confirmationCode, rec.suggestedMessage);
    if (!result.ok) {
      setSentRecIds((prev) => {
        const next = new Set(prev);
        next.delete(rec.id);
        return next;
      });
    }
  };

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
    setBriefingAt(Date.now());
  };

  const askQuestion = async (text: string) => {
    const trimmed = text.trim();
    if (!trimmed || asking) return;
    setAsking(true);
    setQuestion("");
    const result = await getHostBriefing(trimmed);
    setAsking(false);
    setConversation((prev) => [...prev, { q: trimmed, a: result.ok ? result.briefing || "" : result.error || "Something went wrong." }]);
  };

  useEffect(() => {
    const t = setInterval(() => setNow(Date.now()), 5000);
    return () => clearInterval(t);
  }, []);

  const agoLabel = (() => {
    if (!briefingAt) return null;
    const secs = Math.max(0, Math.round((now - briefingAt) / 1000));
    if (secs < 5) return "Updated just now";
    if (secs < 60) return `Updated ${secs}s ago`;
    const mins = Math.round(secs / 60);
    return `Updated ${mins}m ago`;
  })();

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
    fetchBriefing();
    fetch("/api/weather").then((res) => (res.ok ? res.json() : null)).then((data) => {
      if (data && !data.error) setWeather(data);
    }).catch(() => {});
    getRecommendations().then((result) => {
      if (result) {
        setFocus(result.todaysFocus);
        setRecommendations(result.recommendations);
      }
      setRecsLoading(false);
    });
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

  const greeting = (() => {
    const hour = new Date().getHours();
    if (hour < 12) return "Good Morning";
    if (hour < 18) return "Good Afternoon";
    return "Good Evening";
  })();

  return (
    <div>
      <p style={{ color: "#fff", fontSize: 16, fontWeight: 600, marginBottom: 4 }}>
        {greeting}, {propertyConfig.hostName}.
      </p>
      <p style={{ color: "#6B7280", fontSize: 12.5, marginBottom: 16 }}>
        {focus ? "Everything is ready for today." : "Loading today's overview…"}
      </p>

      {focus && (
        <div className="lux-glass" style={{ padding: "14px 16px", marginBottom: 16 }}>
          <p style={{ color: "#6B7280", fontSize: 10.5, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 10 }}>
            Today's Focus
          </p>
          <div style={{ display: "flex", flexWrap: "wrap", gap: 14 }}>
            <span style={{ color: "#D1D5DB", fontSize: 13 }}>🏡 {focus.arrivals} arrival{focus.arrivals === 1 ? "" : "s"}</span>
            <span style={{ color: "#D1D5DB", fontSize: 13 }}>🛫 {focus.checkouts} checkout{focus.checkouts === 1 ? "" : "s"}</span>
            {focus.tempF !== null && <span style={{ color: "#D1D5DB", fontSize: 13 }}>☀️ {focus.tempF}°</span>}
            <span style={{ color: "#D1D5DB", fontSize: 13 }}>📬 {focus.pendingExperienceRequests} experience request{focus.pendingExperienceRequests === 1 ? "" : "s"}</span>
          </div>
        </div>
      )}

      {!recsLoading && recommendations.length > 0 && (
        <div className="lux-glass" style={{ padding: "14px 16px", marginBottom: 16 }}>
          <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 10 }}>
            <Sparkles size={14} strokeWidth={1.7} style={{ color: GOLD }} />
            <span style={{ color: "#6B7280", fontSize: 10.5, letterSpacing: "0.15em", textTransform: "uppercase" }}>AI Recommendations</span>
          </div>
          <div style={{ display: "flex", flexDirection: "column", gap: 10 }}>
            {recommendations.map((r) => (
              <div key={r.id} style={{ display: "flex", alignItems: "flex-start", gap: 8 }}>
                <p style={{ color: "#D1D5DB", fontSize: 12.5, lineHeight: 1.6, flex: 1 }}>
                  <span style={{ color: GOLD }}>⭐ </span>{r.text}
                </p>
                {r.confirmationCode && r.suggestedMessage && (
                  <button
                    onClick={() => sendRecommendation(r)}
                    disabled={sentRecIds.has(r.id)}
                    className="btn-press"
                    style={{
                      flexShrink: 0,
                      background: "transparent",
                      border: `1px solid ${sentRecIds.has(r.id) ? `${GOLD}55` : "#1e1e1e"}`,
                      borderRadius: 6,
                      color: sentRecIds.has(r.id) ? GOLD : "#9CA3AF",
                      fontSize: 10.5,
                      fontWeight: 600,
                      padding: "4px 10px",
                      cursor: sentRecIds.has(r.id) ? "default" : "pointer",
                      textTransform: "uppercase",
                      letterSpacing: "0.05em",
                    }}
                  >
                    {sentRecIds.has(r.id) ? "✓ Sent" : "Send"}
                  </button>
                )}
              </div>
            ))}
          </div>
        </div>
      )}

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

      <div className="lux-glass" style={{ padding: "16px 16px", marginBottom: 16 }}>
        <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: 4 }}>
          <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
            <Sparkles size={16} strokeWidth={1.7} style={{ color: GOLD }} />
            <span style={{ color: "#fff", fontSize: 13, fontWeight: 600 }}>AI Operations Manager</span>
          </div>
          <button
            onClick={fetchBriefing}
            disabled={briefingLoading}
            style={{
              background: "transparent",
              border: "none",
              color: GOLD,
              fontSize: 11,
              fontWeight: 600,
              padding: 0,
              cursor: briefingLoading ? "default" : "pointer",
              opacity: briefingLoading ? 0.6 : 1,
              display: "flex",
              alignItems: "center",
              gap: 4,
            }}
          >
            ↻ {briefingLoading ? "Analyzing…" : "Refresh Analysis"}
          </button>
        </div>
        {agoLabel && !briefingLoading && (
          <p style={{ color: "#4B5563", fontSize: 10.5, marginBottom: 10 }}>{agoLabel}</p>
        )}
        {briefingError && <p style={{ color: "#EF4444", fontSize: 12, marginTop: 4 }}>{briefingError}</p>}
        {briefing && (
          <div style={{ display: "flex", gap: 10, alignItems: "flex-start", marginBottom: 14 }}>
            <button
              onClick={toggleSpeak}
              title={speaking ? "Stop reading" : "Read briefing aloud"}
              style={{ background: "transparent", border: "none", color: speaking ? GOLD : "#6B7280", cursor: "pointer", padding: 0, marginTop: 1, flexShrink: 0, display: "flex" }}
            >
              {speaking ? <VolumeX size={15} /> : <Volume2 size={15} />}
            </button>
            <p style={{ color: "#D1D5DB", fontSize: 12.5, lineHeight: 1.75, whiteSpace: "pre-wrap", flex: 1 }}>{briefing}</p>
          </div>
        )}

        <div style={{ height: 1, background: "#1e1e1e", margin: "4px 0 12px" }} />

        {conversation.length > 0 && (
          <div style={{ marginBottom: 12, display: "flex", flexDirection: "column", gap: 12 }}>
            {conversation.map((c, i) => (
              <div key={i}>
                <p style={{ color: GOLD, fontSize: 11.5, fontWeight: 600, marginBottom: 4 }}>{c.q}</p>
                <p style={{ color: "#D1D5DB", fontSize: 12.5, lineHeight: 1.7, whiteSpace: "pre-wrap" }}>{c.a}</p>
              </div>
            ))}
          </div>
        )}
        {asking && <p style={{ color: "#6B7280", fontSize: 12, marginBottom: 12 }}>Thinking…</p>}

        <form
          onSubmit={(e) => {
            e.preventDefault();
            askQuestion(question);
          }}
          style={{ display: "flex", gap: 8, marginBottom: 10 }}
        >
          <input
            value={question}
            onChange={(e) => setQuestion(e.target.value)}
            placeholder="Ask your Operations Manager…"
            style={{ ...inputStyle, marginBottom: 0, flex: 1 }}
          />
          <button
            type="submit"
            disabled={asking || !question.trim()}
            style={{
              background: "transparent",
              border: `1px solid ${GOLD}55`,
              borderRadius: 6,
              color: GOLD,
              fontSize: 12,
              fontWeight: 600,
              padding: "0 14px",
              cursor: asking || !question.trim() ? "default" : "pointer",
              opacity: asking || !question.trim() ? 0.5 : 1,
            }}
          >
            Ask
          </button>
        </form>
        <div style={{ display: "flex", flexWrap: "wrap", gap: 6 }}>
          {[
            "What needs my attention today?",
            "Which guests arrive this weekend?",
            "Who hasn't completed onboarding?",
            "Show me reservations longer than 5 nights.",
          ].map((example) => (
            <button
              key={example}
              onClick={() => askQuestion(example)}
              disabled={asking}
              style={{
                background: "rgba(255,255,255,0.03)",
                border: "1px solid #1e1e1e",
                borderRadius: 999,
                color: "#9CA3AF",
                fontSize: 10.5,
                padding: "5px 10px",
                cursor: asking ? "default" : "pointer",
              }}
            >
              {example}
            </button>
          ))}
        </div>
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

const PRIORITY_COLOR: Record<RequestPriority, string> = {
  urgent: "#EF4444",
  high: "#F59E0B",
  medium: "#9CA3AF",
  low: "#6B7280",
};

const CATEGORY_LABEL: Record<RequestCategory, string> = {
  property_issue: "Property Issue",
  late_checkout: "Late Checkout",
  upsell_opportunity: "Upsell Opportunity",
  question: "Question",
  complaint: "Complaint",
  general: "General",
};

function GuestRequests() {
  const [requests, setRequests] = useState<GuestRequest[]>([]);
  const [loading, setLoading] = useState(true);
  const [repliedIds, setRepliedIds] = useState<Set<number>>(new Set());

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

  const handleReply = async (r: GuestRequest) => {
    if (!r.confirmationCode) return;
    const text = window.prompt(`Reply to ${r.guestName}:`, r.suggestedResponse || "");
    if (!text || !text.trim()) return;
    const result = await sendInboxMessage(r.confirmationCode, text.trim());
    if (result.ok) {
      setRepliedIds((prev) => new Set(prev).add(r.id));
      if (!r.isRead) handleMarkRead(r.id);
    }
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
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-start", gap: 10, marginBottom: 8 }}>
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

            {(r.priority || r.category) && (
              <div style={{ display: "flex", flexWrap: "wrap", gap: 6, marginBottom: 8 }}>
                {r.priority && (
                  <span
                    className="status-pill"
                    style={{ fontSize: 9.5, color: PRIORITY_COLOR[r.priority], borderColor: PRIORITY_COLOR[r.priority] }}
                  >
                    {r.priority.toUpperCase()}
                  </span>
                )}
                {r.category && (
                  <span className="status-pill" style={{ fontSize: 9.5, color: "#9CA3AF" }}>
                    {CATEGORY_LABEL[r.category]}
                  </span>
                )}
              </div>
            )}

            {r.suggestedResponse && (
              <p style={{ color: "#6B7280", fontSize: 11.5, lineHeight: 1.5, marginBottom: 8, fontStyle: "italic" }}>
                Suggested: “{r.suggestedResponse}”
              </p>
            )}

            {r.confirmationCode && (
              <button
                onClick={() => handleReply(r)}
                className="btn-press"
                style={{
                  background: "transparent",
                  border: `1px solid ${repliedIds.has(r.id) ? `${GOLD}55` : "#1e1e1e"}`,
                  borderRadius: 6,
                  color: repliedIds.has(r.id) ? GOLD : "#9CA3AF",
                  fontSize: 10.5,
                  fontWeight: 600,
                  padding: "5px 10px",
                  cursor: "pointer",
                  textTransform: "uppercase",
                  letterSpacing: "0.05em",
                }}
              >
                {repliedIds.has(r.id) ? "✓ Replied" : "Reply"}
              </button>
            )}
          </div>
        ))
      )}
    </div>
  );
}

const EXP_STATUS_LABEL: Record<ExperienceRequestStatus, string> = {
  requested: "Requested",
  approved: "Approved",
  billed: "Billed",
  paid: "Paid",
  scheduled: "Scheduled",
  completed: "Completed",
  declined: "Declined",
};

const EXP_NEXT_STATUS: Partial<Record<ExperienceRequestStatus, ExperienceRequestStatus>> = {
  requested: "approved",
  approved: "billed",
  billed: "paid",
  paid: "scheduled",
  scheduled: "completed",
};

function buildOfferMessage(r: ExperienceRequest): string {
  const platform = r.bookingSource === "airbnb" ? "Airbnb" : r.bookingSource === "vrbo" ? "VRBO" : null;
  if (platform) {
    return `Hi ${r.guestName}! We'd be happy to arrange the ${r.experienceTitle} for your stay. Per ${platform}'s policies, I'll send you a Special Offer through ${platform}${r.priceFrom ? ` for ${r.priceFrom}` : ""}. Once accepted, everything will be confirmed.`;
  }
  return `Hi ${r.guestName}! We'd be happy to arrange the ${r.experienceTitle} for your stay${r.priceFrom ? ` (${r.priceFrom})` : ""}. I'll follow up with payment details to get it confirmed.`;
}

function ExperienceRequests() {
  const [requests, setRequests] = useState<ExperienceRequest[]>([]);
  const [loading, setLoading] = useState(true);
  const [copiedId, setCopiedId] = useState<number | null>(null);

  const refresh = () => {
    getExperienceRequests().then((r) => {
      setRequests(r);
      setLoading(false);
    });
  };

  useEffect(() => {
    refresh();
  }, []);

  const advance = async (r: ExperienceRequest) => {
    const next = EXP_NEXT_STATUS[r.status];
    if (!next) return;
    if (next === "billed" && (r.bookingSource === "airbnb" || r.bookingSource === "vrbo")) {
      navigator.clipboard.writeText(buildOfferMessage(r)).then(() => {
        setCopiedId(r.id);
        setTimeout(() => setCopiedId(null), 2000);
      });
    }
    await updateExperienceRequestStatus(r.id, next);
    refresh();
  };

  const decline = async (r: ExperienceRequest) => {
    await updateExperienceRequestStatus(r.id, "declined");
    refresh();
  };

  if (loading) {
    return <p style={{ color: "#4B5563", fontSize: 13, textAlign: "center", padding: "20px 0" }}>Loading…</p>;
  }

  return (
    <div>
      <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 12 }}>
        Experience Requests
      </p>
      {requests.length === 0 ? (
        <p style={{ color: "#4B5563", fontSize: 13, textAlign: "center", padding: "12px 0" }}>No experience requests yet</p>
      ) : (
        requests.map((r) => (
          <div key={r.id} className="lux-glass" style={{ padding: "12px 14px", marginBottom: 8 }}>
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-start", gap: 10, marginBottom: 8 }}>
              <div style={{ flex: 1 }}>
                <p style={{ color: "#fff", fontSize: 13, fontWeight: 600 }}>{r.experienceTitle}</p>
                <p style={{ color: "#9CA3AF", fontSize: 11.5, marginTop: 2 }}>
                  {r.guestName} {r.priceFrom ? `· ${r.priceFrom}` : ""}
                </p>
                <p style={{ color: "#4B5563", fontSize: 10, marginTop: 2, textTransform: "uppercase", letterSpacing: "0.05em" }}>
                  via {r.bookingSource}
                </p>
              </div>
              <span
                className="status-pill"
                style={{
                  fontSize: 9.5,
                  flexShrink: 0,
                  color: r.status === "declined" ? "#6B7280" : GOLD,
                  borderColor: r.status === "declined" ? "#6B7280" : undefined,
                }}
              >
                {EXP_STATUS_LABEL[r.status]}
              </span>
            </div>
            {r.status !== "completed" && r.status !== "declined" && (
              <div style={{ display: "flex", gap: 8 }}>
                {EXP_NEXT_STATUS[r.status] && (
                  <button
                    onClick={() => advance(r)}
                    className="btn-press"
                    style={{ flex: 1, background: "transparent", border: `1px solid ${GOLD}55`, borderRadius: 6, color: GOLD, fontSize: 10.5, padding: "6px 8px", cursor: "pointer", textTransform: "uppercase", letterSpacing: "0.05em" }}
                  >
                    {copiedId === r.id
                      ? "✓ Copied message"
                      : r.status === "approved" && (r.bookingSource === "airbnb" || r.bookingSource === "vrbo")
                      ? `Copy ${r.bookingSource === "airbnb" ? "Airbnb" : "VRBO"} Offer`
                      : `Mark ${EXP_STATUS_LABEL[EXP_NEXT_STATUS[r.status]!]}`}
                  </button>
                )}
                {r.status === "requested" && (
                  <button
                    onClick={() => decline(r)}
                    className="btn-press"
                    style={{ background: "transparent", border: "1px solid #1e1e1e", borderRadius: 6, color: "#6B7280", fontSize: 10.5, padding: "6px 10px", cursor: "pointer" }}
                  >
                    Decline
                  </button>
                )}
              </div>
            )}
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
  const [property, setProperty] = useState<PropertyState | null>(null);
  const [form, setForm] = useState({ propertyName: "", hostName: "", city: "", tagline: "", amenities: "", goldColor: "" });
  const [heroPreview, setHeroPreview] = useState<string | null>(null);
  const [uploading, setUploading] = useState(false);
  const [saving, setSaving] = useState(false);
  const [saved, setSaved] = useState(false);
  const [activeSlug, setActiveSlug] = useState("");
  const fileInputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    setActiveSlug(localStorage.getItem("nova_active_property_slug") || "");
  }, []);

  useEffect(() => {
    getPropertyState().then((p) => {
      if (!p) return;
      setProperty(p);
      setHeroPreview(p.heroImageUrl);
      setForm({
        propertyName: p.propertyName || propertyConfig.name,
        hostName: p.hostName || propertyConfig.hostName,
        city: p.city || propertyConfig.city,
        tagline: p.tagline || propertyConfig.tagline,
        amenities: p.amenities || propertyConfig.amenities.join(", "),
        goldColor: p.goldColor || GOLD,
      });
    });
  }, []);

  const onPickFile = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    setUploading(true);
    const result = await uploadHeroImage(file);
    setUploading(false);
    if (result.ok && result.url) {
      setHeroPreview(result.url);
      await updatePropertyState({ heroImageUrl: result.url });
      setSaved(true);
      setTimeout(() => setSaved(false), 2000);
    } else {
      alert(result.error || "Upload failed. Please try a smaller image.");
    }
  };

  const saveIdentity = async () => {
    setSaving(true);
    await updatePropertyState({
      propertyName: form.propertyName,
      hostName: form.hostName,
      city: form.city,
      tagline: form.tagline,
      amenities: form.amenities,
      goldColor: form.goldColor,
    });
    setSaving(false);
    setSaved(true);
    setTimeout(() => setSaved(false), 2000);
  };

  return (
    <div>
      <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 12 }}>
        Property Identity
      </p>

      <div
        onClick={() => fileInputRef.current?.click()}
        style={{
          position: "relative",
          height: 140,
          borderRadius: 12,
          marginBottom: 10,
          overflow: "hidden",
          cursor: "pointer",
          border: `1px solid ${GOLD}33`,
          backgroundImage: `url('${heroPreview || propertyConfig.heroImage}')`,
          backgroundSize: "cover",
          backgroundPosition: "center",
        }}
      >
        <div
          style={{
            position: "absolute",
            inset: 0,
            background: "rgba(0,0,0,0.45)",
            display: "flex",
            alignItems: "center",
            justifyContent: "center",
            color: "#fff",
            fontSize: 12,
            fontWeight: 600,
          }}
        >
          {uploading ? "Uploading…" : "Tap to upload hero image"}
        </div>
      </div>
      <input ref={fileInputRef} type="file" accept="image/jpeg,image/png,image/webp,image/avif" onChange={onPickFile} style={{ display: "none" }} />

      <input style={inputStyle} placeholder="Property name" value={form.propertyName} onChange={(e) => setForm({ ...form, propertyName: e.target.value })} />
      <input style={inputStyle} placeholder="Host name" value={form.hostName} onChange={(e) => setForm({ ...form, hostName: e.target.value })} />
      <input style={inputStyle} placeholder="City" value={form.city} onChange={(e) => setForm({ ...form, city: e.target.value })} />
      <input style={inputStyle} placeholder="Tagline" value={form.tagline} onChange={(e) => setForm({ ...form, tagline: e.target.value })} />
      <input style={inputStyle} placeholder="Amenities (comma separated)" value={form.amenities} onChange={(e) => setForm({ ...form, amenities: e.target.value })} />

      <div style={{ display: "flex", alignItems: "center", gap: 10, marginBottom: 10 }}>
        <input
          type="color"
          value={form.goldColor}
          onChange={(e) => setForm({ ...form, goldColor: e.target.value })}
          style={{ width: 40, height: 36, border: "1px solid #2a2a2a", borderRadius: 8, background: "transparent", cursor: "pointer", padding: 0 }}
        />
        <p style={{ color: "#9CA3AF", fontSize: 12 }}>Brand accent color</p>
      </div>

      <button
        onClick={saveIdentity}
        disabled={saving}
        className="btn-press"
        style={{
          width: "100%",
          background: `linear-gradient(135deg, ${GOLD}, ${GOLD_LIGHT})`,
          color: BG,
          border: "none",
          borderRadius: 8,
          padding: "11px",
          fontSize: 12,
          fontWeight: 700,
          marginBottom: saved ? 6 : 18,
          cursor: saving ? "default" : "pointer",
          opacity: saving ? 0.6 : 1,
        }}
      >
        {saving ? "Saving…" : "Save Property Identity"}
      </button>
      {saved && <p style={{ color: GOLD, fontSize: 12, marginBottom: 12 }}>Saved — reload to see changes everywhere.</p>}

      <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 12, marginTop: 18 }}>
        Settings
      </p>
      <button
        onClick={() => {
          if (!window.confirm("Clear this device's check-in data? The guest app will reload to the confirmation code screen.")) return;
          localStorage.removeItem("stayByNova_guestInfo");
          window.location.reload();
        }}
        className="lux-glass btn-press"
        style={{ width: "100%", display: "flex", alignItems: "center", gap: 10, padding: "12px 14px", marginBottom: 8, color: "#9CA3AF", fontSize: 13, cursor: "pointer", border: "none" }}
      >
        <Trash2 size={16} strokeWidth={1.8} style={{ color: GOLD }} />
        Clear Current Guest Data
      </button>
      {activeSlug && (
        <button
          onClick={() => window.open(`/${activeSlug}/qr`, "_blank")}
          className="lux-glass btn-press"
          style={{ width: "100%", display: "flex", alignItems: "center", gap: 10, padding: "12px 14px", marginBottom: 8, color: "#9CA3AF", fontSize: 13, cursor: "pointer", border: "none" }}
        >
          <span style={{ fontSize: 16 }}>📱</span>
          Print QR Code for Guests
        </button>
      )}
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

const EMPTY_FORM = { guestName: "", confirmationCode: "", checkIn: "", checkOut: "", nights: "1", bookedGuests: "1", bookingSource: "airbnb" as BookingSource };

const BOOKING_SOURCES: { value: BookingSource; label: string }[] = [
  { value: "airbnb",      label: "Airbnb" },
  { value: "vrbo",        label: "VRBO" },
  { value: "direct",      label: "Direct Booking" },
  { value: "booking.com", label: "Booking.com" },
  { value: "other",       label: "Other" },
];

interface LoginProperty {
  id: number;
  slug: string;
  name: string;
}

function HostLogin({ onSuccess }: { onSuccess: () => void }) {
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

  const submit = async () => {
    if (!email.trim() || !password.trim() || loading) return;
    setLoading(true);
    setError(null);
    try {
      const res = await fetch("/api/auth/login", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ email: email.trim(), password: password.trim() }),
      });
      const data = await res.json().catch(() => ({}));
      setLoading(false);
      if (!res.ok || !data.ok) {
        setError(data.error || "Incorrect email or password.");
        return;
      }
      localStorage.setItem("nova_session_token", data.sessionToken);
      const properties: LoginProperty[] = data.properties || [];
      localStorage.setItem("nova_host_properties", JSON.stringify(properties));
      if (properties.length > 0) {
        localStorage.setItem("nova_active_property_slug", properties[0].slug);
      }
      if (data.host) {
        localStorage.setItem("nova_host_info", JSON.stringify(data.host));
      }
      onSuccess();
    } catch {
      setLoading(false);
      setError("Login failed. Please try again.");
    }
  };

  return (
    <div>
      <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 12 }}>
        Host Login Required
      </p>
      <input
        style={inputStyle}
        type="email"
        placeholder="Email"
        value={email}
        onChange={(e) => setEmail(e.target.value)}
        onKeyDown={(e) => e.key === "Enter" && submit()}
      />
      <input
        style={inputStyle}
        type="password"
        placeholder="Password"
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

function PropertySwitcher({ onSwitchToBuilder }: { onSwitchToBuilder: () => void }) {
  const [properties, setProperties] = useState<LoginProperty[]>([]);
  const [active, setActive] = useState<string>("");

  useEffect(() => {
    try {
      const raw = localStorage.getItem("nova_host_properties");
      const parsed: LoginProperty[] = raw ? JSON.parse(raw) : [];
      setProperties(parsed);
      setActive(localStorage.getItem("nova_active_property_slug") || "");
    } catch {
      setProperties([]);
    }
  }, []);

  const handleChange = (e: React.ChangeEvent<HTMLSelectElement>) => {
    const slug = e.target.value;
    setActive(slug);
    localStorage.setItem("nova_active_property_slug", slug);
    window.location.reload();
  };

  const addProperty = async () => {
    const name = window.prompt("New property name:");
    if (!name || !name.trim()) return;
    const slug = window.prompt("New property URL slug (e.g. lake-house):");
    if (!slug || !slug.trim()) return;

    const token = localStorage.getItem("nova_session_token");
    try {
      const res = await fetch("/api/properties", {
        method: "POST",
        headers: { "Content-Type": "application/json", ...(token ? { "x-session-token": token } : {}) },
        body: JSON.stringify({ slug: slug.trim(), name: name.trim() }),
      });
      const data = await res.json().catch(() => ({}));
      if (!res.ok || !data.ok) {
        window.alert(data.error || "Failed to create property.");
        return;
      }
      const next = [...properties, data.property];
      setProperties(next);
      localStorage.setItem("nova_host_properties", JSON.stringify(next));
      localStorage.setItem("nova_active_property_slug", data.property.slug);
      setActive(data.property.slug);
      onSwitchToBuilder();
    } catch {
      window.alert("Failed to create property.");
    }
  };

  return (
    <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 12 }}>
      {properties.length > 1 && (
        <select value={active} onChange={handleChange} style={{ ...inputStyle, marginBottom: 0, flex: 1 }}>
          {properties.map((p) => (
            <option key={p.slug} value={p.slug}>{p.name}</option>
          ))}
        </select>
      )}
      <button
        onClick={addProperty}
        className="btn-press"
        style={{
          flexShrink: 0,
          background: "transparent",
          border: `1px solid ${GOLD}55`,
          borderRadius: 6,
          color: GOLD,
          fontSize: 10.5,
          fontWeight: 600,
          padding: "6px 10px",
          cursor: "pointer",
          textTransform: "uppercase",
          letterSpacing: "0.05em",
          whiteSpace: "nowrap",
        }}
      >
        + Add Property
      </button>
    </div>
  );
}

function ReservationManager() {
  const [bookings, setBookings] = useState<Booking[]>([]);
  const [form, setForm] = useState(EMPTY_FORM);
  const [editingCode, setEditingCode] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  const refresh = () => { getBookings().then(setBookings); };
  useEffect(() => { refresh(); }, []);

  const setField = (key: keyof typeof EMPTY_FORM) => (e: React.ChangeEvent<HTMLInputElement | HTMLSelectElement>) =>
    setForm((f) => ({ ...f, [key]: e.target.value }));

  const startEdit = (b: Booking) => {
    setEditingCode(b.confirmationCode);
    setForm({
      guestName: b.guestName,
      confirmationCode: b.confirmationCode,
      checkIn: b.checkIn,
      checkOut: b.checkOut,
      nights: String(b.nights),
      bookedGuests: String(b.bookedGuests),
      bookingSource: b.bookingSource,
    });
    setError(null);
  };

  const cancelEdit = () => { setEditingCode(null); setForm(EMPTY_FORM); setError(null); };

  const handleSaveEdit = async () => {
    const { guestName, confirmationCode, checkIn, checkOut, nights, bookedGuests, bookingSource } = form;
    if (!guestName.trim() || !checkIn.trim() || !checkOut.trim()) {
      setError("Guest name and dates are required.");
      return;
    }
    const result = await updateBooking({
      guestName: guestName.trim(),
      confirmationCode,
      checkIn: checkIn.trim(),
      checkOut: checkOut.trim(),
      nights: Number(nights) || 1,
      bookedGuests: Number(bookedGuests) || 1,
      bookingSource,
    });
    if (!result.ok) { setError(result.error || "Failed to update reservation."); return; }
    refresh();
    cancelEdit();
  };

  const handleAdd = async () => {
    const { guestName, confirmationCode, checkIn, checkOut, nights, bookedGuests, bookingSource } = form;
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
      bookingSource,
    });
    if (!result.ok) { setError(result.error || "Failed to add reservation."); return; }
    refresh();
    setForm(EMPTY_FORM);
    setError(null);
  };

  const handleDelete = async (code: string) => { await deleteBooking(code); refresh(); };

  const handleMarkCheckedIn = async (code: string) => {
    await fetch("/api/checkin", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ confirmationCode: code }),
    });
    refresh();
  };

  const handleMessage = async (code: string, guestName: string) => {
    const text = window.prompt(`Welcome message for ${guestName}:`, "");
    if (!text || !text.trim()) return;
    const result = await sendWelcomeMessage(code, text.trim());
    if (!result.ok) window.alert(result.error || "Failed to send message.");
  };

  return (
    <div>
      <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 12 }}>
        Active Reservations
      </p>

      {bookings.length === 0 ? (
        <p style={{ color: "#4B5563", fontSize: 13, textAlign: "center", padding: "12px 0" }}>No reservations yet</p>
      ) : (
        <div style={{ marginBottom: 16 }}>
          {bookings.map((b) => (
            <div key={b.confirmationCode}>
              {editingCode === b.confirmationCode ? (
                <div className="lux-glass" style={{ padding: "14px", marginBottom: 8 }}>
                  <p style={{ color: GOLD, fontSize: 11, fontFamily: "monospace", marginBottom: 10 }}>{b.confirmationCode}</p>
                  <input style={inputStyle} placeholder="Guest Name" value={form.guestName} onChange={setField("guestName")} />
                  <div style={{ display: "flex", gap: 8 }}>
                    <input style={inputStyle} placeholder="Check-In" value={form.checkIn} onChange={setField("checkIn")} />
                    <input style={inputStyle} placeholder="Check-Out" value={form.checkOut} onChange={setField("checkOut")} />
                  </div>
                  <div style={{ display: "flex", gap: 8 }}>
                    <input style={inputStyle} type="number" min={1} placeholder="Nights" value={form.nights} onChange={setField("nights")} />
                    <input style={inputStyle} type="number" min={1} placeholder="Guests" value={form.bookedGuests} onChange={setField("bookedGuests")} />
                  </div>
                  <select style={inputStyle} value={form.bookingSource} onChange={setField("bookingSource")}>
                    {BOOKING_SOURCES.map((s) => <option key={s.value} value={s.value}>{s.label}</option>)}
                  </select>
                  {error && <p style={{ color: "#d96b6b", fontSize: 12, marginBottom: 8 }}>{error}</p>}
                  <div style={{ display: "flex", gap: 8 }}>
                    <button
                      onClick={handleSaveEdit}
                      style={{ flex: 1, background: `linear-gradient(135deg, ${GOLD}, ${GOLD_LIGHT})`, color: BG, border: "none", borderRadius: 8, padding: "10px", fontSize: 12, fontWeight: 700, cursor: "pointer" }}
                    >
                      Save
                    </button>
                    <button
                      onClick={cancelEdit}
                      style={{ flex: 1, background: "#1a1a1a", color: "#9CA3AF", border: "1px solid #2a2a2a", borderRadius: 8, padding: "10px", fontSize: 12, cursor: "pointer" }}
                    >
                      Cancel
                    </button>
                  </div>
                </div>
              ) : (
                <div
                  className="lux-glass"
                  style={{ padding: "12px 14px", marginBottom: 8, display: "flex", justifyContent: "space-between", alignItems: "flex-start" }}
                >
                  <div>
                    <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                      <p style={{ color: "#fff", fontSize: 13, fontWeight: 600 }}>{b.guestName}</p>
                      {b.checkedInAt && (
                        <span style={{ background: "#14532d", color: "#4ade80", fontSize: 9.5, fontWeight: 700, letterSpacing: "0.06em", textTransform: "uppercase", padding: "2px 7px", borderRadius: 99 }}>
                          Checked In
                        </span>
                      )}
                    </div>
                    <p style={{ color: "#6B7280", fontSize: 11, marginTop: 2 }}>
                      {b.checkIn} – {b.checkOut} · {b.bookedGuests} guests
                    </p>
                    <p style={{ color: GOLD, fontSize: 11, marginTop: 2, fontFamily: "monospace" }}>{b.confirmationCode}</p>
                    <span style={{ color: "#6B7280", fontSize: 9.5, fontWeight: 700, textTransform: "uppercase", letterSpacing: "0.05em" }}>
                      {BOOKING_SOURCES.find((s) => s.value === b.bookingSource)?.label || b.bookingSource}
                    </span>
                  </div>
                  <div style={{ display: "flex", gap: 10, alignItems: "center" }}>
                    {!b.checkedInAt ? (
                      <button
                        onClick={() => handleMarkCheckedIn(b.confirmationCode)}
                        title="Mark as checked in"
                        style={{ background: "#14532d", border: "none", color: "#4ade80", cursor: "pointer", padding: "3px 6px", borderRadius: 6, display: "flex", alignItems: "center", gap: 4, fontSize: 10, fontWeight: 700 }}
                      >
                        <UserRound size={11} />
                        <span>Check In</span>
                      </button>
                    ) : null}
                    <button
                      onClick={() => startEdit(b)}
                      title="Edit reservation"
                      style={{ background: "transparent", border: "none", color: "#6B7280", cursor: "pointer", padding: 0, display: "flex" }}
                    >
                      <Pencil size={14} />
                    </button>
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
              )}
            </div>
          ))}
        </div>
      )}

      <div style={{ height: 1, background: "#1e1e1e", margin: "16px 0" }} />

      <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 12 }}>
        New Reservation
      </p>

      <input style={inputStyle} placeholder="Guest Name" value={editingCode ? "" : form.guestName} onChange={setField("guestName")} disabled={!!editingCode} />
      <input style={inputStyle} placeholder="Confirmation Code" value={editingCode ? "" : form.confirmationCode} onChange={setField("confirmationCode")} disabled={!!editingCode} />
      <div style={{ display: "flex", gap: 8 }}>
        <input style={inputStyle} placeholder="Check-In (e.g. July 24)" value={editingCode ? "" : form.checkIn} onChange={setField("checkIn")} disabled={!!editingCode} />
        <input style={inputStyle} placeholder="Check-Out" value={editingCode ? "" : form.checkOut} onChange={setField("checkOut")} disabled={!!editingCode} />
      </div>
      <div style={{ display: "flex", gap: 8 }}>
        <input style={inputStyle} type="number" min={1} placeholder="Nights" value={editingCode ? "" : form.nights} onChange={setField("nights")} disabled={!!editingCode} />
        <input style={inputStyle} type="number" min={1} placeholder="Guests" value={editingCode ? "" : form.bookedGuests} onChange={setField("bookedGuests")} disabled={!!editingCode} />
      </div>
      <select style={inputStyle} value={editingCode ? "airbnb" : form.bookingSource} onChange={setField("bookingSource")} disabled={!!editingCode}>
        {BOOKING_SOURCES.map((s) => <option key={s.value} value={s.value}>{s.label}</option>)}
      </select>

      {!editingCode && error && <p style={{ color: "#d96b6b", fontSize: 12, marginBottom: 8 }}>{error}</p>}

      <button
        onClick={handleAdd}
        disabled={!!editingCode}
        style={{
          width: "100%",
          background: editingCode ? "#1a1a1a" : `linear-gradient(135deg, ${GOLD} 0%, ${GOLD_LIGHT} 50%, ${GOLD} 100%)`,
          color: editingCode ? "#4B5563" : BG,
          border: editingCode ? "1px solid #2a2a2a" : "none",
          borderRadius: 8,
          padding: "12px",
          fontSize: 12,
          fontWeight: 700,
          letterSpacing: "0.1em",
          textTransform: "uppercase",
          cursor: editingCode ? "default" : "pointer",
          marginTop: 4,
        }}
      >
        Add Reservation
      </button>
    </div>
  );
}

type DashTab = "overview" | "intelligence" | "reservations" | "requests" | "experiences" | "guest" | "property" | "builder" | "settings";

const DASH_TABS: { key: DashTab; label: string; Icon: LucideIcon }[] = [
  { key: "overview",     label: "Overview",     Icon: LayoutDashboard   },
  { key: "intelligence", label: "Intelligence", Icon: Sparkles          },
  { key: "reservations", label: "Reservations", Icon: CalendarCheck     },
  { key: "requests",     label: "Requests",     Icon: MessageSquareText },
  { key: "experiences",  label: "Experiences",  Icon: Gift              },
  { key: "guest",        label: "Guest",        Icon: UserRound         },
  { key: "property",     label: "Property",     Icon: KeyRound          },
  { key: "builder",      label: "Stay Builder", Icon: Wand2             },
  { key: "settings",     label: "Settings",     Icon: SettingsIcon      },
];

export default function HostPreview() {
  const [open, setOpen] = useState(false);
  const [tab, setTab] = useState<DashTab>("overview");
  const [data, setData] = useState<GuestInfo | null>(null);
  const [loggedIn, setLoggedIn] = useState(false);
  const [checked, setChecked] = useState(false);
  const [unreadCount, setUnreadCount] = useState(0);

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
    let hasSessionToken = false;
    try {
      hasSessionToken = !!localStorage.getItem("nova_session_token");
    } catch { /* ignore */ }
    setLoggedIn(isHostLoggedIn() || hasSessionToken);
    setChecked(true);
  }, [open]);

  // Poll for new guest requests so the host gets a near-real-time alert
  // without needing to refresh — Vercel serverless can't hold a socket
  // open, so polling is the lightest way to get this without new infra.
  useEffect(() => {
    if (!loggedIn) return;
    if (typeof window !== "undefined" && "Notification" in window && Notification.permission === "default") {
      Notification.requestPermission().catch(() => {});
    }

    let seenIds = new Set<number>();
    let firstRun = true;

    const poll = () => {
      getGuestRequests().then((requests) => {
        const unread = requests.filter((r) => !r.isRead);
        setUnreadCount(unread.length);

        if (!firstRun) {
          const fresh = unread.filter((r) => !seenIds.has(r.id));
          if (fresh.length > 0 && typeof window !== "undefined" && "Notification" in window && Notification.permission === "granted") {
            const r = fresh[0];
            new Notification(`New message from ${r.guestName}`, { body: r.message, tag: `request-${r.id}` });
          }
        }
        firstRun = false;
        seenIds = new Set(unread.map((r) => r.id));
      });
    };

    poll();
    const interval = setInterval(poll, 10000);
    return () => clearInterval(interval);
  }, [loggedIn]);

  const fmt = (d: string) => d || "—";

  const clearData = () => {
    if (!window.confirm("Clear this device's check-in data? The guest app will reload to the confirmation code screen.")) return;
    localStorage.removeItem("stayByNova_guestInfo");
    setData(null);
    window.location.reload();
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
          display: "flex",
          alignItems: "center",
          gap: 8,
        }}
      >
        Host View
        {loggedIn && unreadCount > 0 && (
          <span
            style={{
              background: "#EF4444",
              color: "#fff",
              fontSize: 10,
              fontWeight: 700,
              borderRadius: 999,
              minWidth: 16,
              height: 16,
              display: "flex",
              alignItems: "center",
              justifyContent: "center",
              padding: "0 4px",
            }}
          >
            {unreadCount}
          </span>
        )}
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
              <PropertySwitcher onSwitchToBuilder={() => setTab("builder")} />

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
                    <span style={{ position: "relative", display: "flex" }}>
                      <t.Icon size={14} strokeWidth={1.8} />
                      {t.key === "requests" && unreadCount > 0 && (
                        <span
                          style={{
                            position: "absolute",
                            top: -4,
                            right: -6,
                            width: 7,
                            height: 7,
                            borderRadius: "50%",
                            background: "#EF4444",
                          }}
                        />
                      )}
                    </span>
                    {t.label}
                  </button>
                ))}
              </div>

              {tab === "overview" && <Overview onGoToReservations={() => setTab("reservations")} />}
              {tab === "intelligence" && <IntelligenceCenter />}
              {tab === "requests" && <GuestRequests />}
              {tab === "experiences" && <ExperienceRequests />}
              {tab === "property" && <PropertyStatus />}
              {tab === "builder" && <StayBuilder />}
              {tab === "settings" && (
                <HostSettings
                  onLogout={() => {
                    const sessionToken = (() => {
                      try {
                        return localStorage.getItem("nova_session_token");
                      } catch {
                        return null;
                      }
                    })();
                    if (sessionToken) {
                      fetch("/api/auth/logout", {
                        method: "POST",
                        headers: { "x-session-token": sessionToken },
                      }).catch(() => { /* ignore */ });
                    }
                    try {
                      localStorage.removeItem("nova_session_token");
                      localStorage.removeItem("nova_active_property_slug");
                      localStorage.removeItem("nova_host_properties");
                    } catch { /* ignore */ }
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
