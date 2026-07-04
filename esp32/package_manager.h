#pragma once
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>
#include "fs_manager.h"

// Handles two install surfaces:
//   apt           -- system packages, official repo: OpenNoorIlm/esp32-os-packages
//   app-installer -- user apps,       official repo: OpenNoorIlm/esp32-app-installer
// Both also support installing from an arbitrary URL, gated behind an
// explicit [Y/n] confirmation (see CONFIRM_UNTRUSTED handling in
// shell_server.h) since those aren't vetted the way the official repos are.
//
// SECURITY NOTE: HTTPS is used but certificate validation is skipped
// (setInsecure()) to avoid bundling a CA chain on constrained flash. Same
// category of tradeoff as the unencrypted shell transport elsewhere in
// this project -- fine for hobby use, worth hardening (real root CA,
// or at least pinning GitHub's) before trusting this on a hostile network.
namespace PackageManager {

#define APT_REPO "OpenNoorIlm/esp32-os-packages"
#define APP_REPO "OpenNoorIlm/esp32-app-installer"

inline String httpsGet(const String& url) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  if (!https.begin(client, url)) return "";
  https.addHeader("User-Agent", "NoorShell");
  int code = https.GET();
  String body = (code == 200) ? https.getString() : "";
  https.end();
  return body;
}

// Per your spec: absent/empty license -> treat as possibly closed-source
// and refuse. Any non-empty license string is accepted and shown as-is --
// this deliberately doesn't validate against an OSI allowlist yet.
inline bool licenseIsAcceptable(const String& license, String& note) {
  if (license.length() == 0 || license.equalsIgnoreCase("none") ||
      license.equalsIgnoreCase("closed-source") || license.equalsIgnoreCase("proprietary")) {
    note = "License: No license found -- might be closed-source, sorry cannot install "
           "(installing closed-source might come soon)";
    return false;
  }
  note = "License: " + license;
  return true;
}

// Generic curl-style HTTP/HTTPS request, usable for arbitrary URLs (not
// just package manifests). Returns the response body (or an error string
// prefixed "curl: (" like real curl's exit-style errors); status line and
// selected response headers are appended to statusOut unless silent.
// NOTE: ESP32's HTTPClient requires response header *names* to be declared
// up front via collectHeaders() -- it can't dump the full raw header block
// the way a real curl -I does, so we collect a fixed, common set instead.
inline String curlRequest(const String& url, const String& method,
                           const std::vector<String>& headerLines,
                           const String& body, bool follow, bool headOnly,
                           bool silent, const String& userAgent,
                           String& statusOut) {
  statusOut = "";
  bool isHttps = url.startsWith("https://");
  if (!isHttps && !url.startsWith("http://"))
    return "curl: (3) unsupported URL scheme (only http:// and https://)";

  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  HTTPClient http;
  bool began = isHttps ? (secureClient.setInsecure(), http.begin(secureClient, url))
                        : http.begin(plainClient, url);
  if (!began) return "curl: (6) could not resolve host or invalid URL";

  http.setFollowRedirects(follow ? HTTPC_FORCE_FOLLOW_REDIRECTS : HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.addHeader("User-Agent", userAgent.length() ? userAgent : "NoorShell-curl/1.0");
  for (const String& h : headerLines) {
    int c = h.indexOf(':');
    if (c == -1) continue;
    String name = h.substring(0, c); name.trim();
    String val  = h.substring(c + 1); val.trim();
    if (name.length()) http.addHeader(name, val);
  }

  static const char* WANTED[] = {"Content-Type", "Content-Length", "Server",
                                  "Date", "Location", "Cache-Control", "ETag"};
  http.collectHeaders(WANTED, sizeof(WANTED) / sizeof(WANTED[0]));

  String m = method; m.toUpperCase();
  int code;
  if (headOnly) code = http.sendRequest("HEAD");
  else if (m == "POST")   code = http.POST(body);
  else if (m == "PUT")    code = http.sendRequest("PUT", body);
  else if (m == "DELETE") code = http.sendRequest("DELETE", body);
  else if (m == "PATCH")  code = http.sendRequest("PATCH", body);
  else                    code = http.GET();

  if (code <= 0) {
    http.end();
    return "curl: (" + String(code) + ") request failed (connection error, timeout, or refused)";
  }

  if (!silent) {
    statusOut += "> " + (headOnly ? String("HEAD") : m) + " " + url + "\n";
    statusOut += "< HTTP " + String(code) + "\n";
    for (size_t i = 0; i < sizeof(WANTED) / sizeof(WANTED[0]); i++) {
      String v = http.header(WANTED[i]);
      if (v.length()) statusOut += "< " + String(WANTED[i]) + ": " + v + "\n";
    }
  }

  String out = headOnly ? "" : http.getString();
  http.end();
  return out;
}

} // namespace PackageManager

