"use client";

import { useEffect, useRef, useState } from "react";
import { Sparkles } from "lucide-react";
import { getGuestChatHistory, sendGuestChatMessage, type ChatMessage } from "@/lib/propertyStore";
import theme from "@/config/theme";

const GOLD = theme.gold;
const GOLD_LIGHT = theme.goldLight;

export default function NovaChat() {
  const [confirmationCode, setConfirmationCode] = useState<string | null>(null);
  const [messages, setMessages] = useState<ChatMessage[]>([]);
  const [input, setInput] = useState("");
  const [sending, setSending] = useState(false);
  const [loaded, setLoaded] = useState(false);
  const scrollContainerRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    let code: string | null = null;
    try {
      const raw = localStorage.getItem("stayByNova_guestInfo");
      const parsed = raw ? JSON.parse(raw) : null;
      if (parsed?.confirmationCode) code = parsed.confirmationCode;
    } catch { /* ignore */ }
    setConfirmationCode(code);

    if (code) {
      getGuestChatHistory(code).then((history) => {
        setMessages(history);
        setLoaded(true);
      });
    } else {
      setLoaded(true);
    }
  }, []);

  useEffect(() => {
    const el = scrollContainerRef.current;
    if (el) el.scrollTo({ top: el.scrollHeight, behavior: "smooth" });
  }, [messages, sending]);

  const send = async () => {
    const text = input.trim();
    if (!text || sending || !confirmationCode) return;
    setInput("");
    setSending(true);
    setMessages((prev) => [...prev, { id: Date.now(), role: "user", text, createdAt: new Date().toISOString() }]);

    const result = await sendGuestChatMessage(confirmationCode, text);
    setSending(false);
    if (result.ok && result.reply) {
      setMessages((prev) => [...prev, { id: Date.now() + 1, role: "assistant", text: result.reply!, createdAt: new Date().toISOString() }]);
    } else {
      setMessages((prev) => [
        ...prev,
        { id: Date.now() + 1, role: "assistant", text: result.error || "Something went wrong — please try again.", createdAt: new Date().toISOString() },
      ]);
    }
  };

  return (
    <div
      style={{
        background: "rgba(201,168,76,0.04)",
        border: `1px solid ${GOLD}22`,
        borderRadius: 16,
        padding: "20px 18px",
        marginBottom: 6,
      }}
    >
      <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 12 }}>
        <Sparkles size={16} strokeWidth={1.7} style={{ color: GOLD }} />
        <p style={{ color: GOLD, fontSize: 11, letterSpacing: "0.2em", textTransform: "uppercase" }}>Ask Nova</p>
      </div>

      {!loaded ? null : !confirmationCode ? (
        <p style={{ color: "#9CA3AF", fontSize: 13, lineHeight: 1.6 }}>
          Complete your check-in to start chatting with Nova, your AI concierge.
        </p>
      ) : (
        <>
          {messages.length === 0 ? (
            <p style={{ color: "#9CA3AF", fontSize: 13, lineHeight: 1.6, marginBottom: 14 }}>
              Need restaurant recommendations, theater instructions, or anything else? Ask away — Nova remembers your whole stay.
            </p>
          ) : (
            <div ref={scrollContainerRef} style={{ display: "flex", flexDirection: "column", gap: 10, marginBottom: 14, maxHeight: 320, overflowY: "auto" }}>
              {messages.map((m) => (
                <div
                  key={m.id}
                  style={{
                    alignSelf: m.role === "user" ? "flex-end" : "flex-start",
                    maxWidth: "85%",
                    background: m.role === "user" ? "rgba(201,168,76,0.12)" : "#111",
                    border: `1px solid ${m.role === "user" ? `${GOLD}33` : "#1e1e1e"}`,
                    borderRadius: 12,
                    padding: "8px 12px",
                  }}
                >
                  <p style={{ color: m.role === "user" ? "#fff" : "#D1D5DB", fontSize: 13, lineHeight: 1.55, whiteSpace: "pre-wrap" }}>{m.text}</p>
                </div>
              ))}
              {sending && (
                <div style={{ alignSelf: "flex-start", padding: "8px 12px" }}>
                  <p style={{ color: "#6B7280", fontSize: 12 }}>Nova is thinking…</p>
                </div>
              )}
            </div>
          )}

          <div style={{ display: "flex", gap: 8 }}>
            <input
              value={input}
              onChange={(e) => setInput(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === "Enter") send();
              }}
              placeholder="Ask Nova anything about your stay…"
              style={{
                flex: 1,
                background: "#0d0d0d",
                border: "1px solid #2a2a2a",
                borderRadius: 8,
                padding: "10px 12px",
                color: "#fff",
                fontSize: 13,
                outline: "none",
              }}
            />
            <button
              onClick={send}
              disabled={!input.trim() || sending}
              className="btn-press"
              style={{
                background: `linear-gradient(135deg, ${GOLD}, ${GOLD_LIGHT})`,
                color: "#0A0A0A",
                border: "none",
                borderRadius: 8,
                padding: "0 18px",
                fontSize: 12,
                fontWeight: 700,
                cursor: input.trim() && !sending ? "pointer" : "default",
                opacity: input.trim() && !sending ? 1 : 0.5,
              }}
            >
              Ask
            </button>
          </div>
        </>
      )}
    </div>
  );
}
