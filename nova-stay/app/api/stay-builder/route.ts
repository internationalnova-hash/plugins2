import { NextRequest, NextResponse } from "next/server";
import Anthropic from "@anthropic-ai/sdk";
import { isAuthorizedHost } from "@/lib/hostAuth";
import property from "@/config/property";

const GUIDE_FIELDS = [
  "welcomeMessage",
  "checkinInstructions",
  "parkingInstructions",
  "checkoutInstructions",
  "houseRules",
  "amenitiesHighlights",
  "diningRecommendations",
  "faq",
  "emergencyContacts",
] as const;

const GUIDE_TOOL: Anthropic.Tool = {
  name: "generate_guide",
  description: "Write the finished, guest-facing copy for each section of the property guide.",
  input_schema: {
    type: "object",
    properties: Object.fromEntries(
      GUIDE_FIELDS.map((key) => [key, { type: "string", description: `Polished guest-facing text for ${key}.` }])
    ),
    required: [...GUIDE_FIELDS],
  },
};

export async function POST(req: NextRequest) {
  if (!(await isAuthorizedHost(req))) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const apiKey = process.env.ANTHROPIC_API_KEY;
  if (!apiKey) {
    return NextResponse.json({ error: "ANTHROPIC_API_KEY is not configured." }, { status: 500 });
  }

  const body = await req.json().catch(() => ({}));
  const answers = body?.answers;
  if (!answers || typeof answers !== "object") {
    return NextResponse.json({ error: "Missing answers" }, { status: 400 });
  }

  try {
    const client = new Anthropic({ apiKey });
    const response = await client.messages.create({
      model: "claude-sonnet-4-6",
      max_tokens: 1500,
      system:
        `You are helping a host finish setting up their ${property.name} property guide on StayByNova. ` +
        `You're given the host's raw, informal notes for each section of the guide. Turn each one into warm, ` +
        `clear, guest-facing copy — the kind of writing a thoughtful host would actually send a guest. ` +
        `Keep facts (codes, times, addresses, names) exactly as given — never invent details the host didn't ` +
        `provide. If a section's raw notes are empty or missing, write a short, reasonable generic placeholder ` +
        `instead of leaving it blank, and keep it brief. No markdown syntax — plain text only.`,
      tools: [GUIDE_TOOL],
      tool_choice: { type: "tool", name: "generate_guide" },
      messages: [
        {
          role: "user",
          content: `Host's raw notes per section:\n\n${JSON.stringify(answers, null, 2)}`,
        },
      ],
    });

    const toolUse = response.content.find(
      (block): block is Anthropic.ToolUseBlock => block.type === "tool_use" && block.name === "generate_guide"
    );
    if (!toolUse) {
      return NextResponse.json({ error: "The assistant did not return a guide." }, { status: 500 });
    }

    return NextResponse.json({ guide: toolUse.input });
  } catch (err: any) {
    return NextResponse.json({ error: err?.message || "Failed to generate guide." }, { status: 500 });
  }
}
