#pragma once

#include <FTPServer.h>
#include <LittleFS.h>
#include "BootAudioCapture.h"

namespace partner {

class FtpFiles {
 public:
  FtpFiles(fs::LittleFSFS& filesystem, BootAudioCapture& capture)
      : filesystem_(filesystem), capture_(capture), ftp_(filesystem) {}
  void begin();
  void tick();
  bool mounted() const { return mounted_; }

 private:
  fs::LittleFSFS& filesystem_;
  BootAudioCapture& capture_;
  FTPServer ftp_;
  bool mounted_ = false;
  bool started_ = false;
  IPAddress announcedIp_;
};

}  // namespace partner
