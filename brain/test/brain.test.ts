import { beforeEach, describe, expect, it, vi } from "vitest";
import { PetBrain, type BrainDeps } from "../src/brain";
import { PetStore } from "../src/store";
import { FakeWs, fakeClaude, memSql, usage } from "./helpers";
import type { ClaudeReply } from "../src/mind";

const snap = { mood: "happy", food: 90, energy: 80, joy: 95, hygiene: 100, trust: 70, sick: 0, life: "normal" as const, asleep: false, ageDays: 2, growth: 0 };

function setup(replies: ClaudeReply[] = [{ content: [{ type: "text", text: "Hi!" }], stop_reason: "end_turn", usage }]) {
  const store = new PetStore(memSql());
  const toBadge: unknown[] = []; const pushed: unknown[] = [];
  const upstream = new FakeWs();
  const claude = fakeClaude(replies);
  const deps: BrainDeps = {
    store, now: () => Date.UTC(2026, 9, 12, 18, 0),
    sendBadgeJson: (o) => toBadge.push(o), sendBadgeAudio: (b) => toBadge.push({ audio: b.byteLength }), closeBadge: () => toBadge.push("closed"),
    openUpstream: async () => upstream, createMessage: claude, pushApp: (e) => pushed.push(e),
    claudeModel: "claude-sonnet-5-5", transcribeModel: "gpt-4o-mini-transcribe", sha256: async (s) => `h:${s}`, randomToken: () => "tok123",
  };
  return { brain: new PetBrain(deps), store, toBadge, pushed, upstream, claude };
}

