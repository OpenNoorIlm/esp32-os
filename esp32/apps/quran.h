#pragma once
// ── apps/quran.h ──────────────────────────────────────────────────────────────
// Quran App for NoorRobot
// API: https://api.alquran.cloud/v1 (free, no key)
//
// Editions used:
//   quran-uthmani  — Arabic Uthmani script
//   en.kanzuliman  — Kanzul Iman (English)
//   en.kanzulirfan — Kanzul Irfan (English)
//   en.jalalayn    — Tafsir Jalalyn (English)
//
// Shell usage:
//   quran <surah> <ayah>                        — show ayah with all 4 editions
//   quran <surah> <ayah> uthmani|kanzuliman|kanzulirfan|jalalayn
//   quran surah <number>                        — list all ayahs of a surah
//   quran search <word>                         — search in Uthmani text
//
// TFT: Swipe left/right to move between ayahs, tap edition tab to switch
// ─────────────────────────────────────────────────────────────────────────────

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../tft_manager.h"
#include "../vkeyboard.h"

namespace QuranApp {

#define QURAN_API "https://api.alquran.cloud/v1"

// Edition identifiers
#define ED_UTHMANI   "quran-uthmani"
#define ED_KANZULIMAN  "en.kanzuliman"
#define ED_KANZULIRFAN "en.kanzulirfan"
#define ED_JALALAYN    "en.jalalayn"

// All 4 at once for full ayah fetch
#define ED_ALL ED_UTHMANI "," ED_KANZULIMAN "," ED_KANZULIRFAN "," ED_JALALAYN

// ── Surah names (1-114) ───────────────────────────────────────────────────────
static const char* SURAH_NAMES[] = {
  "", // 1-indexed
  "Al-Fatihah","Al-Baqarah","Aal-Imran","An-Nisa","Al-Maidah",
  "Al-Anam","Al-Araf","Al-Anfal","At-Tawbah","Yunus",
  "Hud","Yusuf","Ar-Rad","Ibrahim","Al-Hijr",
  "An-Nahl","Al-Isra","Al-Kahf","Maryam","Ta-Ha",
  "Al-Anbiya","Al-Hajj","Al-Muminun","An-Nur","Al-Furqan",
  "Ash-Shuara","An-Naml","Al-Qasas","Al-Ankabut","Ar-Rum",
  "Luqman","As-Sajdah","Al-Ahzab","Saba","Fatir",
  "Ya-Sin","As-Saffat","Sad","Az-Zumar","Ghafir",
  "Fussilat","Ash-Shura","Az-Zukhruf","Ad-Dukhan","Al-Jathiyah",
  "Al-Ahqaf","Muhammad","Al-Fath","Al-Hujurat","Qaf",
  "Adh-Dhariyat","At-Tur","An-Najm","Al-Qamar","Ar-Rahman",
  "Al-Waqiah","Al-Hadid","Al-Mujadila","Al-Hashr","Al-Mumtahanah",
  "As-Saf","Al-Jumuah","Al-Munafiqun","At-Taghabun","At-Talaq",
  "At-Tahrim","Al-Mulk","Al-Qalam","Al-Haqqah","Al-Maarij",
  "Nuh","Al-Jinn","Al-Muzzammil","Al-Muddaththir","Al-Qiyamah",
  "Al-Insan","Al-Mursalat","An-Naba","An-Naziat","Abasa",
  "At-Takwir","Al-Infitar","Al-Mutaffifin","Al-Inshiqaq","Al-Buruj",
  "At-Tariq","Al-Ala","Al-Ghashiyah","Al-Fajr","Al-Balad",
  "Ash-Shams","Al-Layl","Ad-Duha","Ash-Sharh","At-Tin",
  "Al-Alaq","Al-Qadr","Al-Bayyinah","Az-Zalzalah","Al-Adiyat",
  "Al-Qariah","At-Takathur","Al-Asr","Al-Humazah","Al-Fil",
  "Quraysh","Al-Maun","Al-Kawthar","Al-Kafirun","An-Nasr",
  "Al-Masad","Al-Ikhlas","Al-Falaq","An-Nas"
};

// ── HTTP helper ───────────────────────────────────────────────────────────────
String httpGet(const String& url) {
  if (WiFi.status() != WL_CONNECTED) return "{\"error\":\"No WiFi\"}";
  HTTPClient http;
  http.begin(url);
  http.setTimeout(8000);
  int code = http.GET();
  if (code != 200) { http.end(); return "{\"error\":\"HTTP " + String(code) + "\"}"; }
  String body = http.getString();
  http.end();
  return body;
}

// ── Fetch single ayah (all editions) ─────────────────────────────────────────
struct AyahResult {
  String arabic;
  String kanzuliman;
  String kanzulirfan;
  String jalalayn;
  String surahName;
  int surahNum;
  int ayahNum;
  bool ok;
};

AyahResult fetchAyah(int surah, int ayah) {
  AyahResult r;
  r.ok = false;
  String url = String(QURAN_API) + "/ayah/" + surah + ":" + ayah + "/" + ED_ALL;
  String body = httpGet(url);

  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, body)) return r;
  if (doc["code"] != 200) return r;

  auto data = doc["data"];
  if (!data.is<JsonArray>()) return r;

  r.surahNum  = surah;
  r.ayahNum   = ayah;
  r.surahName = surah <= 114 ? SURAH_NAMES[surah] : "?";
  r.ok        = true;

  // Editions come back in the same order we requested
  r.arabic      = data[0]["text"].as<String>();
  r.kanzuliman  = data[1]["text"].as<String>();
  r.kanzulirfan = data[2]["text"].as<String>();
  r.jalalayn    = data[3]["text"].as<String>();

  return r;
}

