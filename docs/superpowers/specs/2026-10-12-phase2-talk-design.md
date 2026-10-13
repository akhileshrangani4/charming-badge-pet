# Phase 2: Talk to the pet (design spec)

Date: 2026-10-12
Status: draft, awaiting review
Builds on: [Phase 1 spec](2026-10-12-phase1-pet-design.md) (the pet, which is shipped)

## Goal

You can talk to the Charming pet on the badge. Hold A, speak, let go, and it answers out loud in character. Its face shows when it is listening, thinking and speaking. You can also text it from a private Charming app. Voice and text share one memory and one personality, and that personality follows the pet's real state: a neglected pet is curt and a happy one is chatty. The AI can put cards on the badge (answers, QR codes) without covering the mascot.

**Out of scope:**
- **Phase 3:** building Charming apps, plus calendar, email and Slack tools.
- **Phase 4:** a wake word.
- **Guest or visitor chat:** only the owner can chat.
- **AI changing the pet's stats:** the Tamagotchi rules stay on the badge.

## Success criteria

1. **Voice:** hold A, speak, release, and the first reply audio plays within **1.5 s** on a good connection (target 1 s), in a consistent character voice.
2. **Faces:** listening, thinking and speaking are visibly distinct. The speaking mouth follows the loudness of the reply audio.
3. **Personality tracks state.** The same question gets a noticeably different tone at high trust and joy than at low trust or just after being won back.
4. **Shared memory.** A fact said by voice ("my favorite snack is mango") is used correctly in a later text chat, and the other way round.
5. **Cards.** `show_card` puts a title, body or QR code on the badge, and the same card appears in the chat app.
6. **Wi-Fi.** The badge joins whichever saved network is in range, including your phone hotspot. Networks can be added from the app. With no connection, voice fails gracefully and the pet keeps working.
7. **Cost.** Spend is tracked per day and a daily cap (default $2) is enforced.
8. **Secrets.** API keys never reach the badge or the browser. The badge's token can be revoked.

## Architecture

```
Badge ──wss (device token)──► Cloudflare Worker ──► Durable Object "Pet" (one per badge)
  │  hold A: 24 kHz PCM up           │   ├─► OpenAI Realtime  gpt-realtime-2.1-mini   (the voice)
  │  ◄── 24 kHz PCM, faces, cards     │   │      tool think(question) ─► Anthropic Claude Sonnet 5.5 (the mind)
  │                                   │   ├─► the DO's own SQLite: messages, memories, snapshot, networks, spend
  │                                   │   └─► Charming webhook: live updates to the chat app
Charming app (private) ──HTTPS + shared secret──┘  chat, state, settings
```

- **Hosting:** Cloudflare Workers (Paid plan) with a Durable Object that uses its built-in SQLite. Deployed with `wrangler`. The Worker holds `OPENAI_API_KEY`, `ANTHROPIC_API_KEY`, `APP_SHARED_SECRET`, `DEVICE_TOKEN_HASH`, `CHARMING_WEBHOOK_URL` and `CHARMING_WEBHOOK_SECRET` as secrets.
- **Mouth:**
  - Model `gpt-realtime-2.1-mini`, which is the faster and cheaper option. Upgrading to `gpt-realtime-2.1` is a one-line configuration change.
  - The voice is chosen once at setup (default `coral`) and stays the same, because Realtime sessions can't change voice once audio has started.
  - Turn detection is off: push-to-talk decides when a turn ends.
- **Mind:**
  - Claude Sonnet 5.5 (`claude-sonnet-5-5`), reached through the `think` tool.
  - Text chat calls Claude directly.
- **Charming:** a private app on the owner's Pro plan. It receives live updates through one Charming webhook and calls the Worker through `env.fetch`.

## Badge firmware

**New modules under `firmware/badge-pet/src/`:**

| Module | Responsibility |
|---|---|
| `net/WifiManager` | Keeps up to 5 saved networks in NVS (namespace `net`). Scans and joins the strongest known one. Wi-Fi is on only while the screen is on. |
| `net/BrainLink` | The WSS client: authenticate, reconnect with exponential backoff (1 s, 2 s, ... capped at 30 s), and route JSON and binary frames. |
| `net/Protocol` | Pure C++ encoding and decoding of the JSON messages (host-tested). |
| `audio/Voice` | Records the mic at 24 kHz, 16-bit mono, in 40 ms chunks. Plays back through a jitter buffer of about 120 ms. Measures the loudness of what's playing so the mouth can follow it. |

**Push-to-talk:**
- `ButtonArbiter` gains `TalkStart`, fired after A has been held for 400 ms with B up, and `TalkEnd`, fired when A is released after a TalkStart.
- A quick click of A still opens the menu.
- If B is pressed during a talk, the talk is cancelled (the mic stops and a `talk_cancel` message is sent). The existing both-buttons pause then applies.
- On the Gone screen, holding A still calls the pet back, and talking is disabled.
- A turn is sent automatically after 30 s.

