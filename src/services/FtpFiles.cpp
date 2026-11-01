#include "FtpFiles.h"
#include "services/PartnerLog.h"
#include <WiFi.h>
#include <algorithm>
#include <app_config.h>
#include <esp_partition.h>

namespace partner {
namespace {
bool filesystemIsBlank() {
  const auto* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
  if (!partition) return false;
  uint8_t buffer[4096];
  for (size_t offset = 0; offset < partition->size; offset += sizeof(buffer)) {
    const size_t size = std::min(sizeof(buffer), size_t(partition->size) - offset);
    if (esp_partition_read(partition, offset, buffer, size) != ESP_OK) return false;
    for (size_t i = 0; i < size; ++i) if (buffer[i] != 0xff) return false;
    vTaskDelay(1);
  }
  return true;
}
}  // namespace

void FtpFiles::begin() {
  mounted_ = filesystem_.begin(false);
  if (!mounted_ && filesystemIsBlank()) {
    partner::log.println("[files] blank partition; initializing LittleFS");
    mounted_ = filesystem_.format() && filesystem_.begin(false);
  }
  if (!mounted_) {
    partner::log.println("[files] LittleFS mount failed; existing partition left intact, FTP/capture unavailable");
    return;
  }
  const size_t used = filesystem_.usedBytes();
  const size_t total = filesystem_.totalBytes();
  partner::log.printf("[files] LittleFS used=%u total=%u\n",
                static_cast<unsigned>(used), static_cast<unsigned>(total));
  if (!config::bootAudioCaptureEnabled) {
    partner::log.println("[capture] boot recording disabled");
    return;
  }
  if (filesystem_.exists(BootAudioCapture::micPath) || filesystem_.exists(BootAudioCapture::afePath)) {
    partner::log.println("[capture] existing recording preserved; delete both WAV files via FTP and reboot to recapture");
    return;
  }
  const size_t required = BootAudioCapture::sampleCount * 4 + 16384;
  if (total - used < required) {
    partner::log.println("[capture] insufficient storage");
    return;
  }
  capture_.arm();
}

void FtpFiles::tick() {
  if (!mounted_) return;
  if (WiFi.status() == WL_CONNECTED) {
    if (!started_) {
      ftp_.begin(FTP_USER, FTP_PASSWORD);
      started_ = true;
    }
    if (WiFi.localIP() != announcedIp_) {
      announcedIp_ = WiFi.localIP();
      partner::log.printf("[files] FTP ready: ftp://%s:21/ (passive data port 50009)\n",
                    announcedIp_.toString().c_str());
    }
    ftp_.handleFTP();
  }
  if (capture_.complete()) {
    const bool saved = capture_.save(filesystem_);
    partner::log.printf("[capture] %s; files task stack_free=%u\n", saved ? "COMPLETE, WAV files available via FTP" : "save failed",
                  static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
  }
}

}  // namespace partner
