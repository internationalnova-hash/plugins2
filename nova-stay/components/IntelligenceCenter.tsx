"use client";

import { useEffect, useState } from "react";
import { Sparkles, TrendingUp, Clock, Heart, DollarSign, Wand2, BookOpen, Activity } from "lucide-react";
import { getIntelligence, type IntelligenceData } from "@/lib/propertyStore";
import theme from "@/config/theme";

const GOLD = theme.gold;
const GOLD_LIGHT = theme.goldLight;

const BENEFIT_LABEL: Record<string, string> = {
  time: "Saves time",
  experience: "Improves experience",
  revenue: "Earns more",
};
const BENEFIT_COLOR: Record<string, string> = {
  time: "#60A5FA",
  experience: "#34D399",
  revenue: GOLD,
};

function SectionCard({ icon: Icon, title, children }: { icon: any; title: string; children: React.ReactNode }) {
  return (
    <div className="lux-glass" style={{ padding: "14px 16px", marginBottom: 14 }}>
      <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 10 }}>
        <Icon size={14} strokeWidth={1.7} style={{ color: GOLD }} />
        <span style={{ color: "#6B7280", fontSize: 10.5, letterSpacing: "0.15em", textTransform: "uppercase" }}>{title}</span>
      </div>
      {children}
    </div>
  );
}

function BenefitPill({ benefit }: { benefit: string }) {
  return (
    <span
      style={{
        flexShrink: 0,
        fontSize: 9.5,
        fontWeight: 700,
        letterSpacing: "0.04em",
        textTransform: "uppercase",
        color: BENEFIT_COLOR[benefit] || "#9CA3AF",
        border: `1px solid ${BENEFIT_COLOR[benefit] || "#9CA3AF"}44`,
        borderRadius: 999,
        padding: "2px 8px",
      }}
    >
      {BENEFIT_LABEL[benefit] || benefit}
    </span>
  );
}