**New expressions:**

| Expression | Look |
|---|---|
| `Listening` | Large round eyes, the body leaning slightly forward |
| `Thinking` | Eyes looking up, three drifting dots |
| `Speaking` | Mouth opening driven by the loudness of the reply audio |

The mood still shapes them: at trust below 30 the listening eyes are half-lidded.

**Cards:** a `card` message maps onto the existing `display::Content` and message screen, with the mascot moved aside as in Phase 1. Its strings are copied into fixed buffers (title up to 23 characters, body up to 63, QR up to 200).

**Snapshot:** sent on every connect and on every mood change. It contains the mood, food, energy, joy, hygiene, trust, sick, life, asleep, age in days and growth.

**Offline:**
- Holding A with no Wi-Fi shows "NO WIFI".
- Wi-Fi up but the server unreachable shows "CAN'T REACH ME".
- A refused token shows "RE-PAIR ME".

**Serial commands:**

| Command | Purpose |
|---|---|
| `W <ssid> <password>` | Add the first network |
| `D <token>` | Set the device token |
| `N` | List saved networks |

**Power:**
- Wi-Fi and the socket are shut down before deep sleep. Timer wakes stay offline.
- The radio adds draw while the screen is on. The device checklist measures it.

## Wire protocol

The badge and the Durable Object talk over a WebSocket at `wss://<worker>/badge`, with the header `Authorization: Bearer <device token>`.

**Binary frames:**
- 16-bit little-endian PCM, 24 kHz, mono.
- Badge to server: mic audio, only between `talk_start` and `talk_end`.
- Server to badge: reply audio.

**JSON frames:**

| Direction | Message |
|---|---|
| badge → server | `{"t":"hello","fw":"2.0.0","snapshot":{...}}` |
| badge → server | `{"t":"snapshot", ...}` |
| badge → server | `{"t":"talk_start"}`, `{"t":"talk_end"}`, `{"t":"talk_cancel"}` |
| server → badge | `{"t":"face","f":"listening" \| "thinking" \| "speaking" \| "idle"}` |
| server → badge | `{"t":"card","title":"...","body":"...","qr":"..."}` |
| server → badge | `{"t":"say_end"}` (reply audio is finished) |
| server → badge | `{"t":"error","code":"over_budget" \| "brain_fog" \| "busy"}` |
| server → badge | `{"t":"networks","list":[{"ssid":"...","psk":"..."}]}` |

The connection is pinged every 20 s and the badge treats 45 s of silence as a dead link.

## Durable Object "Pet"

**SQLite tables:**

| Table | Columns |
|---|---|
| `messages` | `id`, `ts`, `channel` (voice or text), `role` (user or pet), `text` |
| `memories` | `id`, `ts`, `fact` |
| `snapshot` | a single row: `json`, `ts` |
| `networks` | `ssid`, `psk`, `ts` |
| `spend` | `day`, `usd` |
| `settings` | `name` (default "Charm"), `voice`, `daily_cap_usd`, `realtime_model` |

**Voice turn:**
1. `talk_start`:
   - If no realtime session is open, open one. The Durable Object makes an outbound WebSocket to `wss://api.openai.com/v1/realtime?model=<model>` and sends `session.update` with: the instructions; output audio and PCM at 24 kHz both ways; turn detection off; input transcription on; and the `think` tool.
   - Send `face: listening` to the badge.
2. Binary audio from the badge becomes `input_audio_buffer.append` (base64).
3. `talk_end`: send `input_audio_buffer.commit` and `response.create`.
4. Reply audio deltas are decoded and sent to the badge as binary frames, along with `face: speaking`.
5. `response.done` sends `say_end`. Both sides' transcripts go into `messages` and are pushed to Charming.
6. A function call to `think` sends `face: thinking` and calls Claude. The result goes back as `function_call_output`, followed by `response.create`.
7. `talk_cancel`: clear the input audio buffer and cancel any in-progress response.
8. **Idle:** the upstream session closes after 30 s with no turn. The badge socket uses hibernation, so the object costs nothing while idle.
9. Only one turn runs at a time. A `talk_start` during a reply first cancels that reply, so you can interrupt by talking.

**The `think` tool:** `think({question: string})`, described to the realtime model as: "Ask your deeper mind for anything that needs memory, facts, planning, or showing something on the badge." Before calling it, the realtime model is told to say a short filler line in character.

**Claude call** (used by `think` and by text chat):
- Messages API, `claude-sonnet-5-5`, `max_tokens` 600, an 8 s timeout.
- **Tools available to Claude:**
  - `remember({fact})`: inserts into `memories`.
  - `show_card({title, body?, qr?})`: sends the card to the badge, if connected, and pushes it to Charming.
- **Context:** the system prompt (below), the last 30 messages from both channels, and every memory.

