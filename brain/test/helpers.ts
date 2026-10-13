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
