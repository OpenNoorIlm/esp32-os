#pragma once
// ── video_player.h ────────────────────────────────────────────────────────────
// MJPEG video player for NoorRobot TFT (ILI9341 320x240)
// Reads MJPEG file from SD card, decodes JPEG frames, blits to TFT
// Optional: synchronized audio via I2S DAC (GPIO25, PAM8403)
//
// Video format: MJPEG (.avi or .mjpeg)
//   - Resolution: 320x240 (or smaller, centered)
//   - FPS: 10-15 recommended for smooth playback
//   - Audio track: PCM u8, 8000Hz mono (interleaved in AVI RIFF chunks)
//
// How to export from Blender:
//   Output → FFmpeg Video → Container: AVI → Video Codec: JPEG
//   Resolution: 320x240, FPS: 10-15
//   Audio: AAC → then convert with ffmpeg:
//   ffmpeg -i render.avi -vcodec mjpeg -q:v 5 -ar 8000 -ac 1 -acodec pcm_u8 out.avi
//
// Libraries needed:
//   TJpgDec (JPEG decoder, by Bodmer — install via Library Manager)
//   TFT_eSPI (already in project)
//   SD (built-in)
//   driver/i2s.h (ESP32 core)
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SD.h>
#include <driver/i2s.h>
#if __has_include(<TJpgDec.h>)
#include <TJpgDec.h>
#define NOOR_HAS_TJPGDEC 1
#else
#warning "TJpgDec library not installed -- video_player JPEG decode disabled. Install via Library Manager."
#define NOOR_HAS_TJPGDEC 0
#endif
#include "sd_card.h"

// ── Audio config (matches audio player) ──────────────────────────────────────
#define VP_SAMPLE_RATE   8000
#define VP_AUDIO_BUF     512

// ── Forward declare TFT (defined in tft_manager.h) ───────────────────────────
extern TFT_eSPI tft;

namespace VideoPlayer {

// ── State ─────────────────────────────────────────────────────────────────────
bool      _playing       = false;
bool      _audioEnabled  = true;
bool      _i2sReady      = false;
uint32_t  _frameCount    = 0;
uint32_t  _fps           = 10;
uint16_t  _vidW          = 320;
uint16_t  _vidH          = 240;
int16_t   _offX          = 0;
int16_t   _offY          = 0;

File      _file;

// Pixel buffer for TJpgDec output (one row at a time)
uint16_t  _lineBuf[320];

// ── TJpgDec callback — called per MCU block ───────────────────────────────────
bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return false;
  tft.pushImage(_offX + x, _offY + y, w, h, bitmap);
  return true;
}

// ── I2S audio init ────────────────────────────────────────────────────────────
void initAudio() {
  if (_i2sReady) return;
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
    .sample_rate = VP_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_MSB,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = true
  };
  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, NULL);
  i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
  _i2sReady = true;
}

void stopAudio() {
  if (_i2sReady) {
    i2s_zero_dma_buffer(I2S_NUM_0);
    i2s_driver_uninstall(I2S_NUM_0);
    _i2sReady = false;
  }
}

void writeAudioSamples(uint8_t* raw, size_t len) {
  static uint16_t out[VP_AUDIO_BUF * 2];
  size_t samples = len < VP_AUDIO_BUF ? len : VP_AUDIO_BUF;
  for (size_t i = 0; i < samples; i++) {
    uint16_t s = (uint16_t)raw[i] << 8;
    out[i * 2]     = s;
    out[i * 2 + 1] = s;
  }
  size_t bw;
  i2s_write(I2S_NUM_0, out, samples * 4, &bw, 10);
}

// ── RIFF/AVI parser helpers ───────────────────────────────────────────────────
// We do a minimal streaming RIFF parse — just enough to find movi chunks
// (00dc = video frame, 01wb = audio data)

