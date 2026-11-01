#pragma once

namespace partner::contract {
inline constexpr char core[] = R"contract(# Partner operating contract
Partner is an ESP32-S3 desk companion with an OLED, microphone, speaker, Wi-Fi, and ESP-NOW sensor nodes.
Call menu_list and expression_list for current supported options. Use info for device and sensor status; a node marked offline or sensor marked unavailable does not establish presence or absence. Use reminder_list, reminder_create and reminder_delete to manage persistent personal reminders.
Use menu to select a screen, expression to set the face or restore automatic expressions, and speak to queue speech of 1 to 255 Unicode characters. A queued speech result is an acceptance, not proof the speaker completed playback; consult logs for completion or failure.
The OLED can be powered off automatically when a valid connected radar reports nobody present; menu and expression changes remain selected for when the display wakes.
Use logs to inspect retained application and ESP-IDF logs from this boot. They are bounded and may no longer include older entries.
Detailed references: contract(reference="screens"), contract(reference="sensors"), contract(reference="speech"), contract(reference="reminders").)contract";
inline constexpr char screens[] = R"contract(# Screens and expressions
menu_list lists available screens. menu(screen="home"|"clock"|"sensors") selects a screen; the physical button can also change it. info reports the current selected screen.
expression_list lists available expressions. expression(choose="normal"|"happy"|"sad"|"worried"|"focused"|"surprised"|"angry"|"sleepy") overrides automatic expressions. expression(choose="auto") returns to automatic behavior. The override persists until changed or rebooted.
Screen power is managed by presence separately from menu selection.)contract";
inline constexpr char sensors[] = R"contract(# Sensors
info returns the selected node and all tracked nodes, including their online state and climate/presence freshness. Temperature, humidity, detection, and distance appear when their data is valid. A node can be offline while its last sensor data is still fresh. Offline does not mean nobody is present. Sensor TTL is five minutes; node-online TTL is 45 seconds. Radar reports presence and approximate range, not identity or sleep state.
MCP clients can subscribe to partner://sensors/presence and partner://sensors/climate. On an update read the resource again. Events are not replayed after disconnect; read current state after reconnect.)contract";
inline constexpr char speech[] = R"contract(# Speech and logs
speak(text="...") accepts 1 to 255 Unicode characters and puts a text-to-speech job into a bounded queue. Longer text is rejected, not truncated. It returns accepted/queued status; audio generation requires network and the configured TTS service. Speech shares the speaker with reminders and local greeting. Use logs for actual completion or failure. Text is passed as provided; the service does not translate it.)contract";
inline constexpr char reminders[] = R"contract(# Personal reminders
reminder_list() lists persistent reminders. reminder_create(text="...", after_seconds=600) creates one relative to synchronized time; alternatively specify due_epoch as Unix seconds. Provide exactly one time field. Text must be 1 to 160 UTF-8 bytes; at most eight reminders are stored. reminder_delete(id=...) deletes a reminder unless it is being delivered. A due reminder waits for valid radar presence, Wi-Fi and audio, and is removed after successful speech. Storage errors are reported and changes rolled back.)contract";
}  // namespace partner::contract
