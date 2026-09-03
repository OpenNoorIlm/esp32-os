// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorOS — audio_manager.h                                                ║
// ║  High-level audio API: robot.say(), robot.play(), robot.beep(),          ║
// ║  robot.tone(), robot.silence()                                           ║
// ║                                                                          ║
// ║  Hardware:                                                               ║
// ║    ESP32 DAC GPIO25 (right) / GPIO26 (left) → PAM8403 amp → speakers    ║
// ║    I2S master mode for WAV playback                                      ║
// ║    Buzzer on GPIO27 (optional) for tones                                ║
// ║                                                                          ║
// ║  Lua API (merged into "robot" global):                                  ║
// ║    robot.say("text")             TTS via SAM / espeak phonemes          ║
// ║    robot.play("/sound.wav")      Play WAV from SPIFFS                   ║
// ║    robot.beep(freq, ms)          Tone on DAC/buzzer                     ║
// ║    robot.tone(freq, ms)          Alias for beep                         ║
// ║    robot.silence()               Stop all audio immediately             ║
// ║    robot.volume(0-100)           Set master volume                      ║
// ║    robot.audio_status()          Returns "playing"/"idle"               ║
// ║                                                                          ║
// ║  Shell commands (via shell.add):                                        ║
// ║    say <text>                    Speak text                             ║
// ║    play <path>                   Play a WAV file                        ║
// ║    beep [freq] [ms]              Play a beep                            ║
// ║    volume [0-100]                Get/set volume                         ║
// ║                                                                          ║
// ║  MIT License — NoorRobot / OpenNoorIlm                                  ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once
#include "lua_config.h"
extern "C" {
#include "lua_src/lua-master/lua.h"
#include "lua_src/lua-master/lauxlib.h"
}
#include <Arduino.h>
#include <SPIFFS.h>
#include <driver/dac.h>
#include <driver/i2s.h>
#include <driver/ledc.h>
#include <math.h>
#include "shell_add_manager.h"

// ════════════════════════════════════════════════════════════════════════════
// Audio config
// ════════════════════════════════════════════════════════════════════════════
#define AUDIO_DAC_RIGHT   DAC_CHANNEL_1   // GPIO25
#define AUDIO_DAC_LEFT    DAC_CHANNEL_2   // GPIO26
#define AUDIO_BUZZER_PIN  27              // PWM buzzer (optional)
#define AUDIO_I2S_NUM     I2S_NUM_0
#define AUDIO_SAMPLE_RATE 16000           // WAV target sample rate
#define AUDIO_DMA_BUF_CNT 8
#define AUDIO_DMA_BUF_LEN 64
#define AUDIO_LEDC_CHANNEL LEDC_CHANNEL_0
#define AUDIO_LEDC_TIMER   LEDC_TIMER_0

namespace AudioManager {

// ── State ─────────────────────────────────────────────────────────────────────
static uint8_t  _volume       = 80;     // 0–100
static bool     _playing      = false;
static bool     _i2sInstalled = false;
static TaskHandle_t _audioTask = nullptr;

// ── Volume scaling ────────────────────────────────────────────────────────────
inline float _vol() { return (float)_volume / 100.0f; }

// ── I2S setup ─────────────────────────────────────────────────────────────────
inline void _i2sBegin(uint32_t sampleRate = AUDIO_SAMPLE_RATE) {
  if (_i2sInstalled) { i2s_driver_uninstall(AUDIO_I2S_NUM); _i2sInstalled = false; }
  i2s_config_t cfg = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
    .sample_rate          = sampleRate,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_MSB,
    .intr_alloc_flags     = 0,
    .dma_buf_count        = AUDIO_DMA_BUF_CNT,
    .dma_buf_len          = AUDIO_DMA_BUF_LEN,
    .use_apll             = false,
    .tx_desc_auto_clear   = true,
  };
  i2s_driver_install(AUDIO_I2S_NUM, &cfg, 0, nullptr);
  i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
  _i2sInstalled = true;
}

inline void _i2sStop() {
  if (_i2sInstalled) { i2s_stop(AUDIO_I2S_NUM); }
  _playing = false;
}

