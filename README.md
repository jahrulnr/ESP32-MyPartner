<div align="center">

<h1>ESP32-MyPartner</h1>
<p><strong>A little presence on your desk.</strong></p>
<p>An ESP32-S3 desk companion that responds to your voice, shows expressions,<br>
reminds you of important things, and senses the room around it.</p>
<p><code>ESP32-S3</code> · <code>FreeRTOS</code> · <code>ESP-NOW</code> · <code>MCP</code> · <code>Voice</code></p>

</div>

MyPartner is a physical companion designed to stay nearby: a small face on a
screen, a voice that greets you, and reminders that wait until someone is in the
room. Greetings are recognized locally. Hourly reminder messages and free-form
speech use an LLM and a TTS service over Wi-Fi.

> The repository is named **ESP32-MyPartner** and the device is called
> **MyPartner**. The firmware and voice commands still refer to it as
> **Partner**. Say **“Hi Partner”** or **“Hey Partner”** to hear **“Hi”**.

## A look at MyPartner

These are rendered previews captured from the OLED framebuffer before flashing,
not photographs. The display renders at 128 × 64 pixels.

<table>
  <tr>
    <td align="center"><img src="docs/images/oled-idle.png" alt="OLED face in its idle state" width="384"><br><sub>Idle face</sub></td>
    <td align="center"><img src="docs/images/oled-mcp-happy.png" alt="OLED face showing a happy expression" width="384"><br><sub>Happy expression</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="docs/images/oled-speaking.png" alt="OLED face while speaking" width="384"><br><sub>Speaking</sub></td>
    <td align="center"><img src="docs/images/oled-clock.png" alt="OLED clock screen" width="384"><br><sub>Clock</sub></td>
  </tr>
</table>

## What it can do

- **Respond locally** — AFE and MultiNet recognize greetings; the “Hi” reply
  uses picoTTS audio prepared on the device.
- **Announce the hour** — at each hour change in Western Indonesian Time
  (WIB), Partner generates a message with an LLM and reads it aloud using TTS.
- **Remember personal reminders** — create and manage reminders through MCP.
  Due reminders wait for a valid presence detection before being spoken.
- **Show expressions** — the OLED face blinks and reacts to its surroundings;
  screens and expressions can also be selected through MCP.
- **Sense the room** — receive temperature, humidity, and radar data from a
  compatible ESP-NOW sensor node. Radar reports presence and approximate
  distance, not identity.
- **Connect to AI clients** — MCP exposes tools, device information, and sensor
  resource change notifications through SSE.

```mermaid
flowchart LR
    Mic[I2S microphone] --> Voice[AFE + MultiNet]
    Voice --> Greeting[Local Hi response]
    Node[ESP-NOW sensor node] --> Partner[MyPartner · ESP32-S3]
    Partner --> OLED[OLED display]
    Partner --> Speaker[I2S speaker]
    Greeting --> Speaker
    Partner <--> Cloud[LLM + TTS over Wi-Fi]
    Client[MCP client / automation] <--> Partner
```

## Hardware

The current configuration uses an ESP32-S3 DevKitC-1 N16R8, a 128 × 64 I2C OLED,
an INMP441 I2S microphone, a MAX98357A amplifier, and a speaker. An ESP-NOW
node with a DHT sensor and presence radar is optional. Adjust
[`include/hw.h`](include/hw.h) to match your wiring.

| Signal | GPIO |
|---|---:|
| OLED SDA / SCL | 4 / 5 |
| BOOT button | 0 |
| Microphone SD | 1 |
| Speaker DIN | 2 |
| Shared I2S BCLK | 42 |
| Shared I2S WS/LRC | 41 |

## Getting started

### 1. Prepare the project

The project depends on two Git submodules: `lib/AiProviderKit` and
`model/esp_sr` (ESP-SR). A plain `git clone` leaves them empty, so clone with
submodules:

```sh
git clone --recurse-submodules https://github.com/jahrulnr/ESP32-MyPartner.git
cd ESP32-MyPartner
```

If you already cloned without them, fetch them afterwards:

```sh
git submodule update --init --recursive
```

After pulling changes that move a submodule, run the same command again to
sync it.

Install [PlatformIO](https://platformio.org/) and connect the board over USB.
The project uses the `esp32-s3-devkitc1-n16r8` environment. Make sure the
ESP-SR source and picoTTS model assets are available in `model/`.

Copy `include/secret.h.example` to `include/secret.h`, then configure Wi-Fi
and the API settings. `secret.h` is listed in `.gitignore`.

### 2. Upload the firmware

```sh
pio run -e esp32-s3-devkitc1-n16r8
pio run -e esp32-s3-devkitc1-n16r8 -t upload --upload-port /dev/ttyACM0
```

### 3. Flash the voice models

For a new board or a model change, flash the voice model partitions separately.
This command writes the model partition and both picoTTS voice partitions:

```sh
python3 tools/upload_voice_models.py --port /dev/ttyACM0 --with-tts
```

Select MultiNet 5, 6, or 7 in `model/voice_model.json` before running the
command. Use `--check --with-tts` to validate the assets and partition sizes
without flashing; the script still generates the packed model file.

Check the serial monitor for the IP address and service status. Adjust digital
microphone and speaker gain with `DEFAULT_MIC_VOLUME` and
`DEFAULT_SPEAKER_VOLUME` in [`include/app_config.h`](include/app_config.h).

## Connect with MCP

Connect an MCP client to `http://<PARTNER_IP>:83/mcp`, then start with
`contract()`. Example tools:

```text
info()
menu(screen="sensors")
expression(choose="happy")
speak(text="Hello, welcome back!")
reminder_create(text="Time to take a break.", after_seconds=600)
reminder_list()
```

Subscribe to `partner://sensors/presence` or `partner://sensors/climate` to
receive change notifications. The MCP client or automation application decides
what to do with each notification.

- [MCP, reminders, and sensor event guide](docs/mcp.md)

## Project status

MyPartner is an evolving experimental project. Local greetings, the display,
sensor nodes, reminders, and MCP are available; recognition quality and
connection reliability depend on the environment and hardware. TTS audio
streams to the speaker, while hourly LLM messages are currently collected in
full before TTS begins.

Run the available host checks from the workspace:

```sh
bash tools/test_host.sh
```
