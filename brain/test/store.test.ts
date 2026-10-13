import { describe, expect, it } from "vitest";
import { PetStore } from "../src/store";
import { memSql } from "./helpers";

describe("PetStore", () => {
  it("keeps one message log for voice and text, oldest first", () => {
    const s = new PetStore(memSql());
    s.addMessage("voice", "user", "hi", 1);
    s.addMessage("text", "pet", "hey", 2);
    expect(s.recentMessages(10).map((m) => [m.channel, m.text])).toEqual([["voice", "hi"], ["text", "hey"]]);
    expect(s.countMessages()).toBe(2);
  });
  it("returns only the most recent n messages", () => {
    const s = new PetStore(memSql());
    for (let i = 0; i < 5; i++) s.addMessage("text", "user", `m${i}`, i);
    expect(s.recentMessages(2).map((m) => m.text)).toEqual(["m3", "m4"]);
  });
  it("stores memories, settings with defaults, snapshot, networks, spend and token hash", () => {
    const s = new PetStore(memSql());
    s.addMemory("likes mango", 1);
    expect(s.memories()).toEqual(["likes mango"]);
    expect(s.settings().name).toBe("Charm");
    s.setSettings({ name: "Ember", dailyCapUsd: 1 });
    expect(s.settings()).toMatchObject({ name: "Ember", dailyCapUsd: 1, voice: "coral" });
    expect(s.snapshot()).toBeNull();
    s.setSnapshot({ mood: "happy", food: 90, energy: 80, joy: 95, hygiene: 100, trust: 70, sick: 0, life: "normal", asleep: false, ageDays: 2, growth: 0 }, 5);
    expect(s.snapshot()?.snapshot.mood).toBe("happy");
    s.setNetworks([{ ssid: "home", psk: "pw" }]);
    expect(s.networks()).toEqual([{ ssid: "home", psk: "pw" }]);
    s.addSpend("2026-10-12", 0.25); s.addSpend("2026-10-12", 0.5);
    expect(s.spend("2026-10-12")).toBeCloseTo(0.75);
    expect(s.tokenHash()).toBeNull();
    s.setTokenHash("abc"); expect(s.tokenHash()).toBe("abc");
  });
  it("can drop the oldest messages after compaction", () => {
    const s = new PetStore(memSql());
    for (let i = 0; i < 4; i++) s.addMessage("text", "user", `m${i}`, i);
    const old = s.oldestMessages(2);
    s.deleteMessagesThrough(old[1].id);
    expect(s.recentMessages(10).map((m) => m.text)).toEqual(["m2", "m3"]);
  });
});
