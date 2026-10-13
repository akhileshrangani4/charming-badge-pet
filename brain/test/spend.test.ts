import { describe, expect, it } from "vitest";
import { claudeCost, dayKey, realtimeCost } from "../src/spend";

describe("spend", () => {
  it("prices realtime mini audio and text tokens", () => {
    const usd = realtimeCost("gpt-realtime-2.1-mini", {
      input_token_details: { text_tokens: 1_000_000, audio_tokens: 1_000_000, cached_tokens: 0 },
      output_token_details: { text_tokens: 0, audio_tokens: 1_000_000 },
    });
    expect(usd).toBeCloseTo(0.6 + 10 + 20);
  });
  it("prices cached realtime input at the cached rate", () => {
    const usd = realtimeCost("gpt-realtime-2.1-mini", {
      input_token_details: { text_tokens: 1_000_000, audio_tokens: 0, cached_tokens: 1_000_000, cached_tokens_details: { text_tokens: 1_000_000, audio_tokens: 0 } },
      output_token_details: { text_tokens: 0, audio_tokens: 0 },
    });
    expect(usd).toBeCloseTo(0.06);
  });
  it("prices Claude Sonnet 5.5", () => {
    expect(claudeCost("claude-sonnet-5-5", { input_tokens: 1_000_000, output_tokens: 1_000_000 })).toBeCloseTo(12);
  });
  it("buckets spend by UTC day", () => {
    expect(dayKey(Date.UTC(2026, 9, 12, 23, 59))).toBe("2026-10-12");
  });
});
