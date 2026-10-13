import { DEFAULT_SETTINGS, type Message, type NetworkEntry, type Settings, type Snapshot } from "./types";

export interface SqlLike { exec(query: string, ...bindings: unknown[]): { toArray(): Record<string, unknown>[] } }

export class PetStore {
  constructor(private sql: SqlLike) {
    // One statement per exec so the same code runs on DO SQL and node:sqlite.
    sql.exec("CREATE TABLE IF NOT EXISTS messages (id INTEGER PRIMARY KEY AUTOINCREMENT, ts INTEGER, channel TEXT, role TEXT, text TEXT)");
    sql.exec("CREATE TABLE IF NOT EXISTS memories (id INTEGER PRIMARY KEY AUTOINCREMENT, ts INTEGER, fact TEXT)");
    sql.exec("CREATE TABLE IF NOT EXISTS networks (ssid TEXT PRIMARY KEY, psk TEXT, ts INTEGER)");
    sql.exec("CREATE TABLE IF NOT EXISTS spend (day TEXT PRIMARY KEY, usd REAL)");
    sql.exec("CREATE TABLE IF NOT EXISTS kv (k TEXT PRIMARY KEY, v TEXT)");
  }
  private kvGet(k: string): string | null {
    const r = this.sql.exec("SELECT v FROM kv WHERE k = ?", k).toArray();
    return r.length ? String(r[0].v) : null;
  }
  private kvSet(k: string, v: string) { this.sql.exec("INSERT INTO kv (k, v) VALUES (?, ?) ON CONFLICT(k) DO UPDATE SET v = excluded.v", k, v); }

  addMessage(channel: Message["channel"], role: Message["role"], text: string, ts: number): Message {
    const r = this.sql.exec("INSERT INTO messages (ts, channel, role, text) VALUES (?, ?, ?, ?) RETURNING id", ts, channel, role, text).toArray();
    return { id: Number(r[0].id), ts, channel, role, text };
  }
  private rowToMsg = (r: Record<string, unknown>): Message => ({ id: Number(r.id), ts: Number(r.ts), channel: r.channel as Message["channel"], role: r.role as Message["role"], text: String(r.text) });
  recentMessages(n: number): Message[] {
    return this.sql.exec("SELECT * FROM messages ORDER BY id DESC LIMIT ?", n).toArray().map(this.rowToMsg).reverse();
  }
  countMessages(): number { return Number(this.sql.exec("SELECT COUNT(*) AS c FROM messages").toArray()[0].c); }
  oldestMessages(n: number): Message[] { return this.sql.exec("SELECT * FROM messages ORDER BY id ASC LIMIT ?", n).toArray().map(this.rowToMsg); }
  deleteMessagesThrough(id: number) { this.sql.exec("DELETE FROM messages WHERE id <= ?", id); }

  addMemory(fact: string, ts: number) { this.sql.exec("INSERT INTO memories (ts, fact) VALUES (?, ?)", ts, fact); }
  memories(): string[] { return this.sql.exec("SELECT fact FROM memories ORDER BY id ASC").toArray().map((r) => String(r.fact)); }

  snapshot(): { snapshot: Snapshot; ts: number } | null {
    const v = this.kvGet("snapshot");
    return v ? (JSON.parse(v) as { snapshot: Snapshot; ts: number }) : null;
  }
  setSnapshot(snapshot: Snapshot, ts: number) { this.kvSet("snapshot", JSON.stringify({ snapshot, ts })); }

  settings(): Settings { const v = this.kvGet("settings"); return { ...DEFAULT_SETTINGS, ...(v ? JSON.parse(v) : {}) }; }
  setSettings(patch: Partial<Settings>) { this.kvSet("settings", JSON.stringify({ ...this.settings(), ...patch })); }

  networks(): NetworkEntry[] { return this.sql.exec("SELECT ssid, psk FROM networks ORDER BY ts ASC").toArray().map((r) => ({ ssid: String(r.ssid), psk: String(r.psk) })); }
  setNetworks(list: NetworkEntry[]) {
    this.sql.exec("DELETE FROM networks");
    list.slice(0, 5).forEach((n, i) => this.sql.exec("INSERT INTO networks (ssid, psk, ts) VALUES (?, ?, ?)", n.ssid, n.psk, i));
  }

  spend(day: string): number { const r = this.sql.exec("SELECT usd FROM spend WHERE day = ?", day).toArray(); return r.length ? Number(r[0].usd) : 0; }
  addSpend(day: string, usd: number) { this.sql.exec("INSERT INTO spend (day, usd) VALUES (?, ?) ON CONFLICT(day) DO UPDATE SET usd = usd + excluded.usd", day, usd); }

  tokenHash(): string | null { return this.kvGet("tokenHash"); }
  setTokenHash(h: string) { this.kvSet("tokenHash", h); }
}
