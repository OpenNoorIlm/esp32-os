#pragma once
// ── apps/browser.h ────────────────────────────────────────────────────────────
// NoorBrowser — lightweight text web browser for NoorRobot
//
// Shell usage:
//   browse <url>          — fetch and display page as plain text
//   browse                — opens TFT browser with keyboard URL bar
//
// TFT: Address bar at top, scrollable text content, Back/Refresh buttons
//
// How it works:
//   - Fetches URL via HTTPClient
//   - Strips HTML tags to get readable text
//   - Renders text on TFT in scrollable pages
//   - Links are extracted so user can navigate
// ─────────────────────────────────────────────────────────────────────────────

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "../tft_manager.h"
#include "../vkeyboard.h"

namespace Browser {

#define BROWSER_MAX_PAGE 65536  // max page size in bytes
#define BROWSER_LINE_W   38     // chars per line at textSize 1
#define BROWSER_LINES    20     // visible lines per screen page

// ── Strip HTML to plain text ──────────────────────────────────────────────────
String stripHtml(const String& html) {
  String out;
  bool inTag    = false;
  bool inScript = false;
  bool inStyle  = false;
  String tag;

  for (int i = 0; i < (int)html.length(); i++) {
    char c = html[i];

    if (inTag) {
      tag += tolower(c);
      if (c == '>') {
        inTag = false;
        // Block tags → newline
        if (tag.startsWith("/p") || tag.startsWith("/div") ||
            tag.startsWith("/h") || tag.startsWith("br") ||
            tag.startsWith("/li") || tag.startsWith("/tr")) {
          out += '\n';
        }
        if (tag.startsWith("script") || tag.startsWith("/script")) inScript = !inScript;
        if (tag.startsWith("style")  || tag.startsWith("/style"))  inStyle  = !inStyle;
        tag = "";
      }
      continue;
    }

    if (c == '<') { inTag = true; tag = ""; continue; }
    if (inScript || inStyle) continue;

    // Decode basic HTML entities
    if (c == '&') {
      String entity;
      int j = i + 1;
      while (j < (int)html.length() && html[j] != ';' && j < i + 8) entity += html[j++];
      if (entity == "amp")  { out += '&'; i = j; continue; }
      if (entity == "lt")   { out += '<'; i = j; continue; }
      if (entity == "gt")   { out += '>'; i = j; continue; }
      if (entity == "nbsp") { out += ' '; i = j; continue; }
      if (entity == "quot") { out += '"'; i = j; continue; }
    }

    // Skip non-printable except newline
    if (c == '\n' || c == '\r') { if (out.length() > 0 && out[out.length()-1] != '\n') out += '\n'; continue; }
    if (c < 32) continue;

    out += c;
  }

  // Collapse multiple blank lines
  String clean;
  int blankCount = 0;
  for (int i = 0; i < (int)out.length(); i++) {
    if (out[i] == '\n') {
      blankCount++;
      if (blankCount <= 2) clean += '\n';
    } else {
      blankCount = 0;
      clean += out[i];
    }
  }
  return clean;
}

// ── HTTP fetch ────────────────────────────────────────────────────────────────
String fetchUrl(const String& url) {
  if (WiFi.status() != WL_CONNECTED) return "ERROR: No WiFi connection";

  HTTPClient http;
  bool isHttps = url.startsWith("https://");

  if (isHttps) {
    WiFiClientSecure client;
    client.setInsecure(); // skip cert validation for hobby use
    http.begin(client, url);
  } else {
    http.begin(url);
  }

  http.setUserAgent("NoorBot/1.0 (ESP32 Robot Browser)");
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);
  http.addHeader("Accept", "text/html,text/plain");

  int code = http.GET();
  if (code <= 0) { http.end(); return "ERROR: Connection failed (" + String(code) + ")"; }
  if (code != 200) { http.end(); return "ERROR: HTTP " + String(code); }

  String contentType = http.header("Content-Type");
  String body = http.getString();
  http.end();

  if (body.length() > BROWSER_MAX_PAGE) body = body.substring(0, BROWSER_MAX_PAGE);

  if (contentType.indexOf("text/html") >= 0) return stripHtml(body);
  return body; // plain text — return as-is
}

// ── Word wrap for shell output ────────────────────────────────────────────────
String wrapText(const String& text, int cols = 72) {
  String out;
  int lineLen = 0;
  for (int i = 0; i < (int)text.length(); i++) {
    char c = text[i];
    if (c == '\n') { out += '\n'; lineLen = 0; continue; }
    out += c;
    lineLen++;
    if (lineLen >= cols && c == ' ') { out += '\n'; lineLen = 0; }
  }
  return out;
}

// ── Shell command handler ─────────────────────────────────────────────────────
String shellCmd(const String& args) {
  String url = args;
  url.trim();
  if (url.isEmpty()) return "Usage: browse <url>\nExample: browse http://example.com\n";
  if (!url.startsWith("http://") && !url.startsWith("https://")) url = "http://" + url;

  String body = fetchUrl(url);
  return wrapText(body) + "\n";
}

// ── TFT App state ─────────────────────────────────────────────────────────────
String _currentUrl     = "";
String _pageText       = "";
int    _scrollLine     = 0;
std::vector<String> _lines;

