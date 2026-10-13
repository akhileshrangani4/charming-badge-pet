import { think, type CreateMessage } from "./mind";
import { buildSystemPrompt } from "./personality";
import { RealtimeLink, type WsLike } from "./realtime";
import { dayKey, realtimeCost } from "./spend";
import type { PetStore } from "./store";
import type { Card, Message, NetworkEntry, Settings, Snapshot } from "./types";

export interface BrainDeps {
  store: PetStore; now(): number;
  sendBadgeJson(o: unknown): void; sendBadgeAudio(b: ArrayBuffer): void; closeBadge(): void;
  openUpstream(model: string): Promise<WsLike>; createMessage: CreateMessage;
  pushApp(event: { kind: "message"; message: Message } | { kind: "snapshot"; snapshot: Snapshot } | { kind: "card"; card: Card }): void;
  claudeModel: string; claudeEffort?: string; transcribeModel: string;
  sha256(s: string): Promise<string>; randomToken(): string;
}

export class PetBrain {
  private link: RealtimeLink | null = null;
  private idle: ReturnType<typeof setTimeout> | null = null;
  private lastCard: Card | undefined;
  constructor(private d: BrainDeps) {}

  // The badge's local hour from its last snapshot; UTC only as a fallback (Workers run in UTC).
  private hourLocal() { return this.d.store.snapshot()?.snapshot.hour ?? new Date(this.d.now()).getUTCHours(); }
  private prompt(channel: "voice" | "text") {
    return buildSystemPrompt(this.d.store.snapshot()?.snapshot ?? null, this.d.store.settings(), this.d.store.memories(), this.hourLocal(), channel);
  }
  private overBudget() { return this.d.store.spend(dayKey(this.d.now())) >= this.d.store.settings().dailyCapUsd; }
  private addSpend(usd: number) { if (usd > 0) this.d.store.addSpend(dayKey(this.d.now()), usd); }
  private message(channel: Message["channel"], role: Message["role"], text: string) {
    if (!text.trim()) return;
    const m = this.d.store.addMessage(channel, role, text.trim(), this.d.now());
    if (channel === "voice") this.d.pushApp({ kind: "message", message: m });
  }
  private card = (c: Card) => { this.lastCard = c; this.d.sendBadgeJson({ t: "card", ...c }); this.d.pushApp({ kind: "card", card: c }); };
  private async mind(channel: "voice" | "text", question: string) {
    const r = await think({ create: this.d.createMessage, model: this.d.claudeModel, effort: this.d.claudeEffort, remember: (f) => this.d.store.addMemory(f, this.d.now()), showCard: this.card },
      { system: this.prompt(channel), history: this.d.store.recentMessages(30), question });
    this.addSpend(r.usd);
    return r.text;
  }

  private ensureLink(): RealtimeLink {
    if (this.link) return this.link;
    const settings: Settings = this.d.store.settings();
    this.link = new RealtimeLink(() => this.d.openUpstream(settings.realtimeModel),
      { model: settings.realtimeModel, voice: settings.voice, instructions: this.prompt("voice"), transcribeModel: this.d.transcribeModel },
      {
        audio: (b) => this.d.sendBadgeAudio(b),
        face: (f) => this.d.sendBadgeJson({ t: "face", f }),
        done: () => { console.log("reply done"); this.d.sendBadgeJson({ t: "say_end" }); this.armIdle(); },
        userTranscript: (t) => this.message("voice", "user", t),
        petTranscript: (t) => this.message("voice", "pet", t),
        usage: (u) => this.addSpend(realtimeCost(settings.realtimeModel, u)),
        think: (q) => this.mind("voice", q),
        error: (code) => { this.d.sendBadgeJson({ t: "error", code }); this.armIdle(); },
      });
    return this.link;
  }
  private armIdle() {
    if (this.idle) clearTimeout(this.idle);
    this.idle = setTimeout(() => { this.link?.close(); this.link = null; }, 30_000);
  }