inline void silence() {
  _playing = false;
  if (_audioTask) { vTaskDelete(_audioTask); _audioTask = nullptr; }
  if (_i2sInstalled) { i2s_stop(AUDIO_I2S_NUM); }
  dacWrite(25, 128); dacWrite(26, 128); // mid-rail silence
}

inline void setVolume(uint8_t v) { _volume = v > 100 ? 100 : v; }
inline uint8_t getVolume() { return _volume; }
inline bool isPlaying() { return _playing; }

// ── Beep / tone via LEDC PWM ──────────────────────────────────────────────────
inline void beep(uint32_t freqHz = 880, uint32_t durationMs = 200) {
  if (freqHz == 0) { silence(); return; }
  // Use LEDC on buzzer pin if wired, else DAC sine
#ifdef AUDIO_BUZZER_PIN
  // Arduino ESP32 core 3.x API: ledcAttach(pin, freq, resolution)
  ledcAttach(AUDIO_BUZZER_PIN, freqHz, 8);
  ledcWrite(AUDIO_BUZZER_PIN, 128); // 50% duty
  delay(durationMs);
  ledcWrite(AUDIO_BUZZER_PIN, 0);
  ledcDetach(AUDIO_BUZZER_PIN);
#else
  // Synthesise sine on DAC
  _i2sBegin(freqHz * 32);
  const int samples = 32;
  uint16_t sine[samples * 2]; // interleaved L+R
  float vol = _vol();
  for (int i = 0; i < samples; i++) {
    uint16_t s = (uint16_t)((sinf(2.0f * M_PI * i / samples) * 0.5f + 0.5f) * 65535.0f * vol);
    sine[i * 2]     = s; // left
    sine[i * 2 + 1] = s; // right
  }
  unsigned long end = millis() + durationMs;
  size_t written = 0;
  while (millis() < end) {
    i2s_write(AUDIO_I2S_NUM, sine, sizeof(sine), &written, portMAX_DELAY);
    yield();
  }
  _i2sStop();
#endif
}

// ── WAV file player ───────────────────────────────────────────────────────────
struct WavHeader {
  char     riff[4];       // "RIFF"
  uint32_t fileSize;
  char     wave[4];       // "WAVE"
  char     fmt[4];        // "fmt "
  uint32_t fmtSize;
  uint16_t audioFormat;   // 1=PCM
  uint16_t channels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
  char     data[4];       // "data"
  uint32_t dataSize;
};

struct PlayCtx { String path; uint8_t vol; };

static void _playTask(void* arg) {
  PlayCtx* ctx = static_cast<PlayCtx*>(arg);
  String path  = ctx->path;
  float vol    = (float)ctx->vol / 100.0f;
  delete ctx;

  File f = SPIFFS.open(path, "r");
  if (!f) { _playing = false; vTaskDelete(nullptr); return; }

  // Read WAV header (44 bytes standard)
  WavHeader hdr;
  if (f.read((uint8_t*)&hdr, sizeof(hdr)) != sizeof(hdr)) {
    f.close(); _playing = false; vTaskDelete(nullptr); return;
  }

  // Validate and configure I2S
  uint32_t sr = hdr.sampleRate > 0 ? hdr.sampleRate : AUDIO_SAMPLE_RATE;
  _i2sBegin(sr);
  _playing = true;

  uint8_t buf[512];
  uint8_t scaled[512];
  while (_playing && f.available()) {
    int n = f.read(buf, sizeof(buf));
    if (n <= 0) break;

    // Apply volume to 16-bit PCM
    if (hdr.bitsPerSample == 16) {
      for (int i = 0; i < n; i += 2) {
        int16_t s = (int16_t)(buf[i] | (buf[i+1] << 8));
        s = (int16_t)(s * vol);
        scaled[i]   = (uint8_t)(s & 0xFF);
        scaled[i+1] = (uint8_t)((s >> 8) & 0xFF);
      }
    } else {
      // 8-bit: scale around midpoint 128
      for (int i = 0; i < n; i++) {
        int16_t s = (int16_t)buf[i] - 128;
        s = (int16_t)(s * vol);
        scaled[i] = (uint8_t)(s + 128);
      }
    }

    size_t written = 0;
    i2s_write(AUDIO_I2S_NUM, scaled, n, &written, portMAX_DELAY);
    yield();
  }

  f.close();
  _playing = false;
  _i2sStop();
  _audioTask = nullptr;
  vTaskDelete(nullptr);
}