// ── Shell: word-wrap for terminal ─────────────────────────────────────────────
String wrap(const String& text, int cols = 72) {
  String out;
  int lineLen = 0;
  for (int i = 0; i < (int)text.length(); i++) {
    char c = text[i];
    out += c;
    lineLen++;
    if (c == '\n') lineLen = 0;
    if (lineLen >= cols && c == ' ') { out += '\n'; lineLen = 0; }
  }
  return out;
}

// ── Shell command handler ─────────────────────────────────────────────────────
// Called from shell_server.h when user types "quran ..."
String shellCmd(const String& args) {
  String out;

  // Parse args
  int sp1 = args.indexOf(' ');
  String a1 = sp1 < 0 ? args : args.substring(0, sp1);
  String rest = sp1 < 0 ? "" : args.substring(sp1 + 1);
  int sp2 = rest.indexOf(' ');
  String a2 = sp2 < 0 ? rest : rest.substring(0, sp2);
  String a3 = sp2 < 0 ? "" : rest.substring(sp2 + 1);

  // quran surah <n>
  if (a1 == "surah") {
    int sn = a2.toInt();
    if (sn < 1 || sn > 114) return "Usage: quran surah <1-114>\n";
    String url = String(QURAN_API) + "/surah/" + sn + "/" + ED_UTHMANI;
    String body = httpGet(url);
    DynamicJsonDocument doc(65536);
    if (deserializeJson(doc, body)) return "Parse error\n";
    auto ayahs = doc["data"]["ayahs"];
    out += "── Surah " + String(sn) + ": " + SURAH_NAMES[sn] + " ──\n";
    out += "Total ayahs: " + String(ayahs.size()) + "\n\n";
    for (auto a : ayahs) {
      out += "[" + a["numberInSurah"].as<String>() + "] ";
      out += a["text"].as<String>() + "\n\n";
    }
    return out;
  }

  // quran search <word>
  if (a1 == "search") {
    if (rest.isEmpty()) return "Usage: quran search <word>\n";
    String url = String(QURAN_API) + "/search/" + rest + "/all/" + ED_UTHMANI;
    String body = httpGet(url);
    DynamicJsonDocument doc(32768);
    if (deserializeJson(doc, body)) return "Parse error\n";
    auto matches = doc["data"]["matches"];
    out += "Search: \"" + rest + "\" — " + String(matches.size()) + " results\n\n";
    for (auto m : matches) {
      int sn = m["surah"]["number"];
      int an = m["numberInSurah"];
      out += String(sn) + ":" + String(an) + " [" + SURAH_NAMES[sn] + "] ";
      out += m["text"].as<String>() + "\n\n";
    }
    return out;
  }

  // quran <surah> <ayah> [edition]
  int surah = a1.toInt();
  int ayah  = a2.toInt();
  if (surah < 1 || surah > 114 || ayah < 1) {
    return "Usage:\n"
           "  quran <surah> <ayah>\n"
           "  quran <surah> <ayah> uthmani|kanzuliman|kanzulirfan|jalalayn\n"
           "  quran surah <number>\n"
           "  quran search <word>\n";
  }

  AyahResult r = fetchAyah(surah, ayah);
  if (!r.ok) return "Failed to fetch " + String(surah) + ":" + String(ayah) + "\n";

  out += "══════════════════════════════════════════\n";
  out += "Surah " + String(surah) + " (" + r.surahName + ") — Ayah " + String(ayah) + "\n";
  out += "══════════════════════════════════════════\n\n";

  if (a3.isEmpty() || a3 == "uthmani") {
    out += "── Arabic (Uthmani) ──\n";
    out += r.arabic + "\n\n";
  }
  if (a3.isEmpty() || a3 == "kanzuliman") {
    out += "── Kanzul Iman ──\n";
    out += wrap(r.kanzuliman) + "\n\n";
  }
  if (a3.isEmpty() || a3 == "kanzulirfan") {
    out += "── Kanzul Irfan ──\n";
    out += wrap(r.kanzulirfan) + "\n\n";
  }
  if (a3.isEmpty() || a3 == "jalalayn") {
    out += "── Tafsir Jalalayn ──\n";
    out += wrap(r.jalalayn) + "\n\n";
  }

  return out;
}