  // ---- badge socket ----
  // Networks are pushed only after hello merges the badge's own list, so the badge never gets an
  // empty list on its first connection and forgets its Wi-Fi.
  onBadgeOpen() {}
  async onBadgeJson(text: string) {
    let m: { t?: string; snapshot?: Snapshot; [k: string]: unknown };
    try { m = JSON.parse(text); } catch { return; }
    if (m.t !== "snapshot") console.log("badge:", m.t);
    switch (m.t) {
      case "hello":
        if (m.snapshot) this.setSnapshot(m.snapshot);
        if (Array.isArray(m.networks)) this.setNetworks(m.networks as NetworkEntry[]);
        else this.d.sendBadgeJson({ t: "networks", list: this.d.store.networks() });
        break;
      case "add_network": if (typeof m.ssid === "string") this.setNetworks([{ ssid: m.ssid, psk: String(m.psk ?? "") }]); break;
      case "snapshot": { const { t: _t, ...s } = m; this.setSnapshot(s as unknown as Snapshot); break; }
      case "talk_start": {
        if (this.idle) clearTimeout(this.idle);
        if (this.overBudget()) { this.d.sendBadgeJson({ t: "error", code: "over_budget" }); return; }
        const link = this.ensureLink();
        this.d.sendBadgeJson({ t: "face", f: "listening" });
        try { await link.ensure(); } catch (err) { console.error("realtime open failed", String(err)); this.d.sendBadgeJson({ t: "error", code: "brain_fog" }); return; }
        link.startTurn();
        break;
      }
      case "talk_end": console.log("badge audio bytes this turn:", this.audioIn); this.audioIn = 0; this.link?.endTurn(); break;
      case "talk_cancel": this.link?.cancelTurn(); this.d.sendBadgeJson({ t: "face", f: "idle" }); this.armIdle(); break;
      default: return;
    }
  }
  private audioIn = 0;
  onBadgeAudio(buf: ArrayBuffer) { this.audioIn += buf.byteLength; this.link?.appendAudio(buf); }
  onBadgeClose() { this.link?.cancelTurn(); this.armIdle(); }
  private setSnapshot(s: Snapshot) {
    this.d.store.setSnapshot(s, this.d.now());
    this.d.pushApp({ kind: "snapshot", snapshot: s });
    this.link?.setInstructions(this.prompt("voice"));
  }

  // ---- app routes ----
  async chat(text: string): Promise<{ reply: string; card?: Card } | { error: string }> {
    if (this.overBudget()) return { error: "over_budget" };
    this.lastCard = undefined;
    const history = this.d.store.recentMessages(30);
    this.d.store.addMessage("text", "user", text, this.d.now());
    const r = await think({ create: this.d.createMessage, model: this.d.claudeModel, effort: this.d.claudeEffort, remember: (f) => this.d.store.addMemory(f, this.d.now()), showCard: this.card },
      { system: this.prompt("text"), history, question: text });
    this.addSpend(r.usd);
    this.d.store.addMessage("text", "pet", r.text, this.d.now());
    return this.lastCard ? { reply: r.text, card: this.lastCard } : { reply: r.text };
  }
  state() {
    const s = this.d.store.snapshot();
    return {
      snapshot: s?.snapshot ?? null, snapshotTs: s?.ts ?? null, settings: this.d.store.settings(),
      spendToday: this.d.store.spend(dayKey(this.d.now())),
      networks: this.d.store.networks().map((n) => ({ ssid: n.ssid })),
      messages: this.d.store.recentMessages(100),
    };
  }
  setSettings(patch: Partial<Settings>) { this.d.store.setSettings(patch); if (patch.voice || patch.realtimeModel) { this.link?.close(); this.link = null; } return this.d.store.settings(); }
  // Merge by SSID so adding one network from the app never drops the others; newest wins, max 5.
  setNetworks(list: NetworkEntry[]) {
    const incoming = list.filter((n) => n.ssid).map((n) => n.ssid);
    const kept = this.d.store.networks().filter((n) => !incoming.includes(n.ssid));
    this.d.store.setNetworks([...kept, ...list.filter((n) => n.ssid)].slice(-5));
    this.d.sendBadgeJson({ t: "networks", list: this.d.store.networks() });
    return { ok: true };
  }
  removeNetwork(ssid: string) {
    this.d.store.setNetworks(this.d.store.networks().filter((n) => n.ssid !== ssid));
    this.d.sendBadgeJson({ t: "networks", list: this.d.store.networks() });
    return { ok: true };
  }
  async rotateToken() {
    const token = this.d.randomToken();
    this.d.store.setTokenHash(await this.d.sha256(token));
    this.d.closeBadge();   // the old token's socket goes away immediately
    return { token };
  }
  async checkToken(bearer: string) { const h = this.d.store.tokenHash(); return !!h && h === (await this.d.sha256(bearer)); }
  async compact() {
    if (this.d.store.countMessages() <= 500) return { compacted: 0 };
    const old = this.d.store.oldestMessages(400);
    const r = await think({ create: this.d.createMessage, model: this.d.claudeModel, effort: this.d.claudeEffort, remember: (f) => this.d.store.addMemory(f, this.d.now()), showCard: () => {} },
      { system: "You condense a pet's chat log into durable memories. Call remember once per important fact about Avi or your shared life (at most 15). Then reply 'done'.", history: [], question: old.map((m) => `${m.role}: ${m.text}`).join("\n") });
    this.addSpend(r.usd);
    this.d.store.deleteMessagesThrough(old[old.length - 1].id);
    return { compacted: old.length };
  }
}
