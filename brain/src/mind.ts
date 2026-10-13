import Anthropic from "@anthropic-ai/sdk";
import { claudeCost } from "./spend";
import type { Card, Message } from "./types";

export interface ClaudeReply { content: Array<{ type: string; [k: string]: unknown }>; stop_reason: string | null; usage: { input_tokens: number; output_tokens: number; cache_read_input_tokens?: number | null } }
export type CreateMessage = (params: Record<string, unknown>) => Promise<ClaudeReply>;

export function makeCreateMessage(apiKey: string): CreateMessage {
  const client = new Anthropic({ apiKey, maxRetries: 0 });
  // `fallbacks` and `betas` ride on the beta surface; cast because the request is assembled generically.
  return async (params) => (await client.beta.messages.create(params as never, { timeout: 8000 })) as unknown as ClaudeReply;
}

export const MIND_TOOLS = [
  {
    name: "remember",
    description: "Save one short fact about Avi or your shared life that you want to remember in future conversations.",
    input_schema: { type: "object", properties: { fact: { type: "string" } }, required: ["fact"], additionalProperties: false },
    strict: true,
  },
  {
    name: "show_card",
    description: "Show something on the badge screen (and in Avi's chat): a short title, an optional one-line body, and optionally a URL to show as a QR code. Use for links, numbers, or anything better seen than heard.",
    input_schema: {
      type: "object",
      properties: { title: { type: "string", maxLength: 23 }, body: { type: "string", maxLength: 63 }, qr: { type: "string", maxLength: 120 } },
      required: ["title"], additionalProperties: false,
    },
    strict: true,
  },
];

export interface MindDeps { create: CreateMessage; model: string; effort?: string; remember(fact: string): void; showCard(card: Card): void }
// Server-side refusal fallbacks exist for Opus/Sonnet/Fable 5.x; Haiku 5.5 has none and must not send the parameter.
const supportsFallbacks = (model: string) => /^claude-(opus|sonnet|fable)-5/.test(model);

function toParams(history: Message[]): Array<{ role: "user" | "assistant"; content: string }> {
  // Merge consecutive same-role turns; the API needs alternating roles starting with the user.
  const out: Array<{ role: "user" | "assistant"; content: string }> = [];
  for (const m of history) {
    const role = m.role === "user" ? "user" : "assistant";
    const tag = m.channel === "voice" ? "(said aloud) " : "";
    if (out.length && out[out.length - 1].role === role) out[out.length - 1].content += `\n${tag}${m.text}`;
    else out.push({ role, content: `${tag}${m.text}` });
  }
  while (out.length && out[0].role !== "user") out.shift();
  return out;
}

export async function think(deps: MindDeps, req: { system: string; history: Message[]; question: string }): Promise<{ text: string; usd: number }> {
  const messages: Array<{ role: "user" | "assistant"; content: unknown }> = [...toParams(req.history), { role: "user", content: req.question }];
  // If history ended on a user turn, fold the question into it to keep roles alternating.
  if (messages.length >= 2 && messages[messages.length - 2].role === "user") {
    const q = messages.pop()!; (messages[messages.length - 1] as { content: string }).content += `\n${q.content as string}`;
  }
  let usd = 0;
  try {
    for (let round = 0; round < 3; round++) {
      const r = await deps.create({
        model: deps.model, max_tokens: 2000, system: req.system, messages, tools: MIND_TOOLS,
        output_config: { effort: deps.effort ?? "medium" },
        ...(supportsFallbacks(deps.model) ? { betas: ["server-side-fallback-2026-07-01"], fallbacks: "default" } : {}),
      });
      usd += claudeCost(deps.model, r.usage);
      if (r.stop_reason === "refusal") return { text: "I'd rather not talk about that.", usd };
      if (r.stop_reason !== "tool_use") {
        const text = r.content.filter((b) => b.type === "text").map((b) => String(b.text)).join(" ").trim();
        return { text: text || "Hmm.", usd };
      }
      messages.push({ role: "assistant", content: r.content });
      const results = r.content.filter((b) => b.type === "tool_use").map((b) => {
        const input = b.input as Record<string, string>;
        if (b.name === "remember") deps.remember(String(input.fact).slice(0, 200));
        else if (b.name === "show_card") deps.showCard({ title: String(input.title).slice(0, 23), ...(input.body ? { body: String(input.body).slice(0, 63) } : {}), ...(input.qr ? { qr: String(input.qr).slice(0, 120) } : {}) });
        return { type: "tool_result", tool_use_id: b.id, content: "done" };
      });
      messages.push({ role: "user", content: results });
    }
  } catch {
    return { text: "I couldn't think of it just now.", usd };
  }
  return { text: "I couldn't think of it just now.", usd };
}
