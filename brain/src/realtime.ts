import type { RealtimeUsage } from "./spend";
import type { Face } from "./types";

export interface WsLike { send(d: string | ArrayBuffer): void; close(code?: number): void; addEventListener(type: "message" | "close" | "error", fn: (e: { data?: unknown }) => void): void }
export interface RealtimeEvents {
  audio(pcm: ArrayBuffer): void; face(f: Face): void; done(): void;
  userTranscript(t: string): void; petTranscript(t: string): void; usage(u: RealtimeUsage): void;
  think(question: string): Promise<string>; error(code: "brain_fog"): void;
}
interface Timers { set: (fn: () => void, ms: number) => unknown; clear: (h: unknown) => void }

const toB64 = (buf: ArrayBuffer) => { let s = ""; const u = new Uint8Array(buf); for (let i = 0; i < u.length; i++) s += String.fromCharCode(u[i]); return btoa(s); };
const fromB64 = (b64: string) => { const s = atob(b64); const u = new Uint8Array(s.length); for (let i = 0; i < s.length; i++) u[i] = s.charCodeAt(i); return u.buffer; };

export class RealtimeLink {
  private ws: WsLike | null = null;
  private opening: Promise<void> | null = null;
  private responding = false;
  private spoke = false;
  private fogTimer: unknown = null;
  private pending: ArrayBuffer[] = [];   // audio that arrived before the upstream socket opened
  constructor(
    private open: () => Promise<WsLike>,
    private cfg: { model: string; voice: string; instructions: string; transcribeModel: string },
    private ev: RealtimeEvents,
    private timers: Timers = { set: (f, ms) => setTimeout(f, ms), clear: (h) => clearTimeout(h as number) },
  ) {}
  get isOpen() { return this.ws !== null; }

  ensure(): Promise<void> {
    if (this.ws) return Promise.resolve();
    this.opening ??= this.open().then((ws) => {
      this.ws = ws;
      ws.addEventListener("message", (e) => this.onMessage(String(e.data)));
      ws.addEventListener("close", () => { this.ws = null; this.opening = null; this.responding = false; });
      ws.addEventListener("error", () => this.ev.error("brain_fog"));
      this.send({
        type: "session.update",
        session: {
          type: "realtime", model: this.cfg.model, output_modalities: ["audio"], instructions: this.cfg.instructions,
          audio: {
            input: { format: { type: "audio/pcm", rate: 24000 }, turn_detection: null, transcription: { model: this.cfg.transcribeModel } },
            output: { format: { type: "audio/pcm", rate: 24000 }, voice: this.cfg.voice },
          },
          tools: [{
            type: "function", name: "think",
            description: "Ask your deeper mind about anything that needs memory, facts, planning, or showing something on the badge screen. Say a very short in-character filler line first.",
            parameters: { type: "object", properties: { question: { type: "string" } }, required: ["question"] },
          }],
          tool_choice: "auto",
        },
      });
    });
    return this.opening;
  }

  startTurn() {
    if (this.responding) { this.send({ type: "response.cancel" }); this.responding = false; }
    this.clearFog();
    this.send({ type: "input_audio_buffer.clear" });
    // Flush audio queued while the socket was opening; after the clear, or the clear would wipe it.
    for (const b of this.pending.splice(0)) this.send({ type: "input_audio_buffer.append", audio: toB64(b) });
  }
  appendAudio(buf: ArrayBuffer) {
    if (!this.ws) { if (this.pending.length < 50) this.pending.push(buf); return; }   // ~2 s cap
    this.send({ type: "input_audio_buffer.append", audio: toB64(buf) });
  }
  endTurn() {
    this.send({ type: "input_audio_buffer.commit" });
    this.createResponse();
  }
  cancelTurn() {
    this.pending = [];
    this.send({ type: "input_audio_buffer.clear" });
    if (this.responding) { this.send({ type: "response.cancel" }); this.responding = false; }
    this.clearFog();
  }
  setInstructions(text: string) { this.cfg.instructions = text; if (this.ws) this.send({ type: "session.update", session: { type: "realtime", instructions: text } }); }
  close() { this.clearFog(); this.ws?.close(1000); this.ws = null; this.opening = null; }

  private createResponse() {
    this.responding = true; this.spoke = false;
    this.send({ type: "response.create" });
    this.clearFog();
    this.fogTimer = this.timers.set(() => { if (!this.spoke) { this.cancelTurn(); this.ev.error("brain_fog"); } }, 10_000);
  }
  private clearFog() { if (this.fogTimer !== null) { this.timers.clear(this.fogTimer); this.fogTimer = null; } }
  private send(obj: unknown) { this.ws?.send(JSON.stringify(obj)); }

  private onMessage(raw: string) {
    let e: { type?: string; [k: string]: any };
    try { e = JSON.parse(raw); } catch { return; }
    switch (e.type) {
      case "response.output_audio.delta":
        if (!this.spoke) { this.spoke = true; this.clearFog(); this.ev.face("speaking"); }
        this.ev.audio(fromB64(String(e.delta)));
        break;
      case "response.output_audio_transcript.done": this.ev.petTranscript(String(e.transcript ?? "")); break;
      case "conversation.item.input_audio_transcription.completed": this.ev.userTranscript(String(e.transcript ?? "")); break;
      case "error": console.error("realtime error", JSON.stringify(e).slice(0, 500)); this.ev.error("brain_fog"); break;
      case "response.done": {
        this.responding = false;
        if (e.response?.usage) this.ev.usage(e.response.usage as RealtimeUsage);
        if (e.response?.status === "cancelled") return;   // barge-in: a new turn is already under way
        const call = (e.response?.output ?? []).find((o: any) => o.type === "function_call" && o.name === "think");
        if (call) { void this.runThink(String(call.call_id), String(call.arguments ?? "{}")); return; }
        this.clearFog();
        this.ev.done();
        break;
      }
    }
  }

  private async runThink(callId: string, args: string) {
    this.clearFog();
    this.ev.face("thinking");
    let question = "";
    try { question = String(JSON.parse(args).question ?? ""); } catch { /* empty question */ }
    const timeout = new Promise<string>((res) => this.timers.set(() => res("I couldn't think of it just now."), 8000));
    const answer = await Promise.race([this.ev.think(question).catch(() => "I couldn't think of it just now."), timeout]);
    this.send({ type: "conversation.item.create", item: { type: "function_call_output", call_id: callId, output: answer } });
    this.createResponse();
  }
}