inline void play(const String& path, bool blocking = false) {
  if (_playing) silence();
  if (!SPIFFS.exists(path)) {
    Serial.printf("[audio] file not found: %s\n", path.c_str());
    return;
  }
  auto* ctx = new PlayCtx{path, _volume};
  if (blocking) {
    _playTask(ctx);
  } else {
    xTaskCreate(_playTask, "audio", 8192, ctx, 4, &_audioTask);
  }
}

// ── SAM-inspired phoneme TTS ──────────────────────────────────────────────────
// Minimal phoneme-to-DAC synthesis. Not a full TTS engine but produces
// recognisable robot voice for short phrases on ESP32's limited RAM.
// For better TTS: put espeak output WAVs in SPIFFS and call play() directly.

// Phoneme table: letter → [freq_Hz, duration_ms, amplitude]
struct Phoneme { uint16_t freq; uint16_t ms; uint8_t amp; };

static const Phoneme _phonemes[26] = {
  {700, 80, 90},  // a
  {800, 40, 70},  // b
  {900, 40, 70},  // c
  {750, 50, 80},  // d
  {600, 80, 95},  // e
  {850, 40, 70},  // f
  {780, 50, 75},  // g
  {820, 40, 60},  // h
  {650, 70, 90},  // i
  {900, 30, 65},  // j
  {830, 40, 70},  // k
  {760, 60, 80},  // l
  {710, 60, 85},  // m
  {720, 60, 85},  // n
  {680, 80, 95},  // o
  {870, 40, 70},  // p
  {950, 30, 65},  // q
  {730, 60, 85},  // r
  {800, 50, 80},  // s
  {840, 40, 75},  // t
  {660, 70, 90},  // u
  {860, 40, 70},  // v
  {790, 40, 65},  // w
  {920, 30, 60},  // x
  {670, 70, 88},  // y
  {810, 40, 70},  // z
};

inline void say(const String& text) {
  if (_playing) silence();
  String lower = text;
  lower.toLowerCase();
  lower.trim();

  _i2sBegin(AUDIO_SAMPLE_RATE);
  _playing = true;

  float vol = _vol();
  const int samplesPerMs = AUDIO_SAMPLE_RATE / 1000;

  for (unsigned int ci = 0; ci < lower.length() && _playing; ci++) {
    char c = lower[ci];

    if (c == ' ' || c == ',') {
      // Short pause
      int pauseSamples = 80 * samplesPerMs;
      std::vector<uint16_t> silence_buf(pauseSamples * 2, 32768); // mid-rail
      size_t w = 0;
      i2s_write(AUDIO_I2S_NUM, silence_buf.data(),
                silence_buf.size() * 2, &w, portMAX_DELAY);
      yield();
      continue;
    }

    if (c == '.') {
      // Longer pause
      int pauseSamples = 200 * samplesPerMs;
      std::vector<uint16_t> silence_buf(pauseSamples * 2, 32768);
      size_t w = 0;
      i2s_write(AUDIO_I2S_NUM, silence_buf.data(),
                silence_buf.size() * 2, &w, portMAX_DELAY);
      yield();
      continue;
    }

    if (c < 'a' || c > 'z') continue;

    const Phoneme& ph = _phonemes[c - 'a'];
    int totalSamples  = ph.ms * samplesPerMs;
    float amp         = (ph.amp / 255.0f) * vol * 32767.0f;
    float period      = (float)AUDIO_SAMPLE_RATE / (float)ph.freq;

    // Generate sine wave for this phoneme
    std::vector<uint16_t> wave(totalSamples * 2);
    for (int s = 0; s < totalSamples; s++) {
      // Apply a short fade-in/out envelope
      float env = 1.0f;
      int fadeLen = min(totalSamples / 8, 8 * samplesPerMs);
      if (s < fadeLen) env = (float)s / fadeLen;
      else if (s > totalSamples - fadeLen) env = (float)(totalSamples - s) / fadeLen;

      float val = sinf(2.0f * M_PI * s / period) * amp * env;
      uint16_t sample = (uint16_t)(val + 32768.0f);
      wave[s * 2]     = sample; // right
      wave[s * 2 + 1] = sample; // left
    }

    size_t written = 0;
    i2s_write(AUDIO_I2S_NUM, wave.data(), wave.size() * 2, &written, portMAX_DELAY);
    yield();
  }

  _playing = false;
  _i2sStop();
}

