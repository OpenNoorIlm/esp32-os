#pragma once
#include <WiFi.h>
#include <Preferences.h>
#include <mbedtls/sha256.h>
#include <mbedtls/md.h>
#include "capability.h"
#include "fs_manager.h"
#include "wifi_manager.h"
#include "robot_api.h"
#include "package_manager.h"
#include "os_manager.h"
#include "lua_engine.h"
#include <vector>

#define SHELL_PORT 2222

// NoorShell: a password-authenticated command shell over TCP on port 2222.
// NOTE: the password itself is never transmitted -- login uses a
// challenge/response HMAC over a random nonce, checked against the stored
// SHA256 hash. Full transport encryption (TLS) is a planned follow-up once
// this command layer is validated; for now, command/response text after
// login is sent in the clear.
namespace ShellServer {

inline WiFiServer server(SHELL_PORT);
inline Preferences shellPrefs;

inline String sha256hex(const String& in) {
  unsigned char hash[32];
  mbedtls_sha256((const unsigned char*)in.c_str(), in.length(), hash, 0);
  String out; char buf[3];
  for (int i = 0; i < 32; i++) { sprintf(buf, "%02x", hash[i]); out += buf; }
  return out;
}

inline String hmacHex(const String& key, const String& msg) {
  unsigned char out[32];
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_hmac(info, (const unsigned char*)key.c_str(), key.length(),
                   (const unsigned char*)msg.c_str(), msg.length(), out);
  String hex; char buf[3];
  for (int i = 0; i < 32; i++) { sprintf(buf, "%02x", out[i]); hex += buf; }
  return hex;
}

} // namespace ShellServer

namespace ShellServer {

// Shell-style tokenizer: splits on whitespace but keeps 'single' and
// "double" quoted spans (quotes stripped) together as one token. Needed
// for real curl usage, e.g.:
//   curl -H "Content-Type: application/json" -d '{"a":1}' http://host/x
inline std::vector<String> tokenize(const String& in) {
  std::vector<String> toks;
  int i = 0, n = in.length();
  while (i < n) {
    while (i < n && in[i] == ' ') i++;
    if (i >= n) break;
    String tok;
    if (in[i] == '"' || in[i] == '\'') {
      char q = in[i]; i++;
      while (i < n && in[i] != q) { tok += in[i]; i++; }
      if (i < n) i++; // skip closing quote
    } else {
      while (i < n && in[i] != ' ') { tok += in[i]; i++; }
    }
    toks.push_back(tok);
  }
  return toks;
}

} // namespace ShellServer

namespace ShellServer {

inline bool hasPassword() {
  shellPrefs.begin("shell", true);
  bool has = shellPrefs.isKey("passhash");
  shellPrefs.end();
  return has;
}

inline String getPassHash() {
  shellPrefs.begin("shell", true);
  String h = shellPrefs.getString("passhash", "");
  shellPrefs.end();
  return h;
}

inline void setPassword(const String& newPass) {
  shellPrefs.begin("shell", false);
  shellPrefs.putString("passhash", sha256hex(newPass));
  shellPrefs.end();
}

inline void begin() {
  server.begin();
  Serial.println("NoorShell listening on port " + String(SHELL_PORT));
}

inline String randomNonce() {
  String n;
  for (int i = 0; i < 16; i++) n += String(random(0, 16), HEX);
  return n;
}

} // namespace ShellServer

