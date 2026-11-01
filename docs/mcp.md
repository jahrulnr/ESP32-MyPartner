# Partner MCP

Partner exposes an MCP Streamable HTTP endpoint on port 83:

```text
http://<PARTNER_IP>:83/mcp
```

The IP is printed in the serial log as `[mcp] ready`. The endpoint is
**session-based**: `initialize` mints a session id, every later request must
carry it, and a client that skips the handshake receives `404 Unknown session`.
Server-initiated messages travel over a per-session SSE stream opened with
`GET /mcp`; POST replies are always plain JSON.

A client may negotiate protocol versions `2025-06-18`, `2025-03-26`, and
`2024-11-05`. It may start by calling `contract()` to read the operating guide,
and `contract(reference="screens"|"sensors"|"speech"|"reminders")` for details.

## Sessions

| Rule | Behaviour |
|---|---|
| Handshake | `initialize` needs no session header and returns `Mcp-Session-Id` in the response headers |
| Required afterwards | Every `POST /mcp` must send `Mcp-Session-Id`; missing, wrong, or stale returns `404 Unknown session` |
| Capacity | The table holds **4 sessions**. When all four are occupied, `initialize` returns `503 Session capacity reached` |
| Idle reclaim | A slot is freed only when its SSE stream is **not** connected and it has been idle for **5 minutes**. Every POST refreshes the idle timer, so an actively used session never expires |
| Explicit close | `DELETE /mcp` with `Mcp-Session-Id` frees the slot immediately and returns `200`; an unknown id returns `404` |
| Reboot | Every session id is invalidated by a device reboot |
| Failed handshake | An `initialize` whose reply carries no JSON-RPC `result` object (bad params, rejected notification) sends **no** session header and leaves no session behind |
| Notifications | A request without an `id` is answered `202` with an empty body and does not create or carry a session |

Capacity is small and reclaim is deliberately slow, so a client that opens a new
session per request will saturate the device and keep it saturated for five
minutes at a time. Treat the session id as a connection that is worth reusing.

### Recommended client behaviour

1. Call `initialize` once, then store the returned `Mcp-Session-Id`.
2. Send that id on every subsequent request; reuse it across tool calls.
3. On `404 Unknown session`, re-initialize **once** and retry the request.
4. Never retry an `initialize` that returned `503`. Free a slot with
   `DELETE /mcp`, or wait for the five-minute idle reclaim.
5. Send `DELETE /mcp` when the client shuts down.
6. Persist the id across restarts when possible, and expect it to be invalid
   after any reboot.

## Transport rules

- `MCP-Protocol-Version`, when present, must be one of the supported versions;
  otherwise the server answers `400`.
- Request bodies are limited to **4096 bytes**; larger bodies get `413`.
- If an `Origin` header is present it must be exactly
  `http://<device-ip>:83`; anything else gets `403`.
- `GET /mcp` opens the SSE stream for the session. It requires
  `Accept: text/event-stream` (`406` otherwise) and a valid session (`404`), and
  returns `409 Stream already open` if a stream is already attached. The server
  emits a `: keepalive` comment every 15 seconds.

## Tools

| Tool | Description |
|---|---|
| `contract(reference?)` | Read the core guide or a detailed reference: `core`, `screens`, `sensors`, `speech`, `reminders`. |
| `menu_list()` | List screens and the selected screen. |
| `menu(screen)` | Select `home`, `clock`, or `sensors`. |
| `info()` | Wi-Fi, time, speaker, microphone, selected node, and all tracked nodes with sensor freshness. |
| `speak(text)` | Queue 1–255 Unicode code points for speech; longer text is rejected. |
| `expression_list()` | List expressions and the current setting. |
| `expression(choose)` | Select a face; `auto` restores automatic behavior. |
| `logs()` | Read the last 8192 bytes of application and ESP-IDF logs retained since boot. |
| `reminder_list()` | List persistent reminders and delivery status. |
| `reminder_create(text, due_epoch \| after_seconds)` | Create a reminder. Supply exactly one time field. |
| `reminder_delete(id)` | Delete a reminder that is not currently being delivered. |

`speak` returns `queued`; audio plays asynchronously on the existing speech task.
The queue holds four requests, is not persistent across reboot, and shares the
speaker with reminders and the local greeting. Check `logs` for
`[mcp] speech complete` or `[mcp] speech failed`. Each menu or expression setting
is visible in `info`. The button can also change menus. A manually selected
expression remains active until replaced with `auto` or the device reboots. An
absent-person radar report may keep the OLED off while menu and expression
changes are stored.

## Reminders

- Supply exactly one of `after_seconds` or `due_epoch` (Unix seconds); relative
  times need a synced clock.
- Up to eight reminders, 1–160 UTF-8 bytes each. They persist in
  `/partner-reminders.json` on LittleFS and reload at boot; tools return an error
  if storage is unavailable.
- A due reminder waits for Wi-Fi, a ready speaker, and a valid radar report
  showing presence. Missing or invalid radar data counts as unknown, so it keeps
  waiting. Radar detects presence, not identity.
- After successful speech the item is deleted; a TTS error retries it.
  `reminder_delete` refuses an item that is being delivered.
- mmWave and DHT data expire after five minutes without updates. A node is
  offline after 45 seconds without packets, and its data is then ignored even if
  fresh. Radar reports with zero frames are invalid. The Linux ESP-NOW server
  uses the same limits; restart it after changing them.

### Room behavior

Clock reminders may include temperature and humidity while node data is valid.
A valid radar transition from empty to present shows a happy face and a short
greeting (needs Wi-Fi and TTS); startup or lost radar data does not count as
leaving. The face also reacts to sound level, blinks, and looks around.

## Resources

Two resources are published, both `application/json`:

```text
partner://sensors/presence
partner://sensors/climate
```

`resources/read` returns the current value. `resources/subscribe` and
`resources/unsubscribe` are per session and deliver
`notifications/resources/updated` over that session's SSE stream, so a client
that wants change events must open `GET /mcp` and keep it alive.

## Examples with a standard MCP client

```text
contract()
contract(reference="sensors")
menu(screen="sensors")
expression(choose="happy")
speak(text="Hi, I'm Partner!")
expression(choose="auto")
reminder_create(text="Time to drink water.", after_seconds=600)
reminder_create(text="Take a short break.", due_epoch=1793588400)
reminder_list()
reminder_delete(id=3)
```

The log buffer contains the latest retained bytes; older entries are overwritten.
It captures project log calls and ESP-IDF logs, but does not promise output
written directly by third-party libraries or the boot ROM. The filesystem and FTP
are not changed by the MCP service.

## Streaming and memory

Cloud TTS already streams PCM from the HTTP response into the I2S speaker,
resampling as it arrives. It does not save a complete audio file. The I2S DMA
buffers provide limited buffering; a sufficiently long network stall can still
interrupt playback. The MCP speech queue and retained logs prefer PSRAM to leave
internal RAM available for TLS.

Hourly LLM reminders currently wait for the full Responses API result before
starting TTS. The existing transport prefers PSRAM for its response buffer, but
still accumulates the whole body even if a streaming callback is supplied. A
future incremental implementation needs to parse Responses SSE events without
retaining the complete stream, and coordinate text chunks with TTS. Streaming
alone does not eliminate TLS memory or guarantee lower CPU usage. No LittleFS
audio spool is used by this path.