// ── TFT App state ─────────────────────────────────────────────────────────────
int    _tft_surah   = 1;
int    _tft_ayah    = 1;
int    _tft_edition = 0; // 0=arabic 1=kanzuliman 2=kanzulirfan 3=jalalayn
AyahResult _tft_cache;
bool   _tft_loaded  = false;

static const char* ED_TABS[] = {"ARAB", "KANZ.I", "KANZ.IR", "JALAL"};
#define TAB_COUNT 4

void tftDrawTabs() {
  int tabW = 240 / TAB_COUNT;
  for (int i = 0; i < TAB_COUNT; i++) {
    uint16_t bg = (i == _tft_edition) ? 0x07FF : 0x2104;
    uint16_t fg = (i == _tft_edition) ? 0x0000 : 0xFFFF;
    tft.fillRect(i * tabW, 40, tabW, 20, bg);
    tft.drawRect(i * tabW, 40, tabW, 20, 0x4208);
    tft.setTextColor(fg, bg);
    tft.setTextSize(1);
    int tx = i * tabW + (tabW - strlen(ED_TABS[i]) * 6) / 2;
    tft.setCursor(tx, 47);
    tft.print(ED_TABS[i]);
  }
}

void tftDrawContent() {
  tft.fillRect(0, 0, 240, 40, 0x0821);
  tft.setTextColor(0xFEA0, 0x0821);
  tft.setTextSize(1);
  tft.setCursor(4, 6);
  tft.print("QURAN");
  tft.setTextColor(0x07FF, 0x0821);
  tft.setCursor(50, 6);
  tft.print(String(_tft_surah) + ":" + String(_tft_ayah));
  if (_tft_surah <= 114) {
    tft.setTextColor(0xFFFF, 0x0821);
    tft.setCursor(90, 6);
    tft.print(SURAH_NAMES[_tft_surah]);
  }
  // Nav arrows
  tft.setTextColor(0x07E0, 0x0821);
  tft.setCursor(4, 22);  tft.print("< PREV");
  tft.setCursor(180, 22); tft.print("NEXT >");

  tftDrawTabs();

  // Content area
  tft.fillRect(0, 62, 240, 258, 0x0000);

  if (!_tft_loaded) {
    tft.setTextColor(0x07FF, 0x0000);
    tft.setTextSize(1);
    tft.setCursor(60, 150);
    tft.print("Loading...");
    return;
  }

  String text;
  uint16_t textColor = 0xFFFF;
  switch (_tft_edition) {
    case 0: text = _tft_cache.arabic;      textColor = 0xFEA0; break;
    case 1: text = _tft_cache.kanzuliman;  break;
    case 2: text = _tft_cache.kanzulirfan; break;
    case 3: text = _tft_cache.jalalayn;    break;
  }

  // Word wrap on TFT (30 chars per line at size 1)
  tft.setTextColor(textColor, 0x0000);
  tft.setTextSize(1);
  int x = 4, y = 66;
  String word = "";
  for (int i = 0; i <= (int)text.length(); i++) {
    char c = i < (int)text.length() ? text[i] : ' ';
    if (c == ' ' || c == '\n') {
      if (x + (int)word.length() * 6 > 234) { y += 12; x = 4; }
      if (y > 310) break;
      tft.setCursor(x, y);
      tft.print(word);
      x += word.length() * 6 + 4;
      word = "";
      if (c == '\n') { y += 12; x = 4; }
    } else {
      word += c;
    }
  }
}

