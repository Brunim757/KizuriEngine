#include "Kizuri/Audio.h"
#include <miniaudio.h>
namespace Kizuri {
const char* Audio_Version() {
  return "0.0.1-fase0";
}
bool Audio_TestEngineNoDevice() {
  ma_engine_config cfg = ma_engine_config_init();
  cfg.noDevice = MA_TRUE;
  cfg.channels = 2;
  cfg.sampleRate = 48000;
  ma_engine engine;
  ma_result r = ma_engine_init(&cfg, &engine);
  if (r != MA_SUCCESS) {
    return false;
  }
  ma_engine_uninit(&engine);
  return true;
}
}