describe("PetBrain", () => {
  beforeEach(() => vi.useFakeTimers());

  it("stores the badge snapshot from hello and pushes it to the app", async () => {
    const { brain, store, pushed } = setup();
    await brain.onBadgeJson(JSON.stringify({ t: "hello", fw: "2.0.0", snapshot: snap }));
    expect(store.snapshot()?.snapshot.mood).toBe("happy");
    expect(pushed).toContainEqual(expect.objectContaining({ kind: "snapshot" }));
  });

  it("talk_start opens the realtime session and shows listening; audio and end are relayed", async () => {
    const { brain, toBadge, upstream } = setup();
    await brain.onBadgeJson(JSON.stringify({ t: "talk_start" }));
    expect(toBadge).toContainEqual({ t: "face", f: "listening" });
    brain.onBadgeAudio(new Uint8Array([1, 2, 3, 4]).buffer);
    await brain.onBadgeJson(JSON.stringify({ t: "talk_end" }));
    expect(upstream.types()).toEqual(expect.arrayContaining(["session.update", "input_audio_buffer.append", "input_audio_buffer.commit", "response.create"]));
  });

  it("voice transcripts land in shared memory and reach the app; say_end on done", async () => {
    const { brain, store, toBadge, pushed, upstream } = setup();
    await brain.onBadgeJson(JSON.stringify({ t: "talk_start" }));
    await brain.onBadgeJson(JSON.stringify({ t: "talk_end" }));
    upstream.emit({ type: "conversation.item.input_audio_transcription.completed", transcript: "my favorite snack is mango" });
    upstream.emit({ type: "response.output_audio_transcript.done", transcript: "Mango, yum!" });
    upstream.emit({ type: "response.done", response: { status: "completed", output: [], usage: { input_token_details: { text_tokens: 0, audio_tokens: 0, cached_tokens: 0 }, output_token_details: { text_tokens: 0, audio_tokens: 0 } } } });
    expect(store.recentMessages(5).map((m) => [m.channel, m.role, m.text])).toEqual([["voice", "user", "my favorite snack is mango"], ["voice", "pet", "Mango, yum!"]]);
    expect(pushed.filter((p: any) => p.kind === "message")).toHaveLength(2);
    expect(toBadge).toContainEqual({ t: "say_end" });
  });

  it("text chat uses the same memory and returns the reply", async () => {
    const { brain, store, claude } = setup();
    store.addMessage("voice", "user", "my favorite snack is mango", 1);
    const r = await brain.chat("what's my favorite snack?");
    expect(r).toEqual({ reply: "Hi!" });
    expect(JSON.stringify(claude.calls[0].messages)).toContain("mango");
    expect(store.recentMessages(5).at(-1)).toMatchObject({ channel: "text", role: "pet", text: "Hi!" });
  });

  it("show_card from the mind reaches the badge and the app", async () => {
    const { brain, toBadge, pushed } = setup([
      { content: [{ type: "tool_use", id: "t", name: "show_card", input: { title: "LINK", qr: "https://a.b" } }], stop_reason: "tool_use", usage },
      { content: [{ type: "text", text: "There!" }], stop_reason: "end_turn", usage },
    ]);
    const r = await brain.chat("show me the link");
    expect(toBadge).toContainEqual({ t: "card", title: "LINK", qr: "https://a.b" });
    expect(pushed).toContainEqual(expect.objectContaining({ kind: "card" }));
    expect(r).toEqual({ reply: "There!", card: { title: "LINK", qr: "https://a.b" } });
  });

  it("enforces the daily spend cap for voice and text", async () => {
    const { brain, store, toBadge } = setup();
    store.addSpend("2026-10-12", 5);
    await brain.onBadgeJson(JSON.stringify({ t: "talk_start" }));
    expect(toBadge).toContainEqual({ t: "error", code: "over_budget" });
    expect(await brain.chat("hi")).toEqual({ error: "over_budget" });
  });

  it("token: rotate returns a new token once; only the matching token passes", async () => {
    const { brain } = setup();
    expect(await brain.checkToken("anything")).toBe(false);   // no token issued yet
    const { token } = await brain.rotateToken();
    expect(token).toBe("tok123");
    expect(await brain.checkToken("tok123")).toBe(true);
    expect(await brain.checkToken("old")).toBe(false);
  });

  it("badge disconnect mid-reply cancels the upstream response", async () => {
    const { brain, upstream } = setup();
    await brain.onBadgeJson(JSON.stringify({ t: "talk_start" }));
    await brain.onBadgeJson(JSON.stringify({ t: "talk_end" }));
    upstream.emit({ type: "response.output_audio.delta", delta: "AAAA" });
    brain.onBadgeClose();
    expect(upstream.types()).toContain("response.cancel");
  });

  it("networks merge by name (newest wins, max 5), are pushed to the badge, and state masks passwords", async () => {
    const { brain, toBadge } = setup();
    brain.setNetworks([{ ssid: "home", psk: "a" }]);
    brain.setNetworks([{ ssid: "hotspot", psk: "secret" }, { ssid: "home", psk: "b" }]);
    expect(toBadge.at(-1)).toEqual({ t: "networks", list: [{ ssid: "hotspot", psk: "secret" }, { ssid: "home", psk: "b" }] });
    for (let i = 0; i < 6; i++) brain.setNetworks([{ ssid: `n${i}`, psk: "x" }]);
    expect((toBadge.at(-1) as { list: unknown[] }).list).toHaveLength(5);
    expect(brain.removeNetwork("n5").ok).toBe(true);
    expect(JSON.stringify(brain.state())).not.toContain("secret");
  });

  it("closes the upstream session after 30 s idle", async () => {
    const { brain, upstream } = setup();
    await brain.onBadgeJson(JSON.stringify({ t: "talk_start" }));
    await brain.onBadgeJson(JSON.stringify({ t: "talk_end" }));
    upstream.emit({ type: "response.done", response: { status: "completed", output: [], usage: { input_token_details: { text_tokens: 0, audio_tokens: 0, cached_tokens: 0 }, output_token_details: { text_tokens: 0, audio_tokens: 0 } } } });
    await vi.advanceTimersByTimeAsync(30_100);
    expect(upstream.closed).toBe(true);
  });

  it("hello merges the badge's saved networks and pushes the merged list back", async () => {
    const { brain, store, toBadge } = setup();
    store.setNetworks([{ ssid: "hotspot", psk: "h" }]);   // added from the app earlier
    await brain.onBadgeJson(JSON.stringify({ t: "hello", fw: "2.0.0", snapshot: snap, networks: [{ ssid: "home", psk: "w" }] }));
    expect(toBadge.at(-1)).toEqual({ t: "networks", list: [{ ssid: "hotspot", psk: "h" }, { ssid: "home", psk: "w" }] });
  });

  it("uses the badge's local hour for night, not UTC", async () => {
    const { brain, claude } = setup();
    await brain.onBadgeJson(JSON.stringify({ t: "hello", fw: "2.0.0", snapshot: { ...snap, hour: 2 } }));
    await brain.chat("hi");
    expect(String(claude.calls[0].system)).toMatch(/late at night/i);
  });

  it("ignores malformed badge frames", async () => {
    const { brain, toBadge } = setup();
    await brain.onBadgeJson("{not json");
    await brain.onBadgeJson(JSON.stringify({ t: "nope" }));
    expect(toBadge).toEqual([]);
  });
});