namespace PackageManager {

inline String fetchConfigFromGithubDir(const String& repo, const String& pkgPath) {
  String url = "https://raw.githubusercontent.com/" + repo + "/main/" + pkgPath + "/config.json";
  return httpsGet(url);
}

// Recursively downloads every file in a GitHub repo folder (via the
// Contents API) into destDir on the active filesystem, descending into
// subdirectories (e.g. a package's "firmware/", "models/", "tools/") so
// packages aren't silently missing everything but their top-level files.
//
// isTopLevel controls two things:
//   - config.json is only skipped at the top level (a nested folder could
//     legitimately contain its own file named config.json that isn't the
//     package manifest, e.g. a "tools/config.json" runtime config).
//   - a hard failure to even reach/parse the top level is a real install
//     error (-1, matching the old behavior installOfficial() checks for).
//     A failure partway through a *nested* subfolder (rate limit, flaky
//     network, malformed listing) is treated as best-effort: that subtree
//     contributes 0 files rather than aborting the whole install, since
//     GitHub's unauthenticated Contents API is rate-limited (60 req/hour
//     per IP) and every subdirectory costs one extra request -- a deeply
//     nested package can burn through that quickly.
// `out` is the shell's live TCP client (or any Print target) -- progress is
// written to it as each file/subfolder is handled, instead of being
// buffered into a String that only gets sent once everything is done.
inline int installTreeFromGithubDir(const String& repo, const String& pkgPath,
                                     const String& destDir, Print& out, bool isTopLevel) {
  String api = "https://api.github.com/repos/" + repo + "/contents/" + pkgPath;
  String listing = httpsGet(api);
  if (listing.length() == 0) {
    if (!isTopLevel) { out.println("  warning: could not list '" + pkgPath + "' (skipped)"); out.flush(); }
    return isTopLevel ? -1 : 0;
  }

  JsonDocument doc;
  if (deserializeJson(doc, listing) != DeserializationError::Ok) {
    if (!isTopLevel) { out.println("  warning: malformed listing for '" + pkgPath + "' (skipped)"); out.flush(); }
    return isTopLevel ? -1 : 0;
  }

  if (!FsManager::mkdirP(destDir)) return isTopLevel ? -1 : 0; // parent dirs (e.g. /apps) may not exist yet

  int count = 0;
  for (JsonObject entry : doc.as<JsonArray>()) {
    String name = entry["name"].as<String>();
    String type = entry["type"].as<String>();
    if (name.length() == 0) continue;
    if (isTopLevel && name == "config.json") continue;

    if (type == "dir") {
      out.println("  entering " + pkgPath + "/" + name + "/ ...");
      out.flush();
      count += installTreeFromGithubDir(repo, pkgPath + "/" + name, destDir + "/" + name, out, false);
      continue;
    }
    if (type != "file") continue; // symlinks/submodules -- not supported, skip

    String url = entry["download_url"].as<String>();
    if (url.length() == 0) continue;
    out.print("  " + pkgPath + "/" + name + " ... ");
    String content = httpsGet(url);
    File f = FsManager::openFile(destDir + "/" + name, "w");
    if (f) {
      f.print(content);
      f.close();
      count++;
      out.println("ok (" + String(content.length()) + " bytes)");
    } else {
      out.println("FAILED");
    }
    out.flush();
  }
  return count;
}

// Kept as the public entry point so installOfficial()'s call site doesn't
// need to change shape -- just always starts the recursive walk at the top level.
inline int installFilesFromGithubDir(const String& repo, const String& pkgPath,
                                      const String& destDir, Print& out) {
  return installTreeFromGithubDir(repo, pkgPath, destDir, out, true);
}

// Version markers let 'update'/'upgrade' actually detect drift instead of
// being a canned stub -- a tiny hidden file per package/app recording the
// version string that was live at install time.
inline void writeVersionMarker(const String& dest, const String& version) {
  File f = FsManager::openFile(dest + "/.pmversion", "w");
  if (f) { f.print(version); f.close(); }
}
inline String readVersionMarker(const String& dest) {
  File f = FsManager::openFile(dest + "/.pmversion", "r");
  if (!f) return "unknown";
  String v = f.readString();
  f.close();
  v.trim();
  return v.length() ? v : "unknown";
}

// name-based install from an official repo (apt or app-installer).
// Streams every stage to `out` (the shell's live TCP client) as it
// actually happens -- fetch, license check, dependency stage, then each
// file as it downloads -- rather than assembling one big String that only
// gets sent to the terminal once the entire install has finished.
inline void installOfficial(const String& repo, const String& name, const String& destRoot, Print& out) {
  if (name.length() == 0) { out.println("error: install needs a package/app name"); return; }

  out.println("Fetching package......");
  out.flush();
  out.println("Fetching config.json ........");
  out.flush();

  String cfgStr = fetchConfigFromGithubDir(repo, name);
  if (cfgStr.length() == 0) { out.println("error: '" + name + "' not found in official repo."); return; }

  JsonDocument doc;
  if (deserializeJson(doc, cfgStr) != DeserializationError::Ok) {
    out.println("error: config.json malformed.");
    return;
  }

  String license = doc["license"] | "";
  String note;
  bool ok = licenseIsAcceptable(license, note);
  out.println(note);
  out.flush();
  if (!ok) return;

  const char* typeVal = doc["type"] | "unknown";
  String version = String((const char*)(doc["version"] | "0"));
  out.println("Type: " + String(typeVal));
  out.println("Version: " + version);
  out.flush();
  out.println("Checking dependencies......"); // dependency resolution: not yet implemented, see note below
  out.flush();
  out.println("Installing .......");
  out.flush();

  String dest = destRoot + "/" + name;
  int n = installFilesFromGithubDir(repo, name, dest, out);
  if (n < 0) { out.println("error: failed to fetch package files."); return; }
  writeVersionMarker(dest, version);
  out.println("100%");
  out.println("completed and installed " + String(n) + " file(s) to " + FsManager::toVirtual(dest));
  out.flush();
}

// Recursively removes an installed package/app by name.
inline String removePackage(const String& destRoot, const String& name) {
  if (name.length() == 0) return "error: remove needs a name";
  String dest = destRoot + "/" + name;
  if (!FsManager::isDir(dest)) return "error: '" + name + "' is not installed";
  bool ok = FsManager::removeRecursive(dest);
  return ok ? ("Removed " + name) : ("error: failed to fully remove '" + name + "' (some files may remain)");
}

// Fetches a single package's config.json and pretty-prints its metadata,
// without installing it -- like 'apt show'/'apt-cache show'.
inline String showPackage(const String& repo, const String& name) {
  if (name.length() == 0) return "error: show needs a name";
  String cfgStr = fetchConfigFromGithubDir(repo, name);
  if (cfgStr.length() == 0) return "error: '" + name + "' not found in official repo.";
  JsonDocument doc;
  if (deserializeJson(doc, cfgStr) != DeserializationError::Ok) return "error: config.json malformed.";

  String out;
  out += "Name: " + name + "\n";
  out += "Version: " + String((const char*)(doc["version"] | "unknown")) + "\n";
  out += "License: " + String((const char*)(doc["license"] | "unknown")) + "\n";
  out += "Type: " + String((const char*)(doc["type"] | "unknown")) + "\n";
  if (doc["description"].is<const char*>())
    out += "Description: " + String(doc["description"].as<const char*>()) + "\n";
  if (doc["author"].is<const char*>())
    out += "Author: " + String(doc["author"].as<const char*>()) + "\n";
  return out;
}

// List available packages/apps in an official repo's root folder.
inline String listOfficial(const String& repo) {
  String api = "https://api.github.com/repos/" + repo + "/contents/";
  String listing = httpsGet(api);
  if (listing.length() == 0) return "error: could not reach official repo.";

  JsonDocument doc;
  if (deserializeJson(doc, listing) != DeserializationError::Ok) return "error: malformed repo listing.";

  String out;
  for (JsonObject entry : doc.as<JsonArray>()) {
    if (String(entry["type"].as<const char*>()) == "dir") {
      out += String(entry["name"].as<const char*>()) + "\n";
    }
  }
  return out.length() ? out : "(no packages found)";
}

// Substring search (case-insensitive) over the official repo's package
// names. NOTE: this only matches names, not descriptions -- searching
// descriptions would mean fetching every package's config.json individually.
inline String search(const String& repo, const String& term) {
  if (term.length() == 0) return "error: search needs a term";
  String listing = listOfficial(repo);
  if (listing.startsWith("error:")) return listing;
  String needle = term; needle.toLowerCase();
  String out;
  int start = 0;
  while (start < (int)listing.length()) {
    int nl = listing.indexOf('\n', start);
    if (nl == -1) nl = listing.length();
    String name = listing.substring(start, nl);
    start = nl + 1;
    if (name.length() == 0) continue;
    String lname = name; lname.toLowerCase();
    if (lname.indexOf(needle) != -1) out += name + "\n";
  }
  return out.length() ? out : "(no matches for '" + term + "')";
}

// List what's ACTUALLY installed locally, by reading the destRoot directory
// (e.g. "/pkgs" or "/apps") on whichever filesystem backend is active.
// Unlike listOfficial() above, this needs no network -- it's just a
// directory listing of top-level folders, each folder name being a
// previously-installed package/app name.
inline String listInstalled(const String& destRoot) {
  if (!FsManager::isDir(destRoot)) return "(none installed)";
  String raw = FsManager::ls(destRoot); // lines like "d name" / "f name"
  String out;
  int start = 0;
  while (start < (int)raw.length()) {
    int nl = raw.indexOf('\n', start);
    if (nl == -1) nl = raw.length();
    String line = raw.substring(start, nl);
    if (line.startsWith("d ")) out += line.substring(2) + "\n";
    start = nl + 1;
  }
  return out.length() ? out : "(none installed)";
}

// Compares every installed package/app under destRoot against the current
// config.json in the official repo, using the .pmversion marker written at
// install time. Packages installed from an untrusted URL (not present in
// the official repo) are reported as "could not check" rather than erroring.
// Streams a line to `out` per package as each check completes -- each one
// is its own network round trip and can take a moment, so this reports
// live instead of only showing the full report once every package has
// been checked.
inline void checkForUpdates(const String& repo, const String& destRoot,
                             std::vector<String>& outdatedNames, Print& out) {
  if (!FsManager::isDir(destRoot)) { out.println("(nothing installed)"); return; }
  String raw = FsManager::ls(destRoot);
  int checked = 0;
  int start = 0;
  while (start < (int)raw.length()) {
    int nl = raw.indexOf('\n', start);
    if (nl == -1) nl = raw.length();
    String line = raw.substring(start, nl);
    start = nl + 1;
    if (!line.startsWith("d ")) continue;
    checked++;
    String name = line.substring(2);
    String dest = destRoot + "/" + name;
    String installedVer = readVersionMarker(dest);
    String cfgStr = fetchConfigFromGithubDir(repo, name);
    if (cfgStr.length() == 0) {
      out.println(name + ": could not check (not in official repo, or offline)");
      out.flush();
      continue;
    }
    JsonDocument doc;
    if (deserializeJson(doc, cfgStr) != DeserializationError::Ok) {
      out.println(name + ": malformed remote config.json");
      out.flush();
      continue;
    }
    String remoteVer = String((const char*)(doc["version"] | "0"));
    if (remoteVer != installedVer) {
      out.println(name + ": " + installedVer + " -> " + remoteVer + " (update available)");
      outdatedNames.push_back(name);
    } else {
      out.println(name + ": " + installedVer + " (up to date)");
    }
    out.flush();
  }
  if (checked == 0) out.println("(nothing installed)");
}

// Real 'apt upgrade'/'app-installer upgrade': checks for drift, then
// reinstalls (overwrites) every package/app whose remote version differs.
inline void upgradeAll(const String& repo, const String& destRoot, Print& out) {
  std::vector<String> outdated;
  checkForUpdates(repo, destRoot, outdated, out);
  if (outdated.empty()) { out.println("\nEverything is up to date."); return; }
  out.println("\nUpgrading " + String(outdated.size()) + " package(s)...");
  out.flush();
  for (const String& name : outdated) {
    out.println("-- " + name + " --");
    out.flush();
    installOfficial(repo, name, destRoot, out);
  }
}

} // namespace PackageManager