void tftLoad() {
  _tft_loaded = false;
  tftDrawContent();
  _tft_cache  = fetchAyah(_tft_surah, _tft_ayah);
  _tft_loaded = _tft_cache.ok;
  tftDrawContent();
}

// ── TFT App loop (called from main TFT app runner) ────────────────────────────
// Returns false when user exits (swipe up or back button)
bool tftRun() {
  tft.fillScreen(0x0000);
  tftLoad();

  while (true) {
    if (touch.tirqTouched() && touch.touched()) {
      TS_Point p = touch.getPoint();
      int tx = map(p.x, 200, 3800, 0, 240);
      int ty = map(p.y, 200, 3800, 0, 320);
      delay(150);

      // Tab bar (y: 40-60)
      if (ty >= 40 && ty <= 60) {
        int tabW = 240 / TAB_COUNT;
        int tab = tx / tabW;
        if (tab != _tft_edition) {
          _tft_edition = tab;
          tftDrawTabs();
          tftDrawContent();
        }
        continue;
      }

      // Nav arrows (y: 18-34)
      if (ty >= 18 && ty <= 34) {
        if (tx < 60 && _tft_ayah > 1) { _tft_ayah--; tftLoad(); }
        if (tx > 180)                  { _tft_ayah++; tftLoad(); }
        continue;
      }

      // Surah:Ayah tap — open keyboard to jump
      if (ty >= 0 && ty < 18 && tx >= 50 && tx < 180) {
        String input = VKeyboard::open("Surah:Ayah (e.g. 2:255)", "");
        int colon = input.indexOf(':');
        if (colon > 0) {
          int s = input.substring(0, colon).toInt();
          int a = input.substring(colon + 1).toInt();
          if (s >= 1 && s <= 114 && a >= 1) {
            _tft_surah = s;
            _tft_ayah  = a;
            tft.fillScreen(0x0000);
            tftLoad();
          }
        }
        continue;
      }

      // Swipe down = exit
      if (ty > 300) return false;
    }
    delay(20);
  }
}

} // namespace QuranApp
