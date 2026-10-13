export interface RealtimeUsage {
  input_token_details: { text_tokens: number; audio_tokens: number; cached_tokens: number; cached_tokens_details?: { text_tokens: number; audio_tokens: number } };
  output_token_details: { text_tokens: number; audio_tokens: number };
}
// USD per 1M tokens, from the providers' published prices (2026-10).
const RT: Record<string, { textIn: number; textCached: number; textOut: number; audioIn: number; audioCached: number; audioOut: number }> = {
  "gpt-realtime-2.1-mini": { textIn: 0.6, textCached: 0.06, textOut: 2.4, audioIn: 10, audioCached: 0.3, audioOut: 20 },
  "gpt-realtime-2.1": { textIn: 4, textCached: 0.4, textOut: 24, audioIn: 32, audioCached: 0.4, audioOut: 64 },
};
const CLAUDE: Record<string, { in: number; cacheRead: number; out: number }> = {
  "claude-sonnet-5-5": { in: 2, cacheRead: 0.2, out: 10 },
  "claude-opus-5-5": { in: 4, cacheRead: 0.2, out: 20 },
  "claude-haiku-5-5": { in: 0.1, cacheRead: 0.01, out: 0.5 },   // prompts up to 100K tokens
};
const M = 1_000_000;
export function realtimeCost(model: string, u: RealtimeUsage): number {
  const p = RT[model] ?? RT["gpt-realtime-2.1"];   // unknown model: price conservatively high
  const cachedText = u.input_token_details.cached_tokens_details?.text_tokens ?? 0;
  const cachedAudio = u.input_token_details.cached_tokens_details?.audio_tokens ?? 0;
  return ((u.input_token_details.text_tokens - cachedText) * p.textIn + cachedText * p.textCached +
    (u.input_token_details.audio_tokens - cachedAudio) * p.audioIn + cachedAudio * p.audioCached +
    u.output_token_details.text_tokens * p.textOut + u.output_token_details.audio_tokens * p.audioOut) / M;
}
export function claudeCost(model: string, u: { input_tokens: number; output_tokens: number; cache_read_input_tokens?: number | null }): number {
  const p = CLAUDE[model] ?? CLAUDE["claude-opus-5-5"];
  return (u.input_tokens * p.in + (u.cache_read_input_tokens ?? 0) * p.cacheRead + u.output_tokens * p.out) / M;
}
export function dayKey(ms: number): string { return new Date(ms).toISOString().slice(0, 10); }
