#include "Control.h"
#include <cstring>
#include <Arduino.h>
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"
#include "esp_system.h"
void ControlModule::poll(bool cameraReady, bool audioReady) {
  while (serial_.available()) {
    int c=serial_.read(); if (c<0) break;
    if (c=='\r') continue;
    if (c=='\n') {
      command_[used_]='\0';
      if (!strcmp(command_,"ping")) serial_.println("pong");
      else if (!strcmp(command_,"bootloader")) {
        serial_.println("Entering ROM bootloader");
        serial_.flush(); delay(100);
        REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
        esp_restart();
      }
      else if (!strcmp(command_,"status")) serial_.printf("camera=%s audio=%s\r\n", cameraReady ? "ready" : "failed", audioReady ? "ready" : "failed");
      else if (!strcmp(command_,"help")) serial_.println("Commands: ping, status, bootloader, help");
      else if (used_) serial_.println("Unknown command");
      used_=0;
    } else if (used_<sizeof(command_)-1) command_[used_++]=static_cast<char>(c);
    else used_=0;
  }
}