uint32_t read32LE(File& f) {
  uint8_t b[4];
  f.read(b, 4);
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

uint16_t read16LE(File& f) {
  uint8_t b[2];
  f.read(b, 2);
  return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

bool readFourCC(File& f, char* cc) {
  return f.read((uint8_t*)cc, 4) == 4;
}

// ── Seek to 'movi' LIST in AVI ────────────────────────────────────────────────
bool seekToMovi(File& f) {
  // AVI structure: RIFF(AVI ) → LIST(hdrl) → LIST(movi) → ...
  // We scan forward for 'movi' fourcc
  f.seek(0);
  uint8_t buf[4];
  uint32_t fileSize = f.size();
  for (uint32_t pos = 12; pos < fileSize - 8; pos++) {
    f.seek(pos);
    f.read(buf, 4);
    if (buf[0]=='m' && buf[1]=='o' && buf[2]=='v' && buf[3]=='i') {
      f.seek(pos + 4); // skip past 'movi', now at first chunk
      return true;
    }
  }
  return false;
}

// ── Parse AVI header for FPS and resolution ───────────────────────────────────
void parseAviHeader(File& f) {
  // Scan for avih chunk
  uint8_t buf[4];
  uint32_t fileSize = f.size();
  f.seek(12);
  for (uint32_t pos = 12; pos < min((uint32_t)4096, fileSize - 8); pos++) {
    f.seek(pos);
    f.read(buf, 4);
    if (buf[0]=='a' && buf[1]=='v' && buf[2]=='i' && buf[3]=='h') {
      f.seek(pos + 4); // skip chunk size
      uint32_t chunkSize = read32LE(f);
      if (chunkSize >= 28) {
        uint32_t microPerFrame = read32LE(f); // microseconds per frame
        if (microPerFrame > 0) _fps = 1000000UL / microPerFrame;
        f.seek(pos + 4 + 4 + 32); // skip to width/height
        _vidW = (uint16_t)read32LE(f);
        _vidH = (uint16_t)read32LE(f);
        // Center on 320x240 TFT
        _offX = (_vidW < 320) ? (320 - _vidW) / 2 : 0;
        _offY = (_vidH < 240) ? (240 - _vidH) / 2 : 0;
        Serial.printf("[VP] FPS=%d  %dx%d  offset=%d,%d\n", _fps, _vidW, _vidH, _offX, _offY);
      }
      return;
    }
  }
  // Defaults if header not parsed
  _fps = 10; _vidW = 320; _vidH = 240; _offX = 0; _offY = 0;
}

// ── JPEG frame decode + display ───────────────────────────────────────────────
bool decodeFrame(uint8_t* jpegData, uint32_t jpegLen) {
#if NOOR_HAS_TJPGDEC
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tftOutput);
  TJpgDec.setSwapBytes(true);
  JRESULT res = TJpgDec.drawJpg(_offX, _offY, jpegData, jpegLen);
  return res == JDR_OK;
#else
  (void)jpegData; (void)jpegLen;
  Serial.println("[VP] TJpgDec not installed -- install via Library Manager");
  return false;
#endif
}

// ── Main play function (blocking — call from a FreeRTOS task) ─────────────────
void playFile(const char* path, bool withAudio = true) {
  if (!SdCard::mounted()) {
    Serial.println("[VP] SD not mounted");
    return;
  }

  _file = SD.open(path);
  if (!_file) {
    Serial.printf("[VP] Cannot open: %s\n", path);
    return;
  }

  Serial.printf("[VP] Playing: %s\n", path);
  parseAviHeader(_file);

  if (withAudio && _audioEnabled) initAudio();

  if (!seekToMovi(_file)) {
    Serial.println("[VP] No movi chunk found — invalid AVI?");
    _file.close();
    return;
  }

  _playing     = true;
  _frameCount  = 0;
  tft.fillScreen(TFT_BLACK);

  uint32_t frameInterval = 1000 / _fps; // ms per frame
  char     cc[5] = {0};
  uint8_t* frameBuf = nullptr;
  uint32_t frameBufSize = 0;

  while (_playing && _file.available()) {
    // Read chunk fourcc + size
    if (!readFourCC(_file, cc)) break;
    uint32_t chunkSize = read32LE(_file);
    if (chunkSize == 0 || chunkSize > 512 * 1024) {
      // Skip garbage / index chunks
      _file.seek(_file.position() + chunkSize);
      continue;
    }

    // RIFF chunks must be word-aligned
    uint32_t alignedSize = (chunkSize + 1) & ~1u;

    // ── Video frame: 00dc ────────────────────────────────────────────────────
    if (cc[0]=='0' && cc[1]=='0' && cc[2]=='d' && cc[3]=='c') {
      // Allocate or reuse frame buffer
      if (chunkSize > frameBufSize) {
        if (frameBuf) free(frameBuf);
        frameBuf = (uint8_t*)ps_malloc(chunkSize); // use PSRAM if available
        if (!frameBuf) frameBuf = (uint8_t*)malloc(chunkSize);
        frameBufSize = frameBuf ? chunkSize : 0;
      }

      if (frameBuf && frameBufSize >= chunkSize) {
        uint32_t t0 = millis();
        _file.read(frameBuf, chunkSize);
        decodeFrame(frameBuf, chunkSize);
        _frameCount++;

        // Frame rate throttle
        uint32_t elapsed = millis() - t0;
        if (elapsed < frameInterval) delay(frameInterval - elapsed);
      } else {
        _file.seek(_file.position() + alignedSize);
      }

      // Consume alignment byte
      if (alignedSize > chunkSize) _file.seek(_file.position() + (alignedSize - chunkSize));
    }

    // ── Audio chunk: 01wb ────────────────────────────────────────────────────
    else if (cc[0]=='0' && cc[1]=='1' && cc[2]=='w' && cc[3]=='b') {
      if (withAudio && _audioEnabled && _i2sReady) {
        uint8_t audioBuf[VP_AUDIO_BUF];
        uint32_t remaining = chunkSize;
        while (remaining > 0) {
          uint32_t toRead = remaining < VP_AUDIO_BUF ? remaining : VP_AUDIO_BUF;
          _file.read(audioBuf, toRead);
          writeAudioSamples(audioBuf, toRead);
          remaining -= toRead;
        }
        if (alignedSize > chunkSize) _file.seek(_file.position() + (alignedSize - chunkSize));
      } else {
        _file.seek(_file.position() + alignedSize);
      }
    }

    // ── LIST chunk (rec, idx1, etc.) — step into or skip ────────────────────
    else if (cc[0]=='L' && cc[1]=='I' && cc[2]=='S' && cc[3]=='T') {
      // Step into LIST — skip its sub-fourcc (4 bytes), parse children
      _file.seek(_file.position() + 4);
    }

    // ── idx1 / JUNK / anything else — skip ──────────────────────────────────
    else {
      _file.seek(_file.position() + alignedSize);
    }
  }

  if (frameBuf) free(frameBuf);
  _file.close();
  _playing = false;
  if (withAudio) stopAudio();
  Serial.printf("[VP] Done. Frames played: %d\n", _frameCount);
}

// ── FreeRTOS task wrapper ─────────────────────────────────────────────────────
struct PlayArgs {
  char path[128];
  bool audio;
};

void _playTask(void* arg) {
  PlayArgs* a = (PlayArgs*)arg;
  playFile(a->path, a->audio);
  free(a);
  vTaskDelete(NULL);
}

// Non-blocking: starts playback on a background task
void play(const char* path, bool withAudio = true) {
  if (_playing) {
    Serial.println("[VP] Already playing, stop first");
    return;
  }
  PlayArgs* a = (PlayArgs*)malloc(sizeof(PlayArgs));
  strncpy(a->path, path, 127);
  a->audio = withAudio;
  xTaskCreatePinnedToCore(_playTask, "VideoTask", 16384, a, 1, NULL, 1);
}

void stop() {
  _playing = false;
}

bool isPlaying() { return _playing; }

// ── Shell command handler ─────────────────────────────────────────────────────
// Usage: video play <path> [noaudio] | stop | status | ls
String handleCommand(const String& args) {
  String cmd = args;
  cmd.trim();

  if (cmd.startsWith("play ")) {
    String rest = cmd.substring(5); rest.trim();
    bool noAudio = rest.endsWith(" noaudio");
    if (noAudio) rest = rest.substring(0, rest.length() - 8);
    rest.trim();
    if (!rest.startsWith("/")) rest = "/" + rest;
    play(rest.c_str(), !noAudio);
    return "Playing: " + rest + (noAudio ? " (no audio)\n" : "\n");
  }

  if (cmd == "stop") {
    stop();
    return "Stopped\n";
  }

  if (cmd == "status") {
    if (_playing) return "Playing  frames=" + String(_frameCount) + "  fps=" + String(_fps) + "\n";
    return "Idle\n";
  }

  if (cmd == "ls" || cmd.startsWith("ls ")) {
    String path = cmd.length() > 3 ? cmd.substring(3) : "/";
    path.trim();
    // List only video files
    File root = SD.open(path.c_str());
    if (!root || !root.isDirectory()) return "Not a directory\n";
    String out = "";
    File f = root.openNextFile();
    while (f) {
      String name = f.name();
      if (name.endsWith(".avi") || name.endsWith(".mjpeg") || name.endsWith(".AVI")) {
        out += name + "  (" + String(f.size() / 1024) + " KB)\n";
      }
      f = root.openNextFile();
    }
    return out.length() ? out : "No video files found\n";
  }

  return "Usage: video [play <path> [noaudio]|stop|status|ls [path]]\n";
}

} // namespace VideoPlayer
