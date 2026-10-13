import { describe, expect, it } from "vitest";
import { buildSystemPrompt } from "../src/personality";
import { DEFAULT_SETTINGS, type Snapshot } from "../src/types";

const base: Snapshot = { mood: "content", food: 80, energy: 80, joy: 60, hygiene: 90, trust: 60, sick: 0, life: "normal", asleep: false, ageDays: 3, growth: 0 };

describe("buildSystemPrompt", () => {
  it("names the pet and keeps voice replies short", () => {
    const p = buildSystemPrompt(base, DEFAULT_SETTINGS, [], 14, "voice");
    expect(p).toContain("You are Charm");
    expect(p).toMatch(/one or two short sentences/i);
  });
  it("is guarded at low trust or right after being won back", () => {
    expect(buildSystemPrompt({ ...base, trust: 20 }, DEFAULT_SETTINGS, [], 14, "voice")).toMatch(/guarded/i);
    expect(buildSystemPrompt({ ...base, life: "returning" }, DEFAULT_SETTINGS, [], 14, "voice")).toMatch(/just came back/i);
  });
  it("is playful when joy is high and weak when sick", () => {
    expect(buildSystemPrompt({ ...base, joy: 90 }, DEFAULT_SETTINGS, [], 14, "text")).toMatch(/playful/i);
    expect(buildSystemPrompt({ ...base, sick: 2 }, DEFAULT_SETTINGS, [], 14, "text")).toMatch(/sick/i);
  });
  it("mentions hunger, sleep, night and memories", () => {
    const p = buildSystemPrompt({ ...base, food: 10, asleep: true }, DEFAULT_SETTINGS, ["Avi likes mango"], 2, "voice");
    expect(p).toMatch(/hungry/i); expect(p).toMatch(/asleep/i); expect(p).toMatch(/late at night/i);
    expect(p).toContain("Avi likes mango");
  });
  it("handles no snapshot yet", () => {
    expect(buildSystemPrompt(null, DEFAULT_SETTINGS, [], 14, "text")).toMatch(/haven't felt your body/i);
  });
});