namespace ShellServer {

// cwd is stored as a REAL path (e.g. "/apps"); shown to the user as VIRTUAL
// ("/storage/esp32/apps") via FsManager::toVirtual().
// `out` is the live TCP client -- passed through so long-running commands
// (install/update/upgrade) can stream progress as it happens instead of
// building one big String that only gets sent once they finish.
inline String runCommand(const String& cmdLine, String& cwd, Print& out) {
  String line = cmdLine;
  line.trim();
  if (line.length() == 0) return "";

  int sp = line.indexOf(' ');
  String cmd  = sp == -1 ? line : line.substring(0, sp);
  String rest = sp == -1 ? ""   : line.substring(sp + 1);

  if (cmd == "pwd") return FsManager::toVirtual(cwd);

  if (cmd == "ls") {
    String target = rest.length() ? FsManager::toRealPath(cwd, rest) : cwd;
    return FsManager::ls(target);
  }

  if (cmd == "cd") {
    String target = FsManager::toRealPath(cwd, rest.length() ? rest : "/");
    if (!FsManager::isDir(target)) return "error: no such directory";
    cwd = target;
    return "";
  }

  if (cmd == "mkdir") {
    if (rest.length() == 0) return "error: mkdir needs a name";
    return FsManager::mkdir(FsManager::toRealPath(cwd, rest)) ? "ok" : "error: mkdir failed";
  }

  if (cmd == "rm") {
    if (rest.length() == 0) return "error: rm needs a path";
    return FsManager::remove(FsManager::toRealPath(cwd, rest)) ? "ok" : "error: rm failed";
  }

  if (cmd == "cat") {
    if (rest.length() == 0) return "error: cat needs a path";
    return FsManager::cat(FsManager::toRealPath(cwd, rest));
  }

  if (cmd == "sysinfo") {
    String out = capabilitiesReport();
    out += "\nInstalled OS packages (/pkgs):\n" + PackageManager::listInstalled("/pkgs");
    out += "\nInstalled apps (/apps):\n" + PackageManager::listInstalled("/apps");
    return out;
  }
  if (cmd == "robot")   return RobotApi::shellCommand(rest);
  if (cmd == "lua") {
    if (rest.length() == 0) return "error: lua needs code, e.g. lua print(1+1)";
    return LuaEngine::eval(rest);
  }

  if (cmd == "wifi") {
    return "IP: " + WiFi.localIP().toString() + "\nSSID: " + WiFi.SSID() +
           "\n(to change network: hold BOOT button and power-cycle)";
  }

  if (cmd == "ip") return WiFi.localIP().toString();

  if (cmd == "df") return FsManager::df();

  if (cmd == "storage") {
    if (rest == "--change") return "ASK_STORAGE_CHANGE";
    return FsManager::df(); // bare "storage" just shows current status
  }

  if (cmd == "restart") return "DO_RESTART";

  if (cmd == "os") {
    int sp2 = rest.indexOf(' ');
    String sub = sp2 == -1 ? rest : rest.substring(0, sp2);

    if (sub.length() == 0 || sub == "version") return "Running firmware version: " + String(FIRMWARE_VERSION);

    if (sub == "check") {
      String v, u;
      return OsManager::checkForOsUpdate(v, u);
    }

    // Real self-update: fetches the compiled firmware from the esp32-os
    // repo and flashes it, streaming progress live via `out`. On success
    // this restarts the board itself and never returns; on failure or if
    // already current it returns a status line for the shell to print.
    if (sub == "update") return OsManager::performUpdate(out);

    return "error: unknown os subcommand '" + sub + "' (try: version, check, update)";
  }

  // Splits "<url> | <tag>" into url + tag; used by apt/curl untrusted-source installs.
  auto splitPipe = [](const String& in, String& left, String& right) -> bool {
    int idx = in.indexOf(" | ");
    if (idx == -1) return false;
    left = in.substring(0, idx); left.trim();
    right = in.substring(idx + 3); right.trim();
    return true;
  };

  if (cmd == "apt") {
    int sp2 = rest.indexOf(' ');
    String sub = sp2 == -1 ? rest : rest.substring(0, sp2);
    String arg = sp2 == -1 ? ""   : rest.substring(sp2 + 1);

    if (sub == "list") {
      if (arg == "--installed") return PackageManager::listInstalled("/pkgs");
      return PackageManager::listOfficial(APT_REPO);
    }

    if (sub == "update") {
      // Real check: compares each installed package's .pmversion marker
      // against the current config.json in the official repo. Streams a
      // line per package live as out (each is its own network round trip).
      std::vector<String> outdated;
      PackageManager::checkForUpdates(APT_REPO, "/pkgs", outdated, out);
      if (outdated.empty()) out.println("\nAll packages are up to date.");
      else out.println("\n" + String(outdated.size()) +
                        " package(s) can be upgraded. Run 'apt upgrade' to install them.");
      return "";
    }

    if (sub == "upgrade") {
      // Real upgrade: re-runs update-check, then reinstalls every package
      // whose remote version differs from what's on disk. Streams live.
      PackageManager::upgradeAll(APT_REPO, "/pkgs", out);
      return "";
    }

    if (sub == "show") {
      if (arg.length() == 0) return "error: apt show needs a package name";
      return PackageManager::showPackage(APT_REPO, arg);
    }

    if (sub == "search") {
      if (arg.length() == 0) return "error: apt search needs a term";
      return PackageManager::search(APT_REPO, arg);
    }

    if (sub == "remove" || sub == "uninstall") {
      if (arg.length() == 0) return "error: apt " + sub + " needs a package name";
      return PackageManager::removePackage("/pkgs", arg);
    }

    if (sub == "install") {
      String url, tag;
      if (splitPipe(arg, url, tag) && tag == "package")
        return "CONFIRM_UNTRUSTED:pkg:" + url;
      PackageManager::installOfficial(APT_REPO, arg, "/pkgs", out); // streams progress live
      return "";
    }
    return "error: unknown apt subcommand '" + sub + "' (try: list, show, search, install, "
           "remove, update, upgrade)";
  }

  if (cmd == "app-installer") {
    int sp2 = rest.indexOf(' ');
    String sub = sp2 == -1 ? rest : rest.substring(0, sp2);
    String arg = sp2 == -1 ? ""   : rest.substring(sp2 + 1);

    if (sub == "list") {
      if (arg == "--installed") return PackageManager::listInstalled("/apps");
      return PackageManager::listOfficial(APP_REPO);
    }

    if (sub == "update") {
      std::vector<String> outdated;
      PackageManager::checkForUpdates(APP_REPO, "/apps", outdated, out);
      if (outdated.empty()) out.println("\nAll apps are up to date.");
      else out.println("\n" + String(outdated.size()) +
                        " app(s) can be upgraded. Run 'app-installer upgrade' to install them.");
      return "";
    }

    if (sub == "upgrade") {
      PackageManager::upgradeAll(APP_REPO, "/apps", out);
      return "";
    }

    if (sub == "show") {
      if (arg.length() == 0) return "error: app-installer show needs an app name";
      return PackageManager::showPackage(APP_REPO, arg);
    }

    if (sub == "search") {
      if (arg.length() == 0) return "error: app-installer search needs a term";
      return PackageManager::search(APP_REPO, arg);
    }

    if (sub == "remove" || sub == "uninstall") {
      if (arg.length() == 0) return "error: app-installer " + sub + " needs an app name";
      return PackageManager::removePackage("/apps", arg);
    }

    if (sub == "install") {
      PackageManager::installOfficial(APP_REPO, arg, "/apps", out); // streams progress live
      return "";
    }
    return "error: unknown app-installer subcommand '" + sub + "' (try: list, show, search, "
           "install, remove, update, upgrade)";
  }

  if (cmd == "curl") {
    // Backward-compatible install shorthand stays as-is: a bare
    // "<url> | app" / "<url> | package" (no other flags) still routes into
    // the untrusted-install confirmation flow instead of firing a literal
    // HTTP request.
    {
      String url0, tag0;
      if (splitPipe(rest, url0, tag0) && (tag0 == "app" || tag0 == "package") &&
          url0.indexOf(' ') == -1) {
        return "CONFIRM_UNTRUSTED:" + String(tag0 == "app" ? "app" : "pkg") + ":" + url0;
      }
    }

    // Real curl-style flag parsing for everything else.
    std::vector<String> toks = tokenize(rest);
    if (toks.empty())
      return "curl: try 'curl <url>' or 'curl --help'";
    if (toks.size() == 1 && toks[0] == "--help") {
      return "Usage: curl [options] <url>\n"
             "  -X, --request <METHOD>   HTTP method (GET, POST, PUT, PATCH, DELETE)\n"
             "  -H, --header <\"K: V\">    add a request header (repeatable)\n"
             "  -d, --data <body>        send body data (implies POST if -X not given)\n"
             "  -A, --user-agent <UA>    set User-Agent\n"
             "  -I, --head               HEAD request only, no body\n"
             "  -L, --location           follow redirects\n"
             "  -s, --silent             suppress the status/header summary\n"
             "  -o, --output <path>      write response body to a file instead of stdout\n"
             "  curl <url> | app|package installs from an untrusted manifest URL";
    }

    String method, data, userAgent, outFile, url;
    std::vector<String> headers;
    bool headOnly = false, follow = false, silent = false, methodSet = false;
    String parseErr;

    for (size_t i = 0; i < toks.size() && parseErr.length() == 0; i++) {
      String t = toks[i];
      auto next = [&](String& dest) -> bool {
        if (i + 1 >= toks.size()) return false;
        dest = toks[++i];
        return true;
      };
      if (t == "-X" || t == "--request") {
        if (!next(method)) { parseErr = "curl: option requires an argument -- 'X'"; break; }
        methodSet = true;
      } else if (t == "-H" || t == "--header") {
        String h;
        if (!next(h)) { parseErr = "curl: option requires an argument -- 'H'"; break; }
        headers.push_back(h);
      } else if (t == "-d" || t == "--data" || t == "--data-raw") {
        if (!next(data)) { parseErr = "curl: option requires an argument -- 'd'"; break; }
        if (!methodSet) { method = "POST"; methodSet = true; }
      } else if (t == "-A" || t == "--user-agent") {
        if (!next(userAgent)) { parseErr = "curl: option requires an argument -- 'A'"; break; }
      } else if (t == "-o" || t == "--output") {
        if (!next(outFile)) { parseErr = "curl: option requires an argument -- 'o'"; break; }
      } else if (t == "-I" || t == "--head") {
        headOnly = true;
      } else if (t == "-L" || t == "--location") {
        follow = true;
      } else if (t == "-s" || t == "--silent") {
        silent = true;
      } else if (t == "-G" || t == "--get") {
        method = "GET"; methodSet = true;
      } else if (t.length() && t[0] != '-') {
        url = t;
      } else {
        parseErr = "curl: unknown option '" + t + "' (see curl --help)";
      }
    }
    if (parseErr.length()) return parseErr;
    if (url.length() == 0) return "curl: no URL specified! (see curl --help)";
    if (!methodSet) method = "GET";

    String statusOut;
    String body = PackageManager::curlRequest(url, method, headers, data, follow,
                                               headOnly, silent, userAgent, statusOut);

    if (body.startsWith("curl: (")) return body; // transport-level failure

    if (outFile.length()) {
      File f = FsManager::openFile(FsManager::toRealPath(cwd, outFile), "w");
      if (!f) return statusOut + "curl: (23) failed to write output to '" + outFile + "'";
      f.print(body);
      f.close();
      return statusOut + (silent ? "" : ("\nSaved " + String(body.length()) +
                           " bytes to " + outFile));
    }

    return statusOut + body;
  }

  if (cmd == "password" && rest == "--set-password") return "USE_SETPASS_FLOW";

  if (cmd == "help") {
    return "Commands: pwd, ls [path], cd <path>, mkdir <name>, rm <path>, "
           "cat <path>, sysinfo, wifi, ip, df, storage [--change], restart, "
           "os [version|check|update], robot <sub>, "
           "apt list [--installed]|show <name>|search <term>|install <name>|"
           "install <url> | package|remove <name>|update|upgrade, "
           "app-installer list [--installed]|show <name>|search <term>|install <name>|"
           "remove <name>|update|upgrade, "
           "curl [-X <method>] [-H \"K: V\"] [-d <body>] [-A <ua>] [-I] [-L] [-s] "
           "[-o <file>] <url>  (or: curl <url> | app|package to install), "
           "lua <code>, "
           "password --set-password, exit";
  }

  if (cmd == "exit") return "BYE";

  return "error: unknown command '" + cmd + "'";
}

} // namespace ShellServer