// ── Register shell commands ───────────────────────────────────────────────────
inline void registerShellCommands() {
  ShellAddManager::add("say", "Say text aloud  say <text>",
    [](const std::vector<String>& args, Print& out) {
      if (args.empty()) { out.println("Usage: say <text>"); return; }
      String text;
      for (auto& a : args) { if (text.length()) text += " "; text += a; }
      out.println("[audio] saying: " + text);
      say(text);
    });

  ShellAddManager::add("play", "Play a WAV file  play <path>",
    [](const std::vector<String>& args, Print& out) {
      if (args.empty()) { out.println("Usage: play <path.wav>"); return; }
      out.println("[audio] playing: " + args[0]);
      play(args[0], false);
    });

  ShellAddManager::add("beep", "Play a beep  beep [freq_hz] [duration_ms]",
    [](const std::vector<String>& args, Print& out) {
      uint32_t freq = args.size() > 0 ? (uint32_t)args[0].toInt() : 880;
      uint32_t ms   = args.size() > 1 ? (uint32_t)args[1].toInt() : 200;
      out.printf("[audio] beep %dHz %dms\n", freq, ms);
      beep(freq, ms);
    });

  ShellAddManager::add("volume", "Get/set volume  volume [0-100]",
    [](const std::vector<String>& args, Print& out) {
      if (args.empty()) {
        out.printf("Volume: %d%%\n", getVolume());
      } else {
        uint8_t v = (uint8_t)constrain(args[0].toInt(), 0, 100);
        setVolume(v);
        out.printf("Volume set to %d%%\n", v);
      }
    });

  ShellAddManager::add("silence", "Stop all audio",
    [](const std::vector<String>&, Print& out) {
      silence(); out.println("[audio] silenced");
    });
}

} // namespace AudioManager

// ════════════════════════════════════════════════════════════════════════════
// Lua bindings — merged into existing "robot" global table
// ════════════════════════════════════════════════════════════════════════════
namespace LuaAudioBindings {



static int l_say(lua_State* L) {
  const char* text = luaL_checkstring(L, 1);
  AudioManager::say(text);
  return 0;
}
static int l_play(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  bool blocking    = lua_toboolean(L, 2); // optional 2nd arg
  AudioManager::play(path, blocking);
  return 0;
}
static int l_beep(lua_State* L) {
  uint32_t freq = (uint32_t)luaL_optinteger(L, 1, 880);
  uint32_t ms   = (uint32_t)luaL_optinteger(L, 2, 200);
  AudioManager::beep(freq, ms);
  return 0;
}
static int l_silence(lua_State* L) {
  (void)L; AudioManager::silence(); return 0;
}
static int l_volume(lua_State* L) {
  if (lua_isnoneornil(L, 1)) {
    lua_pushinteger(L, AudioManager::getVolume());
    return 1;
  }
  AudioManager::setVolume((uint8_t)luaL_checkinteger(L, 1));
  return 0;
}
static int l_audio_status(lua_State* L) {
  lua_pushstring(L, AudioManager::isPlaying() ? "playing" : "idle");
  return 1;
}

static const luaL_Reg _audio_fns[] = {
  {"say",          l_say},
  {"play",         l_play},
  {"beep",         l_beep},
  {"tone",         l_beep},   // alias
  {"silence",      l_silence},
  {"volume",       l_volume},
  {"audio_status", l_audio_status},
  {nullptr,        nullptr}
};

// Merge into existing "robot" table
inline void registerAudio(lua_State* L) {
  lua_getglobal(L, "robot");
  if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_newtable(L); }
  for (const luaL_Reg* r = _audio_fns; r->name; r++) {
    lua_pushcfunction(L, r->func);
    lua_setfield(L, -2, r->name);
  }
  lua_setglobal(L, "robot");
}

} // namespace LuaAudioBindings
