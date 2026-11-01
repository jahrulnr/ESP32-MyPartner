#include <app_config.h>

static_assert(partner::config::speakerVolume == EXPECT_SPEAKER_VOLUME, "Incorrect speaker volume");
static_assert(partner::config::microphoneVolume == EXPECT_MIC_VOLUME, "Incorrect microphone volume");

int main() {}
