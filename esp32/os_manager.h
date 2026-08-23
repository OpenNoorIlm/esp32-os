#pragma once
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h>
#include "package_manager.h" // reuse PackageManager::httpsGet for the small manifest fetch

// Real OS self-update (OTA): fetches a compiled firmware .bin published in
// the esp32-os repo and flashes it to the inactive OTA partition, then
// restarts into it. This is a fundamentally different mechanism from
// apt/app-installer -- those only ever write userland files onto
// LittleFS/SD; this is the only thing in the project that touches the
// actual program flash the ESP32 boots and runs.
//
// ============================== PREREQUISITE ==============================
// This board must be flashed with a partition scheme that actually HAS an
// OTA partition pair (ota_0/ota_1 + otadata) -- e.g. Arduino IDE's
// "Minimal SPIFFS (... APP with OTA / ... SPIFFS)" options. The "Huge APP
// (3MB No OTA/1MB SPIFFS)" scheme deliberately REMOVES the OTA partitions
// to make room for one giant single app partition, so as configured,
// Update.begin() below will always fail. Changing partition scheme means
// re-flashing over USB ONCE (the partition table itself can only be
// rewritten by a full USB flash, never over the air) -- after that one-time
// reflash, updates via this module work wirelessly from then on.
// ===========================================================================
//
// Expected esp32-os repo layout (mirrors the config.json convention
// package_manager.h already uses for apt/app-installer packages):
//   /firmware.json  <- { "version": "0.2.0", "bin": "esp32/build/espressif.esp32.esp32/esp32.ino.bin" }
//   the .bin path above <- output of Sketch > Export Compiled Binary
// Bump "version" and commit a fresh .bin together each time you cut a release.
//
// IMPORTANT -- "bin" must point at the PLAIN "<sketch>.ino.bin", never
// "<sketch>.ino.merged.bin". Arduino's export writes both into the same
// build folder: the merged one bundles the bootloader + partition table at
// their absolute flash offsets for a one-time USB flash via esptool, while
// the plain one is just the application image OTA actually needs. Pointing
// "bin" (or an untrusted os-install URL below) at a merged file is refused
// outright -- see the ".merged.bin" guard in flashFromUrl().
namespace OsManager {

#define OS_REPO "OpenNoorIlm/esp32-os"
#define FIRMWARE_VERSION "0.1.0" // bump this to match whatever you just published

inline String firmwareManifestUrl() {
  return "https://raw.githubusercontent.com/" + String(OS_REPO) + "/main/firmware.json";
}

// Fetches firmware.json and reports what's available, without flashing
// anything -- like 'apt update', but for the OS itself. On success, fills
// outVersion/outBinUrl for performUpdate() to use.
inline String checkForOsUpdate(String& outVersion, String& outBinUrl) {
  String manifest = PackageManager::httpsGet(firmwareManifestUrl());
  if (manifest.length() == 0)
    return "error: could not reach " + String(OS_REPO) + " (offline, or firmware.json not published yet)";

  JsonDocument doc;
  if (deserializeJson(doc, manifest) != DeserializationError::Ok) return "error: firmware.json malformed.";

  String remoteVersion = doc["version"] | "";
  String binPath = doc["bin"] | "";
  if (remoteVersion.length() == 0 || binPath.length() == 0)
    return "error: firmware.json is missing 'version' or 'bin'.";

  outVersion = remoteVersion;
  outBinUrl = "https://raw.githubusercontent.com/" + String(OS_REPO) + "/main/" + binPath;

  if (remoteVersion == FIRMWARE_VERSION) return "Running " + String(FIRMWARE_VERSION) + " -- already up to date.";
  return "Running " + String(FIRMWARE_VERSION) + ", " + remoteVersion + " available.";
}

// Shared flashing routine -- downloads binUrl and flashes it to the
// inactive OTA partition, streaming progress live to `out` (same
// Print&-streaming pattern as installOfficial() in package_manager.h).
// Used by both performUpdate() (official esp32-os repo, via firmware.json)
// and performUpdateFromUrl() (untrusted, arbitrary URL). Restarts into the
// new firmware automatically on success -- does NOT return in that case.
// On any failure, the currently-running firmware is left untouched and an
// "error: ..." string is returned describing what went wrong.
inline String flashFromUrl(const String& binUrl, const String& versionLabel, Print& out) {
  // Hard safety guard, enforced here regardless of entry point: a merged
  // image bundles the bootloader + partition table at absolute flash
  // offsets for one-time USB flashing -- writing it into an OTA app
  // partition will very likely leave the board unable to boot. This is a
  // filename heuristic, not a content check, so it's a first line of
  // defense, not a guarantee -- ultimately the URL still has to actually
  // point at a valid application-only image.
  if (binUrl.endsWith(".merged.bin")) {
    return "error: refusing to flash a '*.merged.bin' -- that's a full-chip image "
           "(bootloader+partitions+app) meant for one-time USB flashing with esptool, not "
           "an OTA app image. Point at the plain '<sketch>.ino.bin' Arduino also exports "
           "into the same build folder instead.";
  }

  out.println("Downloading " + binUrl + " ...");
  out.flush();

  WiFiClientSecure client;
  client.setInsecure(); // matches the rest of this project's HTTPS handling -- see the
                         // SECURITY NOTE at the top of package_manager.h
  HTTPClient https;
  if (!https.begin(client, binUrl)) return "error: could not reach firmware binary URL.";
  https.addHeader("User-Agent", "NoorShell");
  int code = https.GET();
  if (code != 200) { https.end(); return "error: HTTP " + String(code) + " fetching firmware binary."; }

  int len = https.getSize();
  if (len <= 0) {
    https.end();
    return "error: server didn't report a firmware size -- refusing to flash an unknown-size image.";
  }

  if (!Update.begin(len)) {
    https.end();
    return "error: Update.begin() failed -- most likely this board's partition scheme has no "
           "OTA partition (see the PREREQUISITE note at the top of os_manager.h), or the image "
           "is larger than the OTA app partition can hold. Reflash over USB with an "
           "OTA-capable partition scheme first.";
  }

  int lastPct = -1;
  Update.onProgress([&out, lastPct](size_t written, size_t total) mutable {
    int pct = total ? (int)((written * 100) / total) : 0;
    if (pct != lastPct && pct % 10 == 0) {
      out.println("  " + String(pct) + "%");
      out.flush();
      lastPct = pct;
    }
  });

  size_t written = Update.writeStream(*https.getStreamPtr());
  https.end();

  if (written != (size_t)len) {
    Update.abort();
    return "error: only wrote " + String(written) + " of " + String(len) + " bytes -- aborted, firmware unchanged.";
  }
  if (!Update.end(true)) {
    return "error: Update.end() failed (" + String(Update.errorString()) + ") -- firmware unchanged.";
  }

  out.println("100%");
  out.println("Flash write verified. Restarting into " + versionLabel + "......");
  out.flush();
  delay(300);
  ESP.restart();
  return ""; // unreachable
}

// Checks the official esp32-os repo and, if a newer version is published,
// flashes it via flashFromUrl().
inline String performUpdate(Print& out) {
  String remoteVersion, binUrl;
  String status = checkForOsUpdate(remoteVersion, binUrl);
  if (status.startsWith("error:")) return status;
  out.println(status);
  out.flush();
  if (remoteVersion == FIRMWARE_VERSION) return ""; // already current, nothing to do
  return flashFromUrl(binUrl, remoteVersion, out);
}

// Installs firmware from an arbitrary, untrusted URL -- gated behind an
// explicit [Y/n] confirmation in shell_server.h (see the CONFIRM_UNTRUSTED
// handling there), same pattern as PackageManager::installFromManifestUrl()
// for apps/packages. Unlike that function there's no manifest/license step
// here -- there's no equivalent of config.json for a bare firmware URL, so
// this goes straight to flashFromUrl() (which still enforces the
// ".merged.bin" guard regardless of this being an untrusted source).
inline String performUpdateFromUrl(const String& url, Print& out) {
  return flashFromUrl(url, "custom build (" + url + ")", out);
}

} // namespace OsManager
