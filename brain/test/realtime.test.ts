import { beforeEach, describe, expect, it, vi } from "vitest";
import { RealtimeLink, type RealtimeEvents } from "../src/realtime";
import { FakeWs } from "./helpers";

function setup(thinkImpl: (q: string) => Promise<string> = async () => "it's 3pm") {
  const ws = new FakeWs();
  const log: string[] = [];
  const ev: RealtimeEvents = {
    audio: (b) => log.push(`audio:${b.byteLength}`), face: (f) => log.push(`face:${f}`), done: () => log.push("done"),
    userTranscript: (t) => log.push(`user:${t}`), petTranscript: (t) => log.push(`pet:${t}`), usage: () => log.push("usage"),
    think: (q) => { log.push(`think:${q}`); return thinkImpl(q); }, error: (c) => log.push(`error:${c}`),
  };
  const link = new RealtimeLink(async () => ws, { model: "gpt-realtime-2.1", voice: "coral", instructions: "be a flame", transcribeModel: "gpt-transcribe" }, ev);
  return { ws, log, link };
}

describe("RealtimeLink", () => {
  beforeEach(() => vi.useFakeTimers());

  it("configures push-to-talk audio once on open", async () => {
    const { ws, link } = setup();
    await link.ensure(); await link.ensure();
    const upd = ws.sent.filter((m) => (m as { type: string }).type === "session.update");
    expect(upd).toHaveLength(1);
    const s = (upd[0] as { session: Record<string, any> }).session;
    expect(s.audio.input.turn_detection).toBeNull();
    expect(s.audio.input.format).toEqual({ type: "audio/pcm", rate: 24000 });
    expect(s.audio.output.voice).toBe("coral");
    expect(s.tools[0].name).toBe("think");
  });

  it("a voice turn: append, commit, create, then audio and transcripts come back", async () => {
    const { ws, log, link } = setup();
    await link.ensure();
    link.startTurn();
    link.appendAudio(new Uint8Array([1, 0, 2, 0]).buffer);
    link.endTurn();
    expect(ws.types().slice(-3)).toEqual(["input_audio_buffer.append", "input_audio_buffer.commit", "response.create"]);
    ws.emit({ type: "conversation.item.input_audio_transcription.completed", transcript: "what time is it" });
    ws.emit({ type: "response.output_audio.delta", delta: Buffer.from([0, 0, 1, 0]).toString("base64") });
    ws.emit({ type: "response.output_audio_transcript.done", transcript: "Teatime!" });
    ws.emit({ type: "response.done", response: { status: "completed", output: [], usage: { input_token_details: { text_tokens: 1, audio_tokens: 1, cached_tokens: 0 }, output_token_details: { text_tokens: 1, audio_tokens: 1 } } } });
    expect(log).toEqual(["user:what time is it", "face:speaking", "audio:4", "pet:Teatime!", "usage", "done"]);
  });

  it("think: shows the thinking face, calls the mind, returns the output, asks for a new response", async () => {
    const { ws, log, link } = setup();
    await link.ensure(); link.startTurn(); link.endTurn();
    ws.emit({ type: "response.done", response: { status: "completed", usage: { input_token_details: { text_tokens: 0, audio_tokens: 0, cached_tokens: 0 }, output_token_details: { text_tokens: 0, audio_tokens: 0 } }, output: [{ type: "function_call", name: "think", call_id: "c1", arguments: JSON.stringify({ question: "time?" }) }] } });
    await vi.advanceTimersByTimeAsync(10);   // let the mind resolve, but not the 10 s brain-fog timer
    expect(log).toContain("face:thinking"); expect(log).toContain("think:time?");
    const out = ws.sent.find((m) => (m as any).type === "conversation.item.create") as any;
    expect(out.item).toEqual({ type: "function_call_output", call_id: "c1", output: "it's 3pm" });
    expect(ws.types().at(-1)).toBe("response.create");
  });

  it("think that never answers still returns an output after 8 s", async () => {
    const { ws, link } = setup(() => new Promise(() => {}));
    await link.ensure(); link.startTurn(); link.endTurn();
    ws.emit({ type: "response.done", response: { status: "completed", usage: { input_token_details: { text_tokens: 0, audio_tokens: 0, cached_tokens: 0 }, output_token_details: { text_tokens: 0, audio_tokens: 0 } }, output: [{ type: "function_call", name: "think", call_id: "c2", arguments: "{\"question\":\"x\"}" }] } });
    await vi.advanceTimersByTimeAsync(8100);
    const out = ws.sent.find((m) => (m as any).type === "conversation.item.create") as any;
    expect(out.item.output).toMatch(/couldn't think/i);
  });

  it("talking over a reply (barge-in) cancels it first, and the cancelled done is not a real done", async () => {
    const { ws, log, link } = setup();
    await link.ensure(); link.startTurn(); link.endTurn();
    ws.emit({ type: "response.output_audio.delta", delta: "AAAA" });
    link.startTurn();
    expect(ws.types()).toContain("response.cancel");
    expect(ws.types().at(-1)).toBe("input_audio_buffer.clear");
    ws.emit({ type: "response.done", response: { status: "cancelled", output: [], usage: { input_token_details: { text_tokens: 0, audio_tokens: 0, cached_tokens: 0 }, output_token_details: { text_tokens: 0, audio_tokens: 0 } } } });
    expect(log).not.toContain("done");
    expect(log).toContain("usage");
  });

  it("audio sent before the socket opens is kept and sent after the clear", async () => {
    let release!: (w: FakeWs) => void;
    const ws = new FakeWs();
    const ev = { audio() {}, face() {}, done() {}, userTranscript() {}, petTranscript() {}, usage() {}, think: async () => "", error() {} };
    const link = new RealtimeLink(() => new Promise((r) => { release = r as never; }), { model: "m", voice: "coral", instructions: "", transcribeModel: "t" }, ev);
    const opening = link.ensure();
    link.appendAudio(new Uint8Array([1, 0]).buffer);
    link.appendAudio(new Uint8Array([2, 0]).buffer);
    release(ws); await opening;
    link.startTurn();
    expect(ws.types()).toEqual(["session.update", "input_audio_buffer.clear", "input_audio_buffer.append", "input_audio_buffer.append"]);
  });

  it("no audio within 10 s of the end of a turn is brain fog", async () => {
    const { log, link } = setup();
    await link.ensure(); link.startTurn(); link.endTurn();
    await vi.advanceTimersByTimeAsync(10_100);
    expect(log).toContain("error:brain_fog");
  });

  it("cancelTurn clears input and cancels an in-progress response", async () => {
    const { ws, link } = setup();
    await link.ensure(); link.startTurn(); link.endTurn();
    ws.emit({ type: "response.output_audio.delta", delta: "AAAA" });
    link.cancelTurn();
    expect(ws.types().slice(-2)).toEqual(["input_audio_buffer.clear", "response.cancel"]);
  });
});