**System prompt:** built per call from `settings` and `snapshot`.
- **Identity:** "You are {name}, a small hand-drawn flame who lives on Avi's badge."
- **Live state, in words:** for example "you're hungry", "you were just won back after running away and don't trust Avi yet", "it's late at night".
- **Tone rules tied to trust and joy:**
  - trust below 30, or just won back: short and guarded;
  - joy above 70: playful;
  - sick: weak.
- **Voice rules:** one or two short sentences, never lists, never read out URLs. Use `show_card` for anything visual.

**Budget:**
- Tracked from the reported usage. Realtime usage is converted from token counts at configured per-token prices; Claude's usage the same way.
- Over the daily cap: voice replies "too sleepy to talk, try tomorrow" (a fixed clip played without calling a model), and text returns `over_budget`.

**Memory compaction:** a Charming routine runs hourly. When `messages` grows past 500, it asks Claude to summarize the oldest 400 into memories, then deletes them.

## Worker routes

| Route | Auth | Purpose |
|---|---|---|
| `GET /badge` (upgrade) | device token, checked as SHA-256 against `DEVICE_TOKEN_HASH` | The badge socket, routed to `Pet("main")` |
| `POST /chat` `{text}` | `X-App-Secret` header plus an owner id check | Text turn, returns `{reply, card?}` |
| `GET /state` | same | Snapshot, online flag, last seen, today's spend, settings, networks (passwords masked) |
| `PUT /settings` `{name?, voice?, daily_cap_usd?}` | same | Update settings |
| `PUT /networks` `{list}` | same | Replace the saved networks, then push them to the badge if connected |
| `POST /device-token/rotate` | same | Mint a new token and return it once; the old one is revoked immediately |
| `POST /compact` | same | Memory compaction, called by the hourly Charming routine |

## Charming app (private, Pro)

- **Backend operations:** `chat`, `state`, `setSettings`, `setNetworks`, `rotateToken` and `compact` proxy to the Worker through `env.fetch`, with the shared secret sent as `{{secret:PET_SECRET}}`. `ingest` receives webhook pushes and writes them to storage.
  - Each operation checks the caller's user id against the owner id.
  - Errors are returned as `{error}` data, not thrown.
- **UI:**
  - **Chat:** a single thread with voice turns labeled by a mic glyph. Cards render inline, and QR codes render as images.
  - **Header:** online dot, mood, the four stat bars and last seen.
  - **Settings:** name, voice (fixed after first use; changing it starts a new session), daily cap and today's spend, Wi-Fi list, token rotation with a one-time display plus the `D <token>` command to paste into serial.
- **Look:** black background, Silkscreen headings, Inter body text, Geist Mono labels. Laid out for a phone first.
- **Live updates:** the Worker posts message, snapshot and card events to the app's webhook. `ingest` stores them and the open app refreshes through Charming's live-state stream.
- **Routine:** `compact`, hourly.

## Errors and edge cases

| Failure | Behavior |
|---|---|
| No Wi-Fi, or the server is unreachable | A badge message; the pet is unaffected |
| Socket drops mid-reply | Audio stops and the face returns to the mood. The badge reconnects; the partial transcript is kept. |
| Realtime error, or no audio within 10 s of `talk_end` | Error `brain_fog`; the badge says "brain fog..." as a card |
| Claude takes longer than 8 s, or errors | `think` returns "couldn't think of it" and the realtime model answers in character |
| Over budget | Error `over_budget`, a fixed clip, and a text error |
| Bad device token | 401, and the badge shows "RE-PAIR ME" once per boot |
| A second badge connects | The new socket replaces the old one |
| Talk held longer than 30 s | The badge sends the turn automatically |
| Speaking while the pet is replying | The reply is cancelled and the new turn starts |
| Pet is asleep | The pet answers sleepily |
| Pet is gone | No talking |

## Testing

- **Worker** (Vitest with `@cloudflare/vitest-pool-workers`):
  - A fake realtime upstream checks that a voice turn produces the right message order (append, commit, create, audio out, `say_end`).
  - A `think` round trip, with `show_card` reaching the badge socket and the webhook.
  - Memory shared between voice and text.
  - The spend cap.
  - Auth: wrong device token, wrong secret, wrong owner.
  - The prompt reflecting state (trust below 30 gives the guarded rule).
  - Token rotation.
- **Firmware host tests:** `Protocol` encode and decode; push-to-talk timing in the button arbiter (talk start at 400 ms, click unchanged, B cancels the talk, the Gone screen disables talking).
- **Charming app:** a browser check covering chat, reply, card render, adding a network, token rotation and a live voice transcript appearing.
- **On the device:**
  - mic level and clarity;
  - playback quality;
  - first audio within 1.5 s;
  - switching to the hotspot;
  - the offline messages;
  - battery draw with Wi-Fi on.
- **Live smoke test:** one real conversation, checking that the logged spend matches the providers' dashboards to within 10%.

## Setup the owner provides

The keys are entered privately, never pasted into chat:
- a Cloudflare account and a `wrangler login`;
- an OpenAI API key;
- an Anthropic API key;
- the first Wi-Fi network over serial.
