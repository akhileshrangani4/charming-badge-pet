export type Face = "idle" | "listening" | "thinking" | "speaking";
export interface Snapshot {
  mood: string; food: number; energy: number; joy: number; hygiene: number; trust: number;
  sick: number; life: "normal" | "gone" | "returning"; asleep: boolean; ageDays: number; growth: number;
  hour?: number;   // the badge's local hour (0-23) from its RTC; Workers run in UTC
  battery?: number; charging?: boolean;
}
export interface Settings { name: string; voice: string; dailyCapUsd: number; realtimeModel: string }
export interface Message { id: number; ts: number; channel: "voice" | "text"; role: "user" | "pet"; text: string }
export interface Card { title: string; body?: string; qr?: string }
export interface NetworkEntry { ssid: string; psk: string }
export const DEFAULT_SETTINGS: Settings = { name: "Charm", voice: "coral", dailyCapUsd: 2, realtimeModel: "gpt-realtime-2.1" };
