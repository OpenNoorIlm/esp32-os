#pragma once
// ── nano_editor.h ─────────────────────────────────────────────────────────────
// NoorOS text editors
//
// 1. nano  — SSH terminal editor (line-based, feels like real nano)
//    Commands: ^S save  ^X exit  ^K cut  ^U paste  ^G help  ^F find
//
// 2. tedit — TFT visual file editor (uses virtual keyboard)
//    Full visual editing on screen, line-by-line with scroll, save/exit buttons
//
// Shell:
//   nano  <file>    — SSH nano editor (streams via shell connection)
//   tedit <file>    — TFT visual editor
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <SPIFFS.h>
#include "fs_manager.h"
#include "vkeyboard.h"

#include <TFT_eSPI.h>
extern TFT_eSPI tft;
extern XPT2046_Touchscreen touch;

namespace NanoEditor {

// ── File helpers ──────────────────────────────────────────────────────────────
String readFile(const String& path) {
  if (!SPIFFS.exists(path)) return "";
  File f = SPIFFS.open(path, "r");
  if (!f) return "";
  String s = f.readString();
  f.close();
  return s;
}

bool writeFile(const String& path, const String& content) {
  File f = SPIFFS.open(path, "w");
  if (!f) return false;
  f.print(content);
  f.close();
  return true;
}

// ── Split content into lines ──────────────────────────────────────────────────
std::vector<String> splitLines(const String& text) {
  std::vector<String> lines;
  String cur;
  for (int i = 0; i < (int)text.length(); i++) {
    if (text[i] == '\n') { lines.push_back(cur); cur = ""; }
    else cur += text[i];
  }
  lines.push_back(cur);
  return lines;
}

String joinLines(const std::vector<String>& lines) {
  String out;
  for (int i = 0; i < (int)lines.size(); i++) {
    if (i > 0) out += '\n';
    out += lines[i];
  }
  return out;
}

// ── SSH nano-style editor ─────────────────────────────────────────────────────
// This works over the shell TCP connection via the output stream
// Returns the edited content (shell_server sends it line by line)
//
// Protocol: shell sends lines, waits for ctrl chars
// Since our shell is TCP text, we implement a simple line editor:
//   - Shows file content with line numbers
//   - User sends: :w (save), :q (quit), :wq (save+quit), :<n> (go to line),
//                 /<text> (find), d (delete line), i<n> (insert after line n)
//                 r<n>:<text> (replace line n with text)
// It's a simplified vi-nano hybrid that works over plain TCP.
//
String nanoEdit(const String& filePath, std::function<void(String)> sendLine) {
  String content = readFile(filePath);
  auto lines = splitLines(content);
  int topLine = 0;
  int curLine = 0;
  bool modified = false;
  String clipboard;

  auto redraw = [&]() {
    sendLine("\033[2J\033[H"); // clear screen
    sendLine("── NoorNano: " + filePath + (modified ? " [modified]" : "") + " ──");
    sendLine("^S:save  ^X:exit  :w save  :q quit  :wq save+quit  /<find>  d<n> del  r<n>:<text> replace");
    sendLine("──────────────────────────────────────────────────────────────────────");
    int shown = 0;
    for (int i = topLine; i < (int)lines.size() && shown < 30; i++, shown++) {
      String prefix = (i == curLine ? ">" : " ");
      prefix += String(i + 1);
      while (prefix.length() < 5) prefix += " ";
      prefix += "| ";
      sendLine(prefix + lines[i]);
    }
    sendLine("──────────────────────────────────────────────────────────────────────");
    sendLine("Lines: " + String(lines.size()) + "  Cursor: " + String(curLine + 1));
  };

  redraw();

  // Return a marker so shell knows we're in editor mode
  // Shell will route input here until editor exits
  return "NANO_EDIT_MODE:" + filePath;
}

// ── Process a command line in nano mode ───────────────────────────────────────
struct NanoSession {
  String filePath;
  std::vector<String> lines;
  std::vector<String> history; // undo stack
  int curLine = 0;
  int topLine = 0;
  bool modified = false;
  bool active   = true;
  String clipboard;
};

static std::map<String, NanoSession> _sessions;

String nanoCommand(const String& sessionId, const String& cmd, std::function<void(String)> sendLine) {
  if (_sessions.find(sessionId) == _sessions.end()) return "No nano session\n";
  NanoSession& s = _sessions[sessionId];

  String c = cmd;
  c.trim();

  // ── Save ──
  if (c == ":w" || c == ":wq" || c == "^S") {
    writeFile(s.filePath, joinLines(s.lines));
    s.modified = false;
    sendLine("Saved: " + s.filePath);
    if (c == ":wq") { s.active = false; _sessions.erase(sessionId); return "EXIT"; }
    return "OK";
  }
  // ── Quit ──
  if (c == ":q" || c == "^X") {
    if (s.modified) { sendLine("Unsaved changes! Use :wq to save+quit or :q! to force quit."); return "OK"; }
    s.active = false; _sessions.erase(sessionId); return "EXIT";
  }
  if (c == ":q!") { s.active = false; _sessions.erase(sessionId); return "EXIT"; }

  // ── Go to line ──
  if (c.startsWith(":") && c.length() > 1 && isDigit(c[1])) {
    int n = c.substring(1).toInt() - 1;
    s.curLine = constrain(n, 0, (int)s.lines.size() - 1);
    s.topLine = max(0, s.curLine - 15);
    return "REDRAW";
  }

  // ── Delete line ──
  if (c.startsWith("d") && c.length() > 1) {
    int n = c.substring(1).toInt() - 1;
    if (n >= 0 && n < (int)s.lines.size()) {
      s.clipboard = s.lines[n];
      s.history.push_back(joinLines(s.lines));
      s.lines.erase(s.lines.begin() + n);
      s.modified = true;
    }
    return "REDRAW";
  }

  // ── Replace line ──
  if (c.startsWith("r") && c.indexOf(':') > 0) {
    int col = c.indexOf(':');
    int n = c.substring(1, col).toInt() - 1;
    String text = c.substring(col + 1);
    if (n >= 0 && n < (int)s.lines.size()) {
      s.history.push_back(joinLines(s.lines));
      s.lines[n] = text;
      s.modified = true;
    }
    return "REDRAW";
  }

  // ── Insert after line ──
  if (c.startsWith("i") && c.indexOf(':') > 0) {
    int col = c.indexOf(':');
    int n = c.substring(1, col).toInt();
    String text = c.substring(col + 1);
    s.history.push_back(joinLines(s.lines));
    s.lines.insert(s.lines.begin() + min(n, (int)s.lines.size()), text);
    s.modified = true;
    return "REDRAW";
  }

  // ── Append to line ──
  if (c.startsWith("a") && c.indexOf(':') > 0) {
    int col = c.indexOf(':');
    int n = c.substring(1, col).toInt() - 1;
    String text = c.substring(col + 1);
    if (n >= 0 && n < (int)s.lines.size()) {
      s.history.push_back(joinLines(s.lines));
      s.lines[n] += text;
      s.modified = true;
    }
    return "REDRAW";
  }

  // ── Find ──
  if (c.startsWith("/") && c.length() > 1) {
    String needle = c.substring(1);
    for (int i = s.curLine + 1; i < (int)s.lines.size(); i++) {
      if (s.lines[i].indexOf(needle) >= 0) {
        s.curLine = i;
        s.topLine = max(0, i - 10);
        sendLine("Found at line " + String(i + 1));
        return "REDRAW";
      }
    }
    sendLine("Not found: " + needle);
    return "OK";
  }

  // ── Find+Replace all ──
  if (c.startsWith("s/")) {
    int slash2 = c.indexOf('/', 2);
    int slash3 = c.indexOf('/', slash2 + 1);
    if (slash2 > 0 && slash3 > 0) {
      String find = c.substring(2, slash2);
      String repl = c.substring(slash2 + 1, slash3);
      s.history.push_back(joinLines(s.lines));
      int count = 0;
      for (auto& line : s.lines) {
        while (line.indexOf(find) >= 0) {
          line.replace(find, repl);
          count++;
        }
      }
      s.modified = true;
      sendLine("Replaced " + String(count) + " occurrence(s)");
    }
    return "REDRAW";
  }

  // ── Undo ──
  if (c == "u" || c == ":u") {
    if (!s.history.empty()) {
      s.lines = splitLines(s.history.back());
      s.history.pop_back();
      s.modified = true;
      return "REDRAW";
    }
    sendLine("Nothing to undo");
    return "OK";
  }

  // ── Paste clipboard ──
  if (c == "p") {
    s.history.push_back(joinLines(s.lines));
    s.lines.insert(s.lines.begin() + s.curLine + 1, s.clipboard);
    s.modified = true;
    return "REDRAW";
  }

  // ── Navigation ──
  if (c == "j" || c == "down") { s.curLine = min((int)s.lines.size()-1, s.curLine+1); return "REDRAW"; }
  if (c == "k" || c == "up")   { s.curLine = max(0, s.curLine-1); return "REDRAW"; }
  if (c == "gg")                { s.curLine = 0; s.topLine = 0; return "REDRAW"; }
  if (c == "G")                 { s.curLine = s.lines.size()-1; s.topLine = max(0, (int)s.lines.size()-20); return "REDRAW"; }

  // ── Help ──
  if (c == ":h" || c == "^G") {
    sendLine("NoorNano Commands:");
    sendLine("  :w          save");
    sendLine("  :q          quit (fails if unsaved)");
    sendLine("  :q!         force quit");
    sendLine("  :wq         save and quit");
    sendLine("  :<n>        go to line n");
    sendLine("  d<n>        delete line n (stored in clipboard)");
    sendLine("  r<n>:<text> replace line n");
    sendLine("  i<n>:<text> insert after line n");
    sendLine("  a<n>:<text> append to end of line n");
    sendLine("  p           paste clipboard after cursor");
    sendLine("  /<text>     find next");
    sendLine("  s/find/repl/ replace all occurrences");
    sendLine("  u           undo");
    sendLine("  j/k         move cursor down/up");
    sendLine("  gg/G        go to start/end");
    return "OK";
  }

  return "Unknown command. :h for help.\n";
}

String nanoOpen(const String& filePath, const String& sessionId) {
  NanoSession s;
  s.filePath = filePath;
  String content = readFile(filePath);
  s.lines    = splitLines(content);
  _sessions[sessionId] = s;
  return "NANO_SESSION:" + sessionId;
}

bool isNanoActive(const String& sessionId) {
  return _sessions.find(sessionId) != _sessions.end();
}

// ── TFT visual file editor ────────────────────────────────────────────────────
void tftEdit(const String& filePath) {
  String content = readFile(filePath);
  auto lines = splitLines(content);
  int scrollLine = 0;
  int editLine   = 0;
  bool modified  = false;
  std::vector<String> undoStack;

  auto drawEditor = [&]() {
    tft.fillScreen(0x0000);
    // Header
    tft.fillRect(0, 0, 240, 28, 0x0821);
    tft.setTextColor(0xFEA0, 0x0821);
    tft.setTextSize(1);
    tft.setCursor(4, 4);
    tft.print(filePath.length() > 24 ? filePath.substring(filePath.length()-24) : filePath);
    if (modified) { tft.setTextColor(0xF800, 0x0821); tft.print(" *"); }
    tft.drawFastHLine(0, 28, 240, 0x07FF);

    // Lines
    int visLines = 18;
    for (int i = 0; i < visLines && (scrollLine+i) < (int)lines.size(); i++) {
      int ln = scrollLine + i;
      uint16_t bg = (ln == editLine) ? 0x0C21 : 0x0000;
      tft.fillRect(0, 30 + i*14, 240, 14, bg);
      tft.setTextColor(0x4208, bg);
      tft.setTextSize(1);
      tft.setCursor(2, 32 + i*14);
      String num = String(ln+1);
      while (num.length() < 3) num = " " + num;
      tft.print(num + "|");
      tft.setTextColor(ln == editLine ? 0x07FF : 0xFFFF, bg);
      tft.setCursor(26, 32 + i*14);
      String lineText = lines[ln];
      if (lineText.length() > 26) lineText = lineText.substring(0, 25) + "~";
      tft.print(lineText);
    }

    // Footer buttons
    tft.fillRect(0, 285, 240, 35, 0x0821);
    tft.drawFastHLine(0, 285, 240, 0x07FF);
    // EDIT
    tft.fillRoundRect(2,   290, 52, 24, 4, 0x07FF);
    tft.setTextColor(0x0000, 0x07FF); tft.setTextSize(1);
    tft.setCursor(10, 298); tft.print("EDIT");
    // INSERT
    tft.fillRoundRect(58,  290, 52, 24, 4, 0x07E0);
    tft.setTextColor(0x0000, 0x07E0);
    tft.setCursor(64, 298); tft.print("INSRT");
    // DELETE
    tft.fillRoundRect(114, 290, 52, 24, 4, 0xF800);
    tft.setTextColor(0xFFFF, 0xF800);
    tft.setCursor(120, 298); tft.print("DEL");
    // SAVE
    tft.fillRoundRect(170, 290, 34, 24, 4, 0x07E0);
    tft.setTextColor(0x0000, 0x07E0);
    tft.setCursor(175, 298); tft.print("SAV");
    // EXIT
    tft.fillRoundRect(208, 290, 30, 24, 4, 0x4208);
    tft.setTextColor(0xFFFF, 0x4208);
    tft.setCursor(212, 298); tft.print("EXT");
  };

  drawEditor();

  while (true) {
    if (touch.tirqTouched() && touch.touched()) {
      TS_Point p = touch.getPoint();
      int tx = map(p.x, 200, 3800, 0, 240);
      int ty = map(p.y, 200, 3800, 0, 320);
      delay(100);

      // Line tap (select line to edit)
      if (ty >= 30 && ty < 285) {
        int tappedLine = scrollLine + (ty - 30) / 14;
        if (tappedLine < (int)lines.size()) {
          editLine = tappedLine;
          drawEditor();
        }
        continue;
      }

      // Footer buttons
      if (ty >= 290 && ty <= 314) {
        // EDIT line
        if (tx >= 2 && tx <= 54) {
          String result = VKeyboard::open("Edit line " + String(editLine+1) + ":", lines[editLine]);
          undoStack.push_back(joinLines(lines));
          lines[editLine] = result;
          modified = true;
          drawEditor();
        }
        // INSERT after
        else if (tx >= 58 && tx <= 110) {
          String result = VKeyboard::open("New line after " + String(editLine+1) + ":", "");
          undoStack.push_back(joinLines(lines));
          lines.insert(lines.begin() + editLine + 1, result);
          editLine++;
          modified = true;
          drawEditor();
        }
        // DELETE line
        else if (tx >= 114 && tx <= 166) {
          if (lines.size() > 1) {
            undoStack.push_back(joinLines(lines));
            lines.erase(lines.begin() + editLine);
            editLine = min(editLine, (int)lines.size()-1);
            modified = true;
            drawEditor();
          }
        }
        // SAVE
        else if (tx >= 170 && tx <= 204) {
          writeFile(filePath, joinLines(lines));
          modified = false;
          drawEditor();
        }
        // EXIT
        else if (tx >= 208 && tx <= 238) {
          if (modified) {
            // Ask save
            tft.fillRect(30, 100, 180, 100, 0x0821);
            tft.drawRoundRect(30, 100, 180, 100, 8, 0x07FF);
            tft.setTextColor(0xFFFF, 0x0821);
            tft.setTextSize(1);
            tft.setCursor(40, 116); tft.print("Unsaved changes!");
            tft.fillRoundRect(40, 150, 60, 30, 6, 0x07E0);
            tft.setTextColor(0x0000, 0x07E0); tft.setCursor(50, 162); tft.print("SAVE");
            tft.fillRoundRect(120, 150, 60, 30, 6, 0xF800);
            tft.setTextColor(0xFFFF, 0xF800); tft.setCursor(130, 162); tft.print("QUIT");
            unsigned long t = millis() + 15000;
            while (millis() < t) {
              if (touch.tirqTouched() && touch.touched()) {
                TS_Point p2 = touch.getPoint();
                int tx2 = map(p2.x, 200, 3800, 0, 240);
                int ty2 = map(p2.y, 200, 3800, 0, 320);
                if (tx2 >= 40 && tx2 <= 100 && ty2 >= 150 && ty2 <= 180) {
                  writeFile(filePath, joinLines(lines));
                  return;
                }
                if (tx2 >= 120 && tx2 <= 180 && ty2 >= 150 && ty2 <= 180) return;
              }
              delay(50);
            }
            writeFile(filePath, joinLines(lines)); // timeout = save
          }
          return;
        }
        continue;
      }
    }

    // Swipe to scroll
    if (touch.tirqTouched() && touch.touched()) {
      TS_Point p1 = touch.getPoint();
      int y1 = map(p1.y, 200, 3800, 0, 320);
      delay(200);
      if (touch.touched()) {
        TS_Point p2 = touch.getPoint();
        int y2 = map(p2.y, 200, 3800, 0, 320);
        int dy = y1 - y2;
        if (abs(dy) > 25) {
          scrollLine = constrain(scrollLine + (dy > 0 ? 3 : -3), 0, max(0, (int)lines.size()-18));
          drawEditor();
        }
      }
    }
    delay(20);
  }
}

// ── Shell handler ─────────────────────────────────────────────────────────────
String shellCmd(const String& args) {
  // nano handled by shell_server with session management
  // tedit opens TFT editor
  String path = FsManager::resolve(args);
  if (SPIFFS.exists(path)) {
    tftEdit(path);
    return "OK\n";
  }
  return "File not found: " + args + "\n";
}

} // namespace NanoEditor