export default function IntelligenceCenter() {
  const [data, setData] = useState<IntelligenceData | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    getIntelligence().then((result) => {
      setData(result);
      setLoading(false);
    });
  }, []);

  if (loading) {
    return <p style={{ color: "#4B5563", fontSize: 13, textAlign: "center", padding: "20px 0" }}>Analyzing your business…</p>;
  }

  if (!data) {
    return <p style={{ color: "#EF4444", fontSize: 13, textAlign: "center", padding: "20px 0" }}>Failed to load intelligence data.</p>;
  }

  const healthColor = data.healthScore >= 75 ? "#34D399" : data.healthScore >= 50 ? GOLD : "#EF4444";

  return (
    <div>
      <p style={{ color: "#fff", fontSize: 16, fontWeight: 600, marginBottom: 4 }}>Nova Intelligence</p>
      <p style={{ color: "#6B7280", fontSize: 12.5, marginBottom: 16 }}>
        Your AI hospitality advisor — not just answers, but ways to run this property better.
      </p>

      {/* Property Health Score */}
      <div className="lux-glass" style={{ padding: "18px 16px", marginBottom: 14, textAlign: "center" }}>
        <p style={{ color: "#6B7280", fontSize: 10.5, letterSpacing: "0.15em", textTransform: "uppercase", marginBottom: 8 }}>
          Property Health Score
        </p>
        <p style={{ color: healthColor, fontSize: 44, fontWeight: 700, fontFamily: "Georgia, serif", lineHeight: 1, marginBottom: 10 }}>
          {data.healthScore}
        </p>
        <div style={{ display: "flex", justifyContent: "center", gap: 16, flexWrap: "wrap" }}>
          <span style={{ color: "#9CA3AF", fontSize: 11 }}>Response {data.healthBreakdown.responseScore}</span>
          <span style={{ color: "#9CA3AF", fontSize: 11 }}>Engagement {data.healthBreakdown.engagementScore}</span>
          <span style={{ color: "#9CA3AF", fontSize: 11 }}>Conversion {data.healthBreakdown.conversionScore}</span>
        </div>
      </div>

      {/* Weekly AI Insights */}
      <SectionCard icon={Sparkles} title="Weekly AI Insights">
        <div style={{ display: "flex", flexDirection: "column", gap: 8 }}>
          {data.weeklyAiInsights.map((insight, i) => (
            <p key={i} style={{ color: "#D1D5DB", fontSize: 12.5, lineHeight: 1.6 }}>
              <span style={{ color: GOLD }}>⭐ </span>{insight}
            </p>
          ))}
        </div>
      </SectionCard>

      {/* Revenue Opportunities */}
      <SectionCard icon={DollarSign} title={`Revenue Opportunities${data.totalEstimatedRevenue > 0 ? ` — est. $${data.totalEstimatedRevenue}` : ""}`}>
        {data.revenueOpportunities.length === 0 ? (
          <p style={{ color: "#6B7280", fontSize: 12.5 }}>No open upsell opportunities right now.</p>
        ) : (
          <div style={{ display: "flex", flexDirection: "column", gap: 8 }}>
            {data.revenueOpportunities.map((r) => (
              <div key={r.amenity} style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline" }}>
                <p style={{ color: "#D1D5DB", fontSize: 12.5 }}>
                  {r.experienceTitle} — {r.unconvertedGuests} guest{r.unconvertedGuests === 1 ? "" : "s"} interested
                </p>
                <span style={{ color: GOLD, fontSize: 12.5, fontWeight: 700 }}>${r.estimatedValue}</span>
              </div>
            ))}
          </div>
        )}
      </SectionCard>

      {/* Suggested Automations */}
      <SectionCard icon={Wand2} title="Suggested Automations">
        {data.suggestedAutomations.length === 0 ? (
          <p style={{ color: "#6B7280", fontSize: 12.5 }}>Nothing to automate right now.</p>
        ) : (
          <div style={{ display: "flex", flexDirection: "column", gap: 10 }}>
            {data.suggestedAutomations.map((a, i) => (
              <div key={i} style={{ display: "flex", alignItems: "flex-start", gap: 8 }}>
                <p style={{ color: "#D1D5DB", fontSize: 12.5, lineHeight: 1.55, flex: 1 }}>{a.text}</p>
                <BenefitPill benefit={a.benefit} />
              </div>
            ))}
          </div>
        )}
      </SectionCard>

      {/* Suggested Guide Improvements */}
      <SectionCard icon={BookOpen} title="Suggested Guide Improvements">
        <div style={{ display: "flex", flexDirection: "column", gap: 10 }}>
          {data.suggestedGuideImprovements.map((g, i) => (
            <div key={i} style={{ display: "flex", alignItems: "flex-start", gap: 8 }}>
              <p style={{ color: "#D1D5DB", fontSize: 12.5, lineHeight: 1.55, flex: 1 }}>{g.text}</p>
              <BenefitPill benefit={g.benefit} />
            </div>
          ))}
        </div>
      </SectionCard>

      {/* Frequently Asked Questions */}
      <SectionCard icon={Activity} title="Frequently Asked Questions">
        {data.frequentlyAskedQuestions.length === 0 ? (
          <p style={{ color: "#6B7280", fontSize: 12.5 }}>No guest messages yet.</p>
        ) : (
          <div style={{ display: "flex", flexDirection: "column", gap: 6 }}>
            {data.frequentlyAskedQuestions.map((f) => (
              <div key={f.topic} style={{ display: "flex", justifyContent: "space-between" }}>
                <span style={{ color: "#D1D5DB", fontSize: 12.5 }}>{f.topic}</span>
                <span style={{ color: "#6B7280", fontSize: 12.5 }}>{f.count}x</span>
              </div>
            ))}
          </div>
        )}
      </SectionCard>

      {/* Guest Behavior Trends */}
      <SectionCard icon={TrendingUp} title="Guest Behavior Trends">
        {data.guestBehaviorTrends.length === 0 ? (
          <p style={{ color: "#6B7280", fontSize: 12.5 }}>No amenity views recorded yet.</p>
        ) : (
          <div style={{ display: "flex", flexDirection: "column", gap: 6 }}>
            {data.guestBehaviorTrends.map((t) => (
              <div key={t.amenity} style={{ display: "flex", justifyContent: "space-between" }}>
                <span style={{ color: "#D1D5DB", fontSize: 12.5, textTransform: "capitalize" }}>{t.amenity}</span>
                <span style={{ color: t.changePct > 0 ? "#34D399" : t.changePct < 0 ? "#EF4444" : "#6B7280", fontSize: 12.5 }}>
                  {t.recentViews} views {t.changePct !== 0 && (t.changePct > 0 ? `▲${t.changePct}%` : `▼${Math.abs(t.changePct)}%`)}
                </span>
              </div>
            ))}
          </div>
        )}
      </SectionCard>

      {/* Experience Conversion Rates */}
      <SectionCard icon={Heart} title="Experience Conversion Rates">
        {data.experienceConversionRates.length === 0 ? (
          <p style={{ color: "#6B7280", fontSize: 12.5 }}>Not enough data yet.</p>
        ) : (
          <div style={{ display: "flex", flexDirection: "column", gap: 6 }}>
            {data.experienceConversionRates.map((r) => (
              <div key={r.amenity} style={{ display: "flex", justifyContent: "space-between" }}>
                <span style={{ color: "#D1D5DB", fontSize: 12.5 }}>{r.experienceTitle}</span>
                <span style={{ color: "#6B7280", fontSize: 12.5 }}>{r.booked}/{r.interestedGuests} ({r.conversionPct}%)</span>
              </div>
            ))}
          </div>
        )}
      </SectionCard>

      {/* Guide Engagement */}
      <SectionCard icon={Clock} title="Guide Engagement">
        <p style={{ color: "#D1D5DB", fontSize: 12.5, marginBottom: 8 }}>
          {data.guideEngagement.guestsWhoOpenedGuide}/{data.guideEngagement.totalGuests} guests opened the guide ({data.guideEngagement.engagementPct}%)
        </p>
        {data.guideEngagement.topAmenities.length > 0 && (
          <div style={{ display: "flex", flexDirection: "column", gap: 4 }}>
            {data.guideEngagement.topAmenities.map((a) => (
              <div key={a.amenity} style={{ display: "flex", justifyContent: "space-between" }}>
                <span style={{ color: "#9CA3AF", fontSize: 11.5, textTransform: "capitalize" }}>{a.amenity}</span>
                <span style={{ color: "#6B7280", fontSize: 11.5 }}>{a.count} views</span>
              </div>
            ))}
          </div>
        )}
      </SectionCard>
    </div>
  );
}