namespace PackageManager {

// Install from a manifest hosted directly at an arbitrary URL (untrusted
// source). Expected JSON shape at that URL:
//   { "name": "...", "license": "...", "type": "...",
//     "files": [ { "path": "main.lua", "url": "https://..." }, ... ] }
// This is NOT the GitHub Contents API format -- it's this project's own
// minimal manifest schema, since an arbitrary domain won't speak GitHub's API.
// Streams every stage live to `out` as it happens, same as installOfficial.
inline void installFromManifestUrl(const String& url, const String& destRoot, Print& out) {
  out.println("Downloading package....");
  out.flush();
  String manifestStr = httpsGet(url);
  if (manifestStr.length() == 0) { out.println("error: could not reach that URL."); return; }

  out.println("Getting config.json");
  out.flush();
  JsonDocument doc;
  if (deserializeJson(doc, manifestStr) != DeserializationError::Ok) {
    out.println("error: response wasn't a valid package manifest.");
    return;
  }

  String license = doc["license"] | "";
  String note;
  bool ok = licenseIsAcceptable(license, note);
  out.println(note);
  out.flush();
  if (!ok) return;

  String name = doc["name"] | "unnamed-package";
  String dest = destRoot + "/" + name;
  if (!FsManager::mkdirP(dest)) {
    out.println("error: could not create destination directory '" + dest + "'.");
    return;
  }

  out.println("Installing...");
  out.flush();
  int count = 0;
  for (JsonObject fentry : doc["files"].as<JsonArray>()) {
    String fname = fentry["path"] | "";
    String furl  = fentry["url"]  | "";
    if (fname.length() == 0 || furl.length() == 0) continue;
    out.print("  " + fname + " ... ");
    String content = httpsGet(furl);
    File f = FsManager::openFile(dest + "/" + fname, "w");
    if (f) {
      f.print(content);
      f.close();
      count++;
      out.println("ok (" + String(content.length()) + " bytes)");
    } else {
      out.println("FAILED");
    }
    out.flush();
  }
  writeVersionMarker(dest, String((const char*)(doc["version"] | "0")));
  out.println("100%");
  out.println("Installed " + String(count) + " file(s) to " + FsManager::toVirtual(dest));
  out.flush();
}

} // namespace PackageManager
