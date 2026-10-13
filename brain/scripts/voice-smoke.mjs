// Plays a recorded question to the brain as if it were the badge and saves the spoken reply.
// Usage: node scripts/voice-smoke.mjs <wav 24k mono s16le> <out.raw>
import { readFileSync, writeFileSync } from "node:fs";
const url = readFileSync("DEPLOYED_URL", "utf8").trim().replace("https://", "wss://") + "/badge";
const token = readFileSync("../backups/device-token", "utf8").trim();
const wav = readFileSync(process.argv[2]);
// Find the WAV "data" chunk instead of assuming a 44-byte header (afconvert may add chunks).
const dataAt = wav.indexOf(Buffer.from("data"));
const pcm = wav.subarray(dataAt + 8, dataAt + 8 + wav.readUInt32LE(dataAt + 4));
const ws = new WebSocket(url, { headers: { Authorization: `Bearer ${token}` } });
const out = []; const t0 = Date.now(); let first = 0;
ws.binaryType = "arraybuffer";
ws.onopen = () => {
  ws.send(JSON.stringify({ t: "hello", fw: "smoke", snapshot: { mood: "happy", food: 90, energy: 80, joy: 90, hygiene: 90, trust: 80, sick: 0, life: "normal", asleep: false, ageDays: 1, growth: 0 } }));
  ws.send(JSON.stringify({ t: "talk_start" }));
  for (let i = 0; i < pcm.length; i += 1920) ws.send(pcm.subarray(i, i + 1920));
  setTimeout(() => { ws.send(JSON.stringify({ t: "talk_end" })); console.log("sent", pcm.length, "bytes"); }, 300);
};
ws.onmessage = (e) => {
  if (typeof e.data === "string") { console.log(Date.now() - t0, "ms", e.data); if (JSON.parse(e.data).t === "say_end") { writeFileSync(process.argv[3], Buffer.concat(out)); console.log("first audio after", first, "ms"); ws.close(); } }
  else { if (!first) first = Date.now() - t0; out.push(Buffer.from(e.data)); }
};
ws.onclose = (e) => console.log("closed", e.code);
