import { DurableObject } from "cloudflare:workers";
import { PetBrain } from "./brain";
import { makeCreateMessage } from "./mind";
import { PetStore } from "./store";
import type { WsLike } from "./realtime";

export interface Env {
  PET: DurableObjectNamespace<Pet>;
  OPENAI_API_KEY: string; ANTHROPIC_API_KEY: string; APP_SHARED_SECRET: string;
  CHARMING_WEBHOOK_URL?: string; CHARMING_WEBHOOK_KEY?: string;
  REALTIME_MODEL: string; CLAUDE_MODEL: string; TRANSCRIBE_MODEL: string;
}

const hex = (buf: ArrayBuffer) => [...new Uint8Array(buf)].map((b) => b.toString(16).padStart(2, "0")).join("");

export class Pet extends DurableObject<Env> {
  private brain: PetBrain;
  constructor(ctx: DurableObjectState, env: Env) {
    super(ctx, env);
    const store = new PetStore(ctx.storage.sql as never);
    if (!store.settings().realtimeModel) store.setSettings({ realtimeModel: env.REALTIME_MODEL });
    const badge = () => ctx.getWebSockets("badge")[0];
    this.brain = new PetBrain({
      store, now: () => Date.now(),
      sendBadgeJson: (o) => badge()?.send(JSON.stringify(o)),
      // Reply audio arrives in large deltas; the badge allocates each frame in internal RAM, so send
      // small 80 ms frames (3840 bytes at 24 kHz, 16-bit mono) to keep its socket alive.
      sendBadgeAudio: (b) => { const ws = badge(); if (!ws) return; for (let i = 0; i < b.byteLength; i += 3840) ws.send(b.slice(i, Math.min(i + 3840, b.byteLength))); },
      closeBadge: () => ctx.getWebSockets("badge").forEach((w) => w.close(4001, "token rotated")),
      openUpstream: async (model) => {
        const res = await fetch(`https://api.openai.com/v1/realtime?model=${encodeURIComponent(model)}`, {
          headers: { Upgrade: "websocket", Authorization: `Bearer ${env.OPENAI_API_KEY}` },
        });
        const ws = res.webSocket;
        if (!ws) throw new Error(`realtime upgrade failed: ${res.status}`);
        ws.accept();
        return ws as unknown as WsLike;
      },
      createMessage: makeCreateMessage(env.ANTHROPIC_API_KEY),
      pushApp: (event) => {
        if (!env.CHARMING_WEBHOOK_URL || !env.CHARMING_WEBHOOK_KEY) return;
        ctx.waitUntil(fetch(env.CHARMING_WEBHOOK_URL, {
          method: "POST", headers: { "Content-Type": "application/json", Authorization: `Bearer ${env.CHARMING_WEBHOOK_KEY}` }, body: JSON.stringify(event),
        }).then(() => undefined, () => undefined));
      },
      claudeModel: env.CLAUDE_MODEL, transcribeModel: env.TRANSCRIBE_MODEL,
      sha256: async (s) => hex(await crypto.subtle.digest("SHA-256", new TextEncoder().encode(s))),
      randomToken: () => hex(crypto.getRandomValues(new Uint8Array(32)).buffer),
    });
  }

  async fetch(req: Request): Promise<Response> {
    const url = new URL(req.url);
    if (url.pathname === "/badge") {
      const bearer = (req.headers.get("Authorization") ?? "").replace(/^Bearer\s+/i, "");
      if (!(await this.brain.checkToken(bearer))) return new Response("unauthorized", { status: 401 });
      if (req.headers.get("Upgrade") !== "websocket") return new Response("expected websocket", { status: 426 });
      this.ctx.getWebSockets("badge").forEach((w) => w.close(4000, "replaced by a newer connection"));
      const [client, server] = Object.values(new WebSocketPair());
      this.ctx.acceptWebSocket(server, ["badge"]);
      console.log("badge socket opened");
      this.brain.onBadgeOpen();
      return new Response(null, { status: 101, webSocket: client });
    }
    const body = req.method === "GET" ? {} : await req.json().catch(() => ({}));
    const json = (v: unknown, status = 200) => Response.json(v, { status });
    switch (`${req.method} ${url.pathname}`) {
      case "POST /chat": return json(await this.brain.chat(String((body as { text?: unknown }).text ?? "").slice(0, 2000)));
      case "GET /state": return json(this.brain.state());
      case "PUT /settings": return json(this.brain.setSettings(body as never));
      case "PUT /networks": return json(this.brain.setNetworks(((body as { list?: unknown }).list ?? []) as never));
      case "DELETE /networks": return json(this.brain.removeNetwork(String((body as { ssid?: unknown }).ssid ?? "")));
      case "POST /device-token/rotate": return json(await this.brain.rotateToken());
      case "POST /compact": return json(await this.brain.compact());
      default: return json({ error: "not_found" }, 404);
    }
  }
  async webSocketMessage(_ws: WebSocket, msg: string | ArrayBuffer) {
    if (typeof msg === "string") await this.brain.onBadgeJson(msg); else this.brain.onBadgeAudio(msg);
  }
  async webSocketClose(_ws: WebSocket, code: number) { console.log("badge socket closed", code); this.brain.onBadgeClose(); }
}
