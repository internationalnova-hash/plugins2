"use client";

import { useEffect, useState } from "react";
import { Wand2, ChevronRight, ChevronLeft, Check } from "lucide-react";
import { getStayGuide, saveStayGuide, generateStayGuide, type StayGuide } from "@/lib/propertyStore";
import theme from "@/config/theme";

const GOLD = theme.gold;

const QUESTIONS: { key: string; label: string; placeholder: string }[] = [
  { key: "checkin", label: "How does a guest check in?", placeholder: "e.g. Door code is 1710#, available after 4 PM, front door on the left of the driveway." },
  { key: "parking", label: "Where do guests park?", placeholder: "e.g. Driveway fits 3 cars, please don't block the garage." },
  { key: "checkout", label: "What's your checkout process?", placeholder: "e.g. Checkout by 11 AM. Start the dishwasher, take out trash, lock the front door behind you." },
  { key: "houseRules", label: "Any house rules guests should know?", placeholder: "e.g. No smoking indoors, quiet hours after 10 PM, no parties." },
  { key: "amenities", label: "What amenities should we highlight?", placeholder: "e.g. Heated pool, home theater, recording studio, game room." },
  { key: "dining", label: "Favorite nearby restaurants or food spots?", placeholder: "e.g. Bones Steakhouse for a special night, Busy Bee Cafe for soul food." },
  { key: "emergency", label: "Who should guests contact in an emergency?", placeholder: "e.g. Text the host directly at this number, or call 911 for true emergencies." },
  { key: "faq", label: "Anything guests frequently ask about?", placeholder: "e.g. Extra towels are in the hallway closet, thermostat is on the wall by the kitchen." },
];

const inputStyle: React.CSSProperties = {
  width: "100%",
  background: "#0d0d0d",
  border: "1px solid #1e1e1e",
  borderRadius: 10,
  padding: "12px 14px",
  color: "#fff",
  fontSize: 13,
  lineHeight: 1.6,
  minHeight: 90,
  resize: "vertical",
};

const GUIDE_LABELS: Record<keyof StayGuide, string> = {
  welcomeMessage: "Welcome Message",
  checkinInstructions: "Check-In",
  parkingInstructions: "Parking",
  checkoutInstructions: "Checkout",
  houseRules: "House Rules",
  amenitiesHighlights: "Amenities",
  diningRecommendations: "Dining",
  faq: "FAQ",
  emergencyContacts: "Emergency Contacts",
};

