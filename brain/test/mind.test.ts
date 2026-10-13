import { describe, expect, it } from "vitest";
import { think } from "../src/mind";
import { fakeClaude, usage } from "./helpers";
import type { Card } from "../src/types";

function deps(create: ReturnType<typeof fakeClaude>) {
  const remembered: string[] = []; const cards: Card[] = [];
  return { create, model: "claude-haiku-5-5", remember: (f: string) => remembered.push(f), showCard: (c: Card) => cards.push(c), remembered, cards };
}

describe("think", () => {
  it("returns Claude's text and the cost", async () => {
    const c = fakeClaude([{ content: [{ type: "text", text: "Hi Avi!" }], stop_reason: "end_turn", usage }]);
    const d = deps(c);
    const r = await think(d, { system: "S", history: [], question: "hello?" });
    expect(r.text).toBe("Hi Avi!");
    expect(r.usd).toBeGreaterThan(0);
    expect(c.calls[0]).toMatchObject({ model: "claude-haiku-5-5", output_config: { effort: "medium" } });
    expect(c.calls[0]).not.toHaveProperty("fallbacks");   // Haiku 5.5 has no server-side fallback
  });
  it("sends server-side fallbacks only for models that support them", async () => {
    const c = fakeClaude([{ content: [{ type: "text", text: "ok" }], stop_reason: "end_turn", usage }]);
    await think({ ...deps(c), model: "claude-opus-5-5" }, { system: "S", history: [], question: "?" });
    expect(c.calls[0]).toMatchObject({ fallbacks: "default", betas: ["server-side-fallback-2026-07-01"] });
  });
  it("runs remember and show_card tools, then finishes", async () => {
    const c = fakeClaude([
      { content: [{ type: "tool_use", id: "t1", name: "remember", input: { fact: "likes mango" } }, { type: "tool_use", id: "t2", name: "show_card", input: { title: "MANGO", qr: "https://x.y" } }], stop_reason: "tool_use", usage },
      { content: [{ type: "text", text: "Noted!" }], stop_reason: "end_turn", usage },
    ]);
    const d = deps(c);
    const r = await think(d, { system: "S", history: [], question: "I like mango" });
    expect(d.remembered).toEqual(["likes mango"]);
    expect(d.cards).toEqual([{ title: "MANGO", qr: "https://x.y" }]);
    expect(r.text).toBe("Noted!");
    const second = c.calls[1].messages as Array<{ role: string; content: unknown }>;
    expect(JSON.stringify(second.at(-1))).toContain("tool_result");
  });
  it("answers in character on refusal", async () => {
    const c = fakeClaude([{ content: [], stop_reason: "refusal", usage }]);
    expect((await think(deps(c), { system: "S", history: [], question: "?" })).text).toMatch(/rather not/i);
  });
  it("gives up gracefully when Claude throws or times out", async () => {
    const c = fakeClaude([]);
    expect((await think(deps(c), { system: "S", history: [], question: "?" })).text).toMatch(/couldn't think/i);
  });
  it("stops after 3 tool rounds", async () => {
    const loop = { content: [{ type: "tool_use", id: "t", name: "remember", input: { fact: "x" } }], stop_reason: "tool_use", usage };
    const c = fakeClaude([loop, loop, loop, loop]);
    const r = await think(deps(c), { system: "S", history: [], question: "?" });
    expect(c.calls.length).toBe(3);
    expect(r.text).toMatch(/couldn't think/i);
  });
});
