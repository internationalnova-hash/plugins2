import { NextRequest, NextResponse } from "next/server";
import { sql, ensureSchema } from "@/lib/db";
import { getAuthorizedProperty, isSuperAdmin } from "@/lib/hostAuth";

// Keyword → topic map used to cluster free-text guest messages/chat turns
// into FAQ themes without an LLM call (instant, free, rule-based per scope).
const FAQ_TOPICS: { topic: string; keywords: string[] }[] = [
  { topic: "Parking", keywords: ["park", "parking", "driveway", "car"] },
  { topic: "WiFi", keywords: ["wifi", "wi-fi", "internet", "password", "network"] },
  { topic: "Check-in / Check-out", keywords: ["check in", "check-in", "checkin", "check out", "check-out", "checkout", "key", "door code", "lockbox"] },
  { topic: "Pool", keywords: ["pool", "heat the pool", "pool heat", "swim"] },
  { topic: "Theater / Entertainment", keywords: ["theater", "movie", "projector", "game room", "games"] },
  { topic: "Dining / Restaurants", keywords: ["restaurant", "dinner", "food", "eat", "dining"] },
  { topic: "Grill / BBQ", keywords: ["grill", "bbq", "barbecue"] },
  { topic: "Recording Studio", keywords: ["studio", "record", "recording", "mic", "microphone"] },
  { topic: "House Rules / Quiet Hours", keywords: ["quiet", "noise", "rules", "loud"] },
  { topic: "Emergency / Issues", keywords: ["broken", "not working", "leak", "emergency", "stuck", "locked out", "ac", "air condition", "hot water"] },
];

function classifyTopics(text: string): string[] {
  const lower = text.toLowerCase();
  return FAQ_TOPICS.filter((t) => t.keywords.some((k) => lower.includes(k))).map((t) => t.topic);
}

const AMENITY_TO_EXPERIENCE: Record<string, { id: string; title: string; estValue: number }> = {
  theater: { id: "content-creation", title: "Content Creation", estValue: 150 },
  studio: { id: "recording-studio", title: "Recording Studio", estValue: 200 },
  pool: { id: "pool-heating", title: "Pool Heating", estValue: 75 },
  grill: { id: "private-chef", title: "Private Chef", estValue: 250 },
};