namespace ShellServer {

inline void handleClient(WiFiClient& client) {
  client.println("NOOR-SHELL v0.1");

  // Default Stream timeout is 1000ms, which is nowhere near enough time
  // for a human to type a password over an interactive client. Give the
  // login phase (SETPASS / AUTH RESPONSE reads below) a full 60s instead.
  // The command loop further down doesn't rely on this -- it only calls
  // readStringUntil() after client.available() is already true, so it
  // isn't affected by this longer timeout sitting around unused.
  client.setTimeout(60000);

  if (!hasPassword()) {
    client.println("AUTH_REQUIRED none (first-time setup)");
    client.println("Send: SETPASS <newpassword>");
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.startsWith("SETPASS ")) {
      setPassword(line.substring(8));
      client.println("Password set. Reconnect to log in.");
    } else {
      client.println("Expected SETPASS. Closing.");
    }
    client.stop();
    return;
  }

  String nonce = randomNonce();
  client.println("AUTH CHALLENGE " + nonce);
  String resp = client.readStringUntil('\n');
  resp.trim();

  if (resp != "AUTH RESPONSE " + hmacHex(getPassHash(), nonce)) {
    client.println("AUTH FAIL");
    client.stop();
    return;
  }
  client.println("AUTH OK");

  String cwd = "/";
  client.println("PROMPT " + FsManager::toVirtual(cwd) + " > ");
  while (client.connected()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      String result = runCommand(line, cwd, client); // client is a live Print& for streaming
      if (result == "BYE") { client.println("bye!"); break; }
      if (result == "DO_RESTART") {
        client.println("Restarting......");
        client.flush();
        delay(300);
        client.stop();
        delay(200);
        ESP.restart();
      }
      if (result == "USE_SETPASS_FLOW") {
        client.println("Send: SETPASS <newpassword>");
        String p = client.readStringUntil('\n');
        p.trim();
        if (p.startsWith("SETPASS ")) {
          setPassword(p.substring(8));
          client.println("Password updated.");
        }
      } else if (result == "ASK_STORAGE_CHANGE") {
        // "ASK " is a generic one-line question/answer protocol: the client
        // prints everything after "ASK " (no newline), reads one line of
        // plain-text input, and sends it back. Used here and for the
        // untrusted-source [Y/n] gate below.
        client.println("ASK Choose storage backend: [1] Built-in flash (LittleFS)  [2] SD card - Select [1/2]: ");
        String choice = client.readStringUntil('\n');
        choice.trim();
        client.println(FsManager::changeStorage(choice));
      } else if (result.startsWith("CONFIRM_UNTRUSTED:")) {
        // Format: CONFIRM_UNTRUSTED:<pkg|app>:<url>
        String rem = result.substring(strlen("CONFIRM_UNTRUSTED:"));
        int c = rem.indexOf(':');
        String kind = rem.substring(0, c);
        String url  = rem.substring(c + 1);
        client.println("ASK That is not an official repo for packages and can contain "
                        "viruses would you like to proceed? [Y/n]: ");
        String answer = client.readStringUntil('\n');
        answer.trim();
        if (answer.equalsIgnoreCase("y") || answer.equalsIgnoreCase("yes")) {
          String destRoot = (kind == "app") ? "/apps" : "/pkgs";
          PackageManager::installFromManifestUrl(url, destRoot, client); // streams progress live
        } else {
          client.println("Aborted.");
        }
      } else if (result.length()) {
        client.println(result);
      }
      client.println("PROMPT " + FsManager::toVirtual(cwd) + " > ");
    }
    delay(2);
  }
  client.stop();
}

inline void loop() {
  WiFiClient client = server.available();
  if (client) handleClient(client); // one session at a time for now
}

} // namespace ShellServer
