#pragma once

#include <ArduinoJson.h>
#include <cstdio>
#include <string>

#include "domain/Reminder.h"
#include "domain/ReminderVariation.h"
#include "domain/SensorRegistry.h"

namespace partner {

class ResponsesProtocol {
 public:
  static void build(JsonDocument& payload, const LocalTime& now, const char* model,
                    const ReminderVariation& variation, ReminderVariation::Choice choice,
                    bool randomSampling, const SensorSnapshot& sensor = {}) {
    char context[128];
    snprintf(context, sizeof(context),
             "Waktu saat ini %04d-%02d-%02d %02d:%02d:%02d WIB (UTC+7). Buat pengingat jam ini.",
             now.year, now.month, now.day, now.hour, now.minute, now.second);
    payload["model"] = model;
    // Prompt v2: direction and recent outputs vary, while the clock supplies time.
    payload["instructions"] =
        "Kamu Partner, teman pengingat waktu yang akrab dan santai. Jawab dalam bahasa Indonesia, "
        "maksimal dua kalimat pendek dan sekitar 30 kata, teks biasa siap dibacakan. "
        "Sebutkan jam dan menit WIB sesuai waktu yang diberikan secara alami, "
        "tuliskan waktunya dengan kata-kata agar nyaman dibacakan. "
        "Gunakan arah pesan yang diminta sebagai inspirasi, sesuaikan dengan waktu; "
        "ajakan bersifat opsional. Variasikan susunan dan kata, hindari pola atau pesan "
        "yang mirip dengan pesan terakhir. Jangan mengarang kegiatan, jadwal, cuaca, "
        "atau keadaan pengguna yang tidak diketahui. Jangan jelaskan instruksi.";
    std::string input(context);
    if (sensor.online && sensor.hasClimate) {
      char climate[112];
      const int temperature = sensor.temperature10;
      const unsigned magnitude = temperature < 0 ? -temperature : temperature;
      snprintf(climate, sizeof(climate), "\nSensor ruangan: suhu %s%u.%u C, kelembapan %u.%u persen. Boleh gunakan bila sesuai.",
               temperature < 0 ? "-" : "", magnitude / 10, magnitude % 10,
               sensor.humidity10 / 10, sensor.humidity10 % 10);
      input += climate;
    }
    input += "\nArah pesan: ";
    input += ReminderVariation::direction(choice.angle);
    bool first = true;
    for (const auto& message : variation.recent()) {
      if (message.empty()) continue;
      input += first ? "\nPesan terakhir, jangan diulangi:\n- " : "\n- ";
      input += message;
      first = false;
    }
    payload["input"] = input;
    if (randomSampling) {
      payload["reasoning"]["effort"] = "none";
      payload["temperature"] = choice.temperature;
    }
    payload["max_output_tokens"] = 1024;
    payload["stream"] = false;
  }

  static bool readText(JsonVariantConst response, std::string& text, std::string& error) {
    text.clear();
    if (std::string(response["status"] | "") != "completed") {
      error = "LLM response was not completed";
      return false;
    }
    for (JsonObjectConst item : response["output"].as<JsonArrayConst>()) {
      if (std::string(item["type"] | "") != "message") continue;
      for (JsonObjectConst part : item["content"].as<JsonArrayConst>()) {
        if (std::string(part["type"] | "") != "output_text") continue;
        if (!text.empty()) text += ' ';
        text += part["text"] | "";
      }
    }
    if (text.find_first_not_of(" \n\r\t") == std::string::npos) {
      text.clear();
      error = "LLM response has no speech text";
      return false;
    }
    return true;
  }
};

}  // namespace partner