export async function GET(req: NextRequest) {
  const auth = await getAuthorizedProperty(req);
  if (!auth && !isSuperAdmin(req)) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  try {
    await ensureSchema();

    const [bookingsRes, requestsRes, experienceRes, viewsRes, conversationsRes, guideRes] = auth
      ? await Promise.all([
          sql`SELECT confirmation_code, guest_name, check_in, check_out, created_at FROM bookings WHERE property_id = ${auth.propertyId};`,
          sql`SELECT message, category, priority, is_read, created_at, confirmation_code FROM guest_requests WHERE property_id = ${auth.propertyId};`,
          sql`SELECT confirmation_code, experience_title, status, price_from, created_at FROM experience_requests WHERE property_id = ${auth.propertyId};`,
          sql`SELECT confirmation_code, guest_name, amenity, viewed_at FROM amenity_views WHERE property_id = ${auth.propertyId};`,
          sql`SELECT confirmation_code, role, content, created_at FROM guest_conversations WHERE role = 'user' AND property_id = ${auth.propertyId};`,
          sql`SELECT content FROM stay_guide WHERE property_id = ${auth.propertyId} LIMIT 1;`,
        ])
      : await Promise.all([
          sql`SELECT confirmation_code, guest_name, check_in, check_out, created_at FROM bookings;`,
          sql`SELECT message, category, priority, is_read, created_at, confirmation_code FROM guest_requests;`,
          sql`SELECT confirmation_code, experience_title, status, price_from, created_at FROM experience_requests;`,
          sql`SELECT confirmation_code, guest_name, amenity, viewed_at FROM amenity_views;`,
          sql`SELECT confirmation_code, role, content, created_at FROM guest_conversations WHERE role = 'user';`,
          sql`SELECT content FROM stay_guide WHERE id = 1;`,
        ]);

    const bookings = bookingsRes.rows;
    const requests = requestsRes.rows;
    const experiences = experienceRes.rows;
    const views = viewsRes.rows;
    const conversations = conversationsRes.rows;
    const guide = guideRes.rows[0]?.content || {};

    const now = Date.now();
    const fourteenDaysAgo = now - 14 * 24 * 60 * 60 * 1000;
    const twentyEightDaysAgo = now - 28 * 24 * 60 * 60 * 1000;

    // ---------- Guest behavior trends ----------
    const viewsRecent = views.filter((v) => new Date(v.viewed_at).getTime() > fourteenDaysAgo);
    const viewsPrior = views.filter((v) => {
      const t = new Date(v.viewed_at).getTime();
      return t > twentyEightDaysAgo && t <= fourteenDaysAgo;
    });
    const countByAmenity = (rows: typeof views) => {
      const map = new Map<string, number>();
      rows.forEach((r) => map.set(r.amenity, (map.get(r.amenity) || 0) + 1));
      return map;
    };
    const recentCounts = countByAmenity(viewsRecent);
    const priorCounts = countByAmenity(viewsPrior);
    const amenitiesSeen = new Set(Array.from(recentCounts.keys()).concat(Array.from(priorCounts.keys())));
    const guestBehaviorTrends = Array.from(amenitiesSeen)
      .map((amenity) => {
        const recent = recentCounts.get(amenity) || 0;
        const prior = priorCounts.get(amenity) || 0;
        const changePct = prior === 0 ? (recent > 0 ? 100 : 0) : Math.round(((recent - prior) / prior) * 100);
        return { amenity, recentViews: recent, priorViews: prior, changePct };
      })
      .sort((a, b) => b.recentViews - a.recentViews);

    // ---------- Frequently asked questions ----------
    const topicCounts = new Map<string, number>();
    const tally = (text: string) => classifyTopics(text).forEach((topic) => topicCounts.set(topic, (topicCounts.get(topic) || 0) + 1));
    requests.forEach((r) => tally(r.message));
    conversations.forEach((c) => tally(c.content));
    const frequentlyAskedQuestions = Array.from(topicCounts.entries())
      .map(([topic, count]) => ({ topic, count }))
      .sort((a, b) => b.count - a.count)
      .slice(0, 8);

    // ---------- Experience conversion rates ----------
    const interestedGuestsByAmenity = new Map<string, Set<string>>();
    views.forEach((v) => {
      if (!AMENITY_TO_EXPERIENCE[v.amenity]) return;
      const key = v.amenity;
      if (!interestedGuestsByAmenity.has(key)) interestedGuestsByAmenity.set(key, new Set());
      interestedGuestsByAmenity.get(key)!.add(v.confirmation_code || v.guest_name || "unknown");
    });
    const bookedByAmenity = new Map<string, number>();
    experiences.forEach((e) => {
      const match = Object.values(AMENITY_TO_EXPERIENCE).find((a) => a.title === e.experience_title);
      if (!match || e.status === "declined") return;
      const key = Object.keys(AMENITY_TO_EXPERIENCE).find((k) => AMENITY_TO_EXPERIENCE[k].title === e.experience_title)!;
      bookedByAmenity.set(key, (bookedByAmenity.get(key) || 0) + 1);
    });
    const experienceConversionRates = Object.entries(AMENITY_TO_EXPERIENCE).map(([amenity, info]) => {
      const interested = interestedGuestsByAmenity.get(amenity)?.size || 0;
      const booked = bookedByAmenity.get(amenity) || 0;
      const conversionPct = interested === 0 ? 0 : Math.round((booked / interested) * 100);
      return { amenity, experienceTitle: info.title, interestedGuests: interested, booked, conversionPct };
    }).filter((r) => r.interestedGuests > 0 || r.booked > 0);

    // ---------- Guide engagement ----------
    const guestsWithCode = new Set(bookings.map((b) => b.confirmation_code).filter(Boolean));
    const guestsWhoViewed = new Set(views.map((v) => v.confirmation_code).filter(Boolean));
    const guideEngagement = {
      totalGuests: guestsWithCode.size,
      guestsWhoOpenedGuide: guestsWhoViewed.size,
      engagementPct: guestsWithCode.size === 0 ? 0 : Math.round((guestsWhoViewed.size / guestsWithCode.size) * 100),
      topAmenities: Array.from(countByAmenity(views).entries()).map(([amenity, count]) => ({ amenity, count })).sort((a, b) => b.count - a.count).slice(0, 5),
    };

    // ---------- Revenue opportunities ----------
    const revenueOpportunities = Array.from(interestedGuestsByAmenity.entries())
      .map(([amenity, guests]) => {
        const info = AMENITY_TO_EXPERIENCE[amenity];
        const booked = bookedByAmenity.get(amenity) || 0;
        const unconverted = Math.max(0, guests.size - booked);
        return { amenity, experienceTitle: info.title, unconvertedGuests: unconverted, estimatedValue: unconverted * info.estValue };
      })
      .filter((r) => r.unconvertedGuests > 0)
      .sort((a, b) => b.estimatedValue - a.estimatedValue);
    const totalEstimatedRevenue = revenueOpportunities.reduce((sum, r) => sum + r.estimatedValue, 0);

    // ---------- Property Health Score ----------
    const unreadCount = requests.filter((r) => !r.is_read).length;
    const totalRequests = requests.length;
    const readPct = totalRequests === 0 ? 100 : Math.round(((totalRequests - unreadCount) / totalRequests) * 100);
    const urgentUnread = requests.filter((r) => !r.is_read && (r.priority === "urgent" || r.priority === "high")).length;

    const responseScore = Math.max(0, readPct - urgentUnread * 10);
    const engagementScore = guideEngagement.engagementPct;
    const conversionScore = experienceConversionRates.length === 0
      ? 50
      : Math.round(experienceConversionRates.reduce((sum, r) => sum + r.conversionPct, 0) / experienceConversionRates.length);
    const healthScore = Math.round(responseScore * 0.4 + engagementScore * 0.3 + conversionScore * 0.3);

    // ---------- Weekly AI insights (rule-based digest) ----------
    const weeklyAiInsights: string[] = [];
    const topTrend = guestBehaviorTrends[0];
    if (topTrend && topTrend.changePct > 0 && topTrend.recentViews >= 2) {
      weeklyAiInsights.push(`Interest in ${topTrend.amenity} is up ${topTrend.changePct}% over the past two weeks (${topTrend.recentViews} views).`);
    }
    const topFaq = frequentlyAskedQuestions[0];
    if (topFaq && topFaq.count >= 2) {
      weeklyAiInsights.push(`"${topFaq.topic}" came up ${topFaq.count} times in guest messages and chats — consider making this more prominent in the guide.`);
    }
    if (urgentUnread > 0) {
      weeklyAiInsights.push(`${urgentUnread} high-priority request${urgentUnread === 1 ? "" : "s"} still unread — guests notice slow responses.`);
    }
    if (totalEstimatedRevenue > 0) {
      weeklyAiInsights.push(`There's an estimated $${totalEstimatedRevenue} in unconverted experience interest sitting in your pipeline right now.`);
    }
    if (guideEngagement.engagementPct < 50 && guideEngagement.totalGuests > 0) {
      weeklyAiInsights.push(`Only ${guideEngagement.engagementPct}% of guests have opened the digital guide — most questions could be answered before they're even asked.`);
    }
    if (weeklyAiInsights.length === 0) {
      weeklyAiInsights.push("Everything looks steady this week — no urgent trends to flag.");
    }

    // ---------- Suggested automations ----------
    const suggestedAutomations: { text: string; benefit: "time" | "experience" | "revenue" }[] = [];
    revenueOpportunities.slice(0, 2).forEach((r) => {
      suggestedAutomations.push({
        text: `Auto-send a ${r.experienceTitle} offer the moment a guest views the ${r.amenity} amenity 3+ times — ${r.unconvertedGuests} guest${r.unconvertedGuests === 1 ? "" : "s"} are showing interest right now.`,
        benefit: "revenue",
      });
    });
    if (topFaq && topFaq.count >= 3) {
      suggestedAutomations.push({
        text: `Set up an automated first-message reply covering "${topFaq.topic}" — it's your most asked-about topic and a canned answer would save you repeat replies.`,
        benefit: "time",
      });
    }
    if (guideEngagement.engagementPct < 50 && guideEngagement.totalGuests > 0) {
      suggestedAutomations.push({
        text: `Send an automatic "Have you seen the guide?" nudge 2 hours after check-in — engagement is currently only ${guideEngagement.engagementPct}%.`,
        benefit: "experience",
      });
    }

    // ---------- Suggested guide improvements ----------
    const guideFieldByTopic: Record<string, keyof typeof guide> = {
      "Parking": "parkingInstructions",
      "Check-in / Check-out": "checkinInstructions",
      "Dining / Restaurants": "diningRecommendations",
      "House Rules / Quiet Hours": "houseRules",
      "Emergency / Issues": "emergencyContacts",
    };
    const suggestedGuideImprovements: { text: string; benefit: "time" | "experience" | "revenue" }[] = [];
    frequentlyAskedQuestions.forEach((f) => {
      const field = guideFieldByTopic[f.topic];
      const content = field ? (guide as any)[field] : null;
      const isThin = !content || String(content).trim().length < 40;
      if (f.count >= 2 && isThin) {
        suggestedGuideImprovements.push({
          text: `Guests keep asking about "${f.topic}" (${f.count}x) but your guide's coverage is thin — expand that section to cut down on repeat questions.`,
          benefit: "time",
        });
      }
    });
    if (suggestedGuideImprovements.length === 0) {
      suggestedGuideImprovements.push({ text: "Your guide currently covers the topics guests ask about most — no gaps detected.", benefit: "experience" });
    }

    return NextResponse.json({
      healthScore,
      healthBreakdown: { responseScore, engagementScore, conversionScore },
      guestBehaviorTrends,
      frequentlyAskedQuestions,
      experienceConversionRates,
      guideEngagement,
      revenueOpportunities,
      totalEstimatedRevenue,
      weeklyAiInsights,
      suggestedAutomations,
      suggestedGuideImprovements,
    });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Failed to compute intelligence data." }, { status: 500 });
  }
}