export default function StayBuilder() {
  const [step, setStep] = useState(0);
  const [answers, setAnswers] = useState<Record<string, string>>({});
  const [generating, setGenerating] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [guide, setGuide] = useState<StayGuide | null>(null);
  const [savedAt, setSavedAt] = useState<string | null>(null);
  const [saving, setSaving] = useState(false);

  useEffect(() => {
    getStayGuide().then((existing) => {
      if (existing) {
        setGuide(existing.content);
        setSavedAt(existing.updatedAt);
      }
    });
  }, []);

  const updateAnswer = (key: string, value: string) => setAnswers((prev) => ({ ...prev, [key]: value }));

  const isLastQuestion = step === QUESTIONS.length - 1;

  const runGenerate = async () => {
    setGenerating(true);
    setError(null);
    const result = await generateStayGuide(answers);
    setGenerating(false);
    if (!result.ok || !result.guide) {
      setError(result.error || "Failed to generate guide.");
      return;
    }
    setGuide(result.guide);
  };

  const updateGuideField = (key: keyof StayGuide, value: string) => {
    if (!guide) return;
    setGuide({ ...guide, [key]: value });
  };

  const handleSave = async () => {
    if (!guide) return;
    setSaving(true);
    const result = await saveStayGuide(guide);
    setSaving(false);
    if (result.ok) {
      setSavedAt(new Date().toISOString());
    } else {
      setError(result.error || "Failed to save guide.");
    }
  };

  if (guide) {
    return (
      <div>
        <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 4 }}>
          <Wand2 size={16} strokeWidth={1.7} style={{ color: GOLD }} />
          <span style={{ color: "#fff", fontSize: 13, fontWeight: 600 }}>Stay Guide</span>
        </div>
        {savedAt && (
          <p style={{ color: "#4B5563", fontSize: 10.5, marginBottom: 14 }}>
            Saved {new Date(savedAt).toLocaleString()}
          </p>
        )}
        {error && <p style={{ color: "#EF4444", fontSize: 12, marginBottom: 10 }}>{error}</p>}

        {(Object.keys(GUIDE_LABELS) as (keyof StayGuide)[]).map((key) => (
          <div key={key} style={{ marginBottom: 14 }}>
            <p style={{ color: "#6B7280", fontSize: 10.5, letterSpacing: "0.1em", textTransform: "uppercase", marginBottom: 6 }}>
              {GUIDE_LABELS[key]}
            </p>
            <textarea
              style={inputStyle}
              value={guide[key]}
              onChange={(e) => updateGuideField(key, e.target.value)}
            />
          </div>
        ))}

        <div style={{ display: "flex", gap: 10 }}>
          <button
            onClick={handleSave}
            disabled={saving}
            className="btn-press lux-btn-gold"
            style={{ flex: 1, padding: "12px 0", fontSize: 12, opacity: saving ? 0.6 : 1 }}
          >
            {saving ? "Saving…" : "Save Stay Guide"}
          </button>
          <button
            onClick={() => {
              setGuide(null);
              setStep(0);
              setAnswers({});
            }}
            className="btn-press"
            style={{ background: "transparent", border: "1px solid #1e1e1e", borderRadius: "var(--radius-sm)", padding: "12px 16px", color: "#6B7280", fontSize: 12, cursor: "pointer" }}
          >
            Rebuild
          </button>
        </div>
      </div>
    );
  }

  const q = QUESTIONS[step];

  return (
    <div>
      <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 4 }}>
        <Wand2 size={16} strokeWidth={1.7} style={{ color: GOLD }} />
        <span style={{ color: "#fff", fontSize: 13, fontWeight: 600 }}>AI Stay Builder</span>
      </div>
      <p style={{ color: "#6B7280", fontSize: 11.5, lineHeight: 1.6, marginBottom: 16 }}>
        Answer a few quick questions and Nova will draft your full guest guide.
      </p>

      <div style={{ display: "flex", gap: 4, marginBottom: 16 }}>
        {QUESTIONS.map((_, i) => (
          <div key={i} style={{ flex: 1, height: 3, borderRadius: 2, background: i <= step ? GOLD : "#1e1e1e" }} />
        ))}
      </div>

      <p style={{ color: "#fff", fontSize: 15, fontWeight: 600, marginBottom: 10 }}>{q.label}</p>
      <textarea
        style={inputStyle}
        placeholder={q.placeholder}
        value={answers[q.key] || ""}
        onChange={(e) => updateAnswer(q.key, e.target.value)}
      />

      {error && <p style={{ color: "#EF4444", fontSize: 12, marginTop: 10 }}>{error}</p>}

      <div style={{ display: "flex", gap: 10, marginTop: 16 }}>
        {step > 0 && (
          <button
            onClick={() => setStep((s) => s - 1)}
            className="btn-press"
            style={{ background: "transparent", border: "1px solid #1e1e1e", borderRadius: "var(--radius-sm)", padding: "12px 16px", color: "#6B7280", fontSize: 12, cursor: "pointer", display: "flex", alignItems: "center", gap: 4 }}
          >
            <ChevronLeft size={14} /> Back
          </button>
        )}
        {!isLastQuestion ? (
          <button
            onClick={() => setStep((s) => s + 1)}
            className="btn-press lux-btn-gold"
            style={{ flex: 1, padding: "12px 0", fontSize: 12, display: "flex", alignItems: "center", justifyContent: "center", gap: 4 }}
          >
            Next <ChevronRight size={14} />
          </button>
        ) : (
          <button
            onClick={runGenerate}
            disabled={generating}
            className="btn-press lux-btn-gold"
            style={{ flex: 1, padding: "12px 0", fontSize: 12, opacity: generating ? 0.6 : 1, display: "flex", alignItems: "center", justifyContent: "center", gap: 4 }}
          >
            {generating ? "Generating…" : <>Generate Guide <Check size={14} /></>}
          </button>
        )}
      </div>
    </div>
  );
}