void buildLines(const String& text) {
  _lines.clear();
  String word = "";
  String line = "";

  for (int i = 0; i <= (int)text.length(); i++) {
    char c = i < (int)text.length() ? text[i] : '\n';

    if (c == '\n') {
      if (line.length() + word.length() <= BROWSER_LINE_W) line += word;
      _lines.push_back(line);
      line = ""; word = "";
    } else if (c == ' ') {
      if ((int)(line.length() + word.length()) > BROWSER_LINE_W) {
        _lines.push_back(line);
        line = word + " ";
      } else {
        line += word + " ";
      }
      word = "";
    } else {
      word += c;
    }
  }
}

void tftDrawAddressBar() {
  tft.fillRect(0, 0, 240, 30, 0x0821);
  tft.drawFastHLine(0, 30, 240, 0x07FF);
  // Back button
  tft.fillRoundRect(2, 3, 24, 22, 4, 0x4208);
  tft.setTextColor(0xFFFF, 0x4208);
  tft.setTextSize(1);
  tft.setCursor(7, 11);
  tft.print("<");
  // Refresh button
  tft.fillRoundRect(28, 3, 24, 22, 4, 0x4208);
  tft.setTextColor(0x07E0, 0x4208);
  tft.setCursor(33, 11);
  tft.print("R");
  // URL bar
  tft.fillRoundRect(56, 3, 180, 22, 4, 0x0000);
  tft.drawRoundRect(56, 3, 180, 22, 4, 0x07FF);
  tft.setTextColor(0x07FF, 0x0000);
  tft.setTextSize(1);
  tft.setCursor(60, 11);
  String displayUrl = _currentUrl;
  if (displayUrl.length() > 28) displayUrl = displayUrl.substring(0, 25) + "...";
  tft.print(displayUrl.isEmpty() ? "Tap to enter URL..." : displayUrl);
}

void tftDrawContent() {
  tft.fillRect(0, 32, 240, 270, 0x0000);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.setTextSize(1);

  int visibleLines = BROWSER_LINES;
  for (int i = 0; i < visibleLines && (_scrollLine + i) < (int)_lines.size(); i++) {
    tft.setCursor(2, 34 + i * 12);
    tft.print(_lines[_scrollLine + i]);
  }

  // Scroll indicator
  if (_lines.size() > visibleLines) {
    int totalH  = 238;
    int barH    = max(10, (int)(totalH * visibleLines / _lines.size()));
    int barY    = 32 + (int)(totalH * _scrollLine / _lines.size());
    tft.fillRect(236, 32, 4, totalH, 0x2104);
    tft.fillRect(236, barY, 4, barH, 0x07FF);
  }
}

void tftDrawStatusBar() {
  tft.fillRect(0, 302, 240, 18, 0x0821);
  tft.setTextColor(0x4208, 0x0821);
  tft.setTextSize(1);
  tft.setCursor(4, 307);
  tft.print("NoorBrowser | Lines: " + String(_lines.size()));
}

void tftLoadPage(const String& url) {
  _currentUrl = url;
  tftDrawAddressBar();
  tft.fillRect(0, 32, 240, 270, 0x0000);
  tft.setTextColor(0x07FF, 0x0000);
  tft.setTextSize(1);
  tft.setCursor(20, 150);
  tft.print("Loading...");
  tft.setCursor(20, 166);
  tft.print(url.substring(0, 30));

  _pageText   = fetchUrl(url);
  _scrollLine = 0;
  buildLines(_pageText);
  tftDrawAddressBar();
  tftDrawContent();
  tftDrawStatusBar();
}

// ── TFT App loop ──────────────────────────────────────────────────────────────
bool tftRun() {
  tft.fillScreen(0x0000);
  _currentUrl = "";
  _scrollLine = 0;
  _lines.clear();
  tftDrawAddressBar();
  tft.setTextColor(0x4208, 0x0000);
  tft.setTextSize(1);
  tft.setCursor(20, 180);
  tft.print("Tap the address bar to browse");

  int swipeStartY = -1;

  while (true) {
    if (touch.tirqTouched() && touch.touched()) {
      TS_Point p = touch.getPoint();
      int tx = map(p.x, 200, 3800, 0, 240);
      int ty = map(p.y, 200, 3800, 0, 320);
      swipeStartY = ty;
      delay(100);

      // Address bar tap
      if (ty >= 3 && ty <= 27 && tx >= 56) {
        String url = VKeyboard::open("Enter URL:", _currentUrl);
        url.trim();
        if (!url.isEmpty()) {
          if (!url.startsWith("http://") && !url.startsWith("https://")) url = "http://" + url;
          tft.fillScreen(0x0000);
          tftLoadPage(url);
        }
        continue;
      }

      // Back button
      if (ty >= 3 && ty <= 27 && tx >= 2 && tx <= 26) {
        return false; // exit app
      }

      // Refresh button
      if (ty >= 3 && ty <= 27 && tx >= 28 && tx <= 54) {
        if (!_currentUrl.isEmpty()) tftLoadPage(_currentUrl);
        continue;
      }

      // Wait for finger up to detect swipe
      delay(200);
      if (touch.touched()) {
        TS_Point p2 = touch.getPoint();
        int ty2 = map(p2.y, 200, 3800, 0, 320);
        int dy = swipeStartY - ty2;

        if (dy > 30) {
          // Swipe up = scroll down
          _scrollLine = min((int)_lines.size() - BROWSER_LINES, _scrollLine + 5);
          tftDrawContent();
        } else if (dy < -30) {
          // Swipe down = scroll up
          _scrollLine = max(0, _scrollLine - 5);
          tftDrawContent();
        }
      }
    }
    delay(20);
  }
}

} // namespace Browser
