import { DatabaseSync } from "node:sqlite";
import type { SqlLike } from "../src/store";

// Stand-in for the Durable Object's ctx.storage.sql with the same exec(...).toArray() shape.
export function memSql(): SqlLike {
  const db = new DatabaseSync(":memory:");
  return {
    exec(q: string, ...b: unknown[]) {
      const st = db.prepare(q);
      if (/^\s*(select|with)\b/i.test(q) || /\breturning\b/i.test(q)) {
        const rows = st.all(...(b as never[])) as Record<string, unknown>[];
        return { toArray: () => rows };
      }
      st.run(...(b as never[]));
      return { toArray: () => [] };
    },
  };
}
import type { ClaudeReply, CreateMessage } from "../src/mind";
// Scripted Claude: returns the queued replies in order and records every request.
export function fakeClaude(replies: ClaudeReply[]): CreateMessage & { calls: Record<string, unknown>[] } {
  const calls: Record<string, unknown>[] = [];
  const fn = (async (p: Record<string, unknown>) => { calls.push(p); const r = replies.shift(); if (!r) throw new Error("no reply"); return r; }) as CreateMessage & { calls: Record<string, unknown>[] };
  fn.calls = calls;
  return fn;
}
export const usage = { input_tokens: 1000, output_tokens: 100 };
