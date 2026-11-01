#pragma once
#include <cstdint>
#include <hw.h>

#if __has_include("secret.h")
#include "secret.h"
#endif

#define DEFAULT_WIFI_SSID "ANDROID AP"
#define DEFAULT_WIFI_PASS "tes12345"
#define DEFAULT_FTP_USER "pio"
#define DEFAULT_FTP_PASS "tes12345"
#ifndef FTP_USER
#define FTP_USER DEFAULT_FTP_USER
#endif
#ifndef FTP_PASSWORD
#define FTP_PASSWORD DEFAULT_FTP_PASS
#endif

// speaker
#define DEFAULT_SPEAKER_SAMPLE_RATE 16000              
#define DEFAULT_SPEAKER_BIT_DEPTH I2S_DATA_BIT_WIDTH_16BIT
#define DEFAULT_SPEAKER_CHANNELS I2S_SLOT_MODE_MONO    
// Linear digital gain: 0.0 = mute, 1.0 = unchanged, 2.0 = twice amplitude.
#ifndef DEFAULT_SPEAKER_VOLUME
#define DEFAULT_SPEAKER_VOLUME 1.2f
#endif
#ifndef DEFAULT_MIC_VOLUME
#define DEFAULT_MIC_VOLUME 28.0f
#endif

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#ifndef WIFI_SSID
#define WIFI_SSID DEFAULT_WIFI_SSID
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD DEFAULT_WIFI_PASS
#endif
#ifndef AI_BASE_URL
#define AI_BASE_URL "https://api.openai.com/v1"
#endif
#ifndef AI_ENDPOINT
#define AI_ENDPOINT "/responses"
#endif
#ifndef AI_MODEL
#define AI_MODEL "gpt-6-luna"
#endif
#ifndef AI_API_KEY
#define AI_API_KEY ""
#endif
#ifndef TTS_BASE_URL
#define TTS_BASE_URL AI_BASE_URL
#endif
#ifndef TTS_API_KEY
#define TTS_API_KEY AI_API_KEY
#endif
#ifndef TTS_MODEL
#define TTS_MODEL "gpt-4o-mini-tts"
#endif
#ifndef TTS_VOICE
#define TTS_VOICE "coral"
#endif

namespace partner::config {
constexpr char timezone[] = "WIB-7";  // POSIX timezone signs are reversed.
constexpr char ntpPrimary[] = "pool.ntp.org";
constexpr char ntpSecondary[] = "time.google.com";
constexpr uint8_t oledAddress = 0x3c;
constexpr uint32_t reconnectMs = 15000;
constexpr uint32_t networkPeriodMs = 250;
constexpr uint32_t displayPeriodMs = 33;
constexpr uint32_t requestTimeoutMs = 45000;
constexpr uint32_t ttsSampleRate = 24000;
constexpr uint32_t audioSampleRate = 16000;
constexpr uint32_t voiceCommandWindowMs = 6000;
// Random sampling is enabled only for OpenAI Responses with gpt-6-luna.
constexpr bool reminderRandomSampling = true;
constexpr float reminderTemperatureMin = 0.85f;
constexpr float reminderTemperatureMax = 1.25f;
static_assert(reminderTemperatureMin >= 0.0f && reminderTemperatureMax <= 2.0f &&
              reminderTemperatureMin <= reminderTemperatureMax, "Invalid reminder temperature range");
constexpr bool bootAudioCaptureEnabled = false;
constexpr float speakerVolume = DEFAULT_SPEAKER_VOLUME < 0 ? 0 : DEFAULT_SPEAKER_VOLUME;
constexpr float microphoneVolume = DEFAULT_MIC_VOLUME < 0 ? 0 : DEFAULT_MIC_VOLUME;
constexpr uint16_t greetingMinMicRms = 90;
constexpr uint32_t maxSpeechSeconds = 45;
constexpr uint32_t networkStackBytes = 8192;  // Includes local reminder HTTP/JSON handling.
constexpr uint32_t reminderStackBytes = 16384;
constexpr uint32_t displayStackBytes = 6144;
}  // namespace partner::config
