#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
build_dir=$(mktemp -d /tmp/partner-tests.XXXXXX)
trap 'rm -rf "$build_dir"' EXIT
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc tests/reminder_test.cpp -o "$build_dir/reminder_test"
"$build_dir/reminder_test"
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc tests/pcm_test.cpp -o "$build_dir/pcm_test"
"$build_dir/pcm_test"
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc tests/resample_test.cpp -o "$build_dir/resample_test"
"$build_dir/resample_test"
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc tests/audio_samples_test.cpp -o "$build_dir/audio_samples_test"
"$build_dir/audio_samples_test"
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Iinclude -DDEFAULT_SPEAKER_VOLUME=-1.0f -DDEFAULT_MIC_VOLUME=100000.0f -DEXPECT_SPEAKER_VOLUME=0.0f -DEXPECT_MIC_VOLUME=100000.0f tests/audio_config_test.cpp -o "$build_dir/audio_config_test"
"$build_dir/audio_config_test"
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Iinclude -DDEFAULT_SPEAKER_VOLUME=100000.0f -DDEFAULT_MIC_VOLUME=-1.0f -DEXPECT_SPEAKER_VOLUME=100000.0f -DEXPECT_MIC_VOLUME=0.0f tests/audio_config_test.cpp -o "$build_dir/audio_config_test"
"$build_dir/audio_config_test"
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc tests/audio_activity_test.cpp -o "$build_dir/audio_activity_test"
"$build_dir/audio_activity_test"
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc tests/sensor_node_test.cpp -o "$build_dir/sensor_node_test"
"$build_dir/sensor_node_test"
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc tests/personal_reminders_test.cpp -o "$build_dir/personal_reminders_test"
"$build_dir/personal_reminders_test"
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc tests/greeting_test.cpp -o "$build_dir/greeting_test"
"$build_dir/greeting_test"
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc tests/button_test.cpp -o "$build_dir/button_test"
"$build_dir/button_test"
json_dir=.pio/libdeps/esp32-s3-devkitc1-n16r8/ArduinoJson/src
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc -I"$json_dir" tests/mcp_protocol_test.cpp -o "$build_dir/mcp_protocol_test"
"$build_dir/mcp_protocol_test"
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc -I"$json_dir" tests/responses_test.cpp -o "$build_dir/responses_test"
"$build_dir/responses_test"
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Iinclude tests/timezone_test.cpp -o "$build_dir/timezone_test"
"$build_dir/timezone_test"
python3 -B tests/test_voice_models.py
python3 -B tests/mic_stream_test.py
python3 -B -m unittest tests.test_espnow_linux
