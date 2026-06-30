"use client";

import { useState } from "react";
import theme from "@/config/theme";

const GOLD = theme.gold;
const GOLD_LIGHT = theme.goldLight;
const BG = theme.bg;

interface AdminProperty {
  id: number;
  slug: string;
  name: string;
}

interface AdminHost {
  id: number;
  email: string;
  name: string;
  createdAt: string;
  properties: AdminProperty[];
}

const inputStyle: React.CSSProperties = {
  width: "100%",
  background: "#0d0d0d",
  border: "1px solid #2a2a2a",
  borderRadius: 8,
  padding: "10px 12px",
  color: "#fff",
  fontSize: 13,
  marginBottom: 10,
  outline: "none",
};

const cardStyle: React.CSSProperties = {
  background: "#141414",
  border: "1px solid #2a2a2a",
  borderRadius: 12,
  padding: 18,
};

export default function AdminPage() {
  const [token, setToken] = useState<string | null>(null);
  const [passwordInput, setPasswordInput] = useState("");
  const [authError, setAuthError] = useState<string | null>(null);
  const [authLoading, setAuthLoading] = useState(false);

  const [hosts, setHosts] = useState<AdminHost[]>([]);
  const [listError, setListError] = useState<string | null>(null);

  const [createForm, setCreateForm] = useState({ email: "", password: "", name: "", propertySlug: "", propertyName: "" });
  const [createError, setCreateError] = useState<string | null>(null);
  const [createLoading, setCreateLoading] = useState(false);

  const loadHosts = async (authToken: string) => {
    setListError(null);
    try {
      const res = await fetch("/api/hosts", { headers: { "x-host-token": authToken } });
      const data = await res.json().catch(() => ({}));
      if (!res.ok) {
        setListError(data.error || "Failed to load hosts.");
        return false;
      }
      setHosts(data.hosts || []);
      return true;
    } catch {
      setListError("Failed to load hosts.");
      return false;
    }
  };

  const submitPassword = async () => {
    if (!passwordInput.trim() || authLoading) return;
    setAuthLoading(true);
    setAuthError(null);
    const candidate = passwordInput.trim();
    const ok = await loadHosts(candidate);
    setAuthLoading(false);
    if (ok) {
      setToken(candidate);
    } else {
      setAuthError("Incorrect admin password.");
    }
  };

  const createHost = async () => {
    if (!token || createLoading) return;
    const { email, password, name, propertySlug, propertyName } = createForm;
    if (!email.trim() || !password.trim() || !name.trim()) {
      setCreateError("Email, password, and name are required.");
      return;
    }
    setCreateLoading(true);
    setCreateError(null);
    try {
      const res = await fetch("/api/hosts", {
        method: "POST",
        headers: { "Content-Type": "application/json", "x-host-token": token },
        body: JSON.stringify({
          email: email.trim(),
          password: password.trim(),
          name: name.trim(),
          propertySlug: propertySlug.trim() || undefined,
          propertyName: propertyName.trim() || undefined,
        }),
      });
      const data = await res.json().catch(() => ({}));
      setCreateLoading(false);
      if (!res.ok || !data.ok) {
        setCreateError(data.error || "Failed to create host.");
        return;
      }
      setCreateForm({ email: "", password: "", name: "", propertySlug: "", propertyName: "" });
      await loadHosts(token);
    } catch {
      setCreateLoading(false);
      setCreateError("Failed to create host.");
    }
  };

  if (!token) {
    return (
      <div
        style={{
          minHeight: "100vh",
          background: BG,
          display: "flex",
          alignItems: "center",
          justifyContent: "center",
          padding: 24,
        }}
      >
        <div style={{ ...cardStyle, width: "100%", maxWidth: 360 }}>
          <p style={{ color: GOLD, fontSize: 11, fontWeight: 700, letterSpacing: "0.2em", textTransform: "uppercase", marginBottom: 16 }}>
            Super Admin
          </p>
          <input
            style={inputStyle}
            type="password"
            placeholder="Admin Password"
            value={passwordInput}
            onChange={(e) => setPasswordInput(e.target.value)}
            onKeyDown={(e) => e.key === "Enter" && submitPassword()}
          />
          {authError && <p style={{ color: "#d96b6b", fontSize: 12, marginBottom: 8 }}>{authError}</p>}
          <button
            onClick={submitPassword}
            disabled={authLoading}
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
              cursor: authLoading ? "default" : "pointer",
              opacity: authLoading ? 0.6 : 1,
            }}
          >
            {authLoading ? "Checking…" : "Enter"}
          </button>
        </div>
      </div>
    );
  }

  return (
    <div style={{ minHeight: "100vh", background: BG, padding: "32px 24px", color: "#E5E7EB" }}>
      <div style={{ maxWidth: 720, margin: "0 auto" }}>
        <p style={{ color: GOLD, fontSize: 11, fontWeight: 700, letterSpacing: "0.2em", textTransform: "uppercase", marginBottom: 24 }}>
          Super Admin — Hosts
        </p>

        <div style={{ ...cardStyle, marginBottom: 24 }}>
          <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 14 }}>
            Create Host
          </p>
          <input style={inputStyle} placeholder="Email" value={createForm.email} onChange={(e) => setCreateForm({ ...createForm, email: e.target.value })} />
          <input style={inputStyle} type="password" placeholder="Password" value={createForm.password} onChange={(e) => setCreateForm({ ...createForm, password: e.target.value })} />
          <input style={inputStyle} placeholder="Host Name" value={createForm.name} onChange={(e) => setCreateForm({ ...createForm, name: e.target.value })} />
          <div style={{ display: "flex", gap: 8 }}>
            <input style={inputStyle} placeholder="First property slug (optional)" value={createForm.propertySlug} onChange={(e) => setCreateForm({ ...createForm, propertySlug: e.target.value })} />
            <input style={inputStyle} placeholder="First property name (optional)" value={createForm.propertyName} onChange={(e) => setCreateForm({ ...createForm, propertyName: e.target.value })} />
          </div>
          {createError && <p style={{ color: "#d96b6b", fontSize: 12, marginBottom: 8 }}>{createError}</p>}
          <button
            onClick={createHost}
            disabled={createLoading}
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
              cursor: createLoading ? "default" : "pointer",
              opacity: createLoading ? 0.6 : 1,
            }}
          >
            {createLoading ? "Creating…" : "Create Host"}
          </button>
        </div>

        <p style={{ color: "#6B7280", fontSize: 11, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 14 }}>
          All Hosts
        </p>
        {listError && <p style={{ color: "#d96b6b", fontSize: 12, marginBottom: 12 }}>{listError}</p>}
        {hosts.length === 0 ? (
          <p style={{ color: "#4B5563", fontSize: 13 }}>No hosts yet.</p>
        ) : (
          hosts.map((h) => (
            <div key={h.id} style={{ ...cardStyle, marginBottom: 10 }}>
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline", marginBottom: 6 }}>
                <p style={{ color: "#fff", fontSize: 14, fontWeight: 600 }}>{h.name}</p>
                <span style={{ color: "#6B7280", fontSize: 10.5 }}>{new Date(h.createdAt).toLocaleDateString()}</span>
              </div>
              <p style={{ color: "#9CA3AF", fontSize: 12.5, marginBottom: 8 }}>{h.email}</p>
              {h.properties.length === 0 ? (
                <p style={{ color: "#4B5563", fontSize: 11.5 }}>No properties</p>
              ) : (
                <div style={{ display: "flex", flexDirection: "column", gap: 4 }}>
                  {h.properties.map((p) => (
                    <span key={p.id} style={{ color: "#D1D5DB", fontSize: 11.5 }}>
                      <span style={{ color: GOLD }}>{p.slug}</span> — {p.name}
                    </span>
                  ))}
                </div>
              )}
            </div>
          ))
        )}
      </div>
    </div>
  );
}
