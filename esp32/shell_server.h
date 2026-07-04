#pragma once
#include <WiFi.h>
#include <Preferences.h>
#include <mbedtls/sha256.h>
#include <mbedtls/md.h>
#include "capability.h"
#include "fs_manager.h"
#include "wifi_manager.h"
#include "robot_api.h"

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
inline String runCommand(const String& cmdLine, String& cwd) {
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

  if (cmd == "sysinfo") return capabilitiesReport();
  if (cmd == "robot")   return RobotApi::shellCommand(rest);

  if (cmd == "wifi") {
    return "IP: " + WiFi.localIP().toString() + "\nSSID: " + WiFi.SSID() +
           "\n(to change network: hold BOOT button and power-cycle)";
  }

  if (cmd == "ip") return WiFi.localIP().toString();

  if (cmd == "df" || cmd == "storage") return FsManager::df();

  if (cmd == "password" && rest == "--set-password") return "USE_SETPASS_FLOW";

  if (cmd == "help") {
    return "Commands: pwd, ls [path], cd <path>, mkdir <name>, rm <path>, "
           "cat <path>, sysinfo, wifi, ip, df, robot <sub>, password --set-password, exit";
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
      String result = runCommand(line, cwd);
      if (result == "BYE") { client.println("bye!"); break; }
      if (result == "USE_SETPASS_FLOW") {
        client.println("Send: SETPASS <newpassword>");
        String p = client.readStringUntil('\n');
        p.trim();
        if (p.startsWith("SETPASS ")) {
          setPassword(p.substring(8));
          client.println("Password updated.");
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
