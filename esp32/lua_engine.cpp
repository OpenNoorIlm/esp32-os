// Embeds the official Lua interpreter (source in lua_src/lua-master/,
// fetched from https://github.com/lua/lua) into this sketch as a library,
// using Lua's own "onelua.c" amalgamation build (MAKE_LIB mode = no main(),
// just the VM + stdlib, so it doesn't fight Arduino's setup()/loop()).
//
// IMPORTANT build note: only THIS .cpp should ever compile the Lua sources.
// Under classic Arduino IDE, files in a sketch SUBFOLDER (lua_src/lua-master/)
// are not auto-discovered as separate compilation units, so this is safe.
// If you build with PlatformIO/arduino-cli instead, double check it isn't
// also picking up lua_src/**/*.c as independent sources -- that would define
// every Lua symbol twice and fail to link. If that happens, either exclude
// lua_src from the source scan, or move it outside this sketch folder and
// reference it via an include path.
#include "lua_engine.h"
#include "fs_manager.h"
#include "sensor_manager.h"
#include "robot_api.h"
#include "tft_manager.h"
#include "vkeyboard.h"
#include "package_manager.h"
#include "capability.h"
#include "task_manager.h"
#include <WiFi.h>

// IMPORTANT: all Arduino/C++ headers above MUST be included before Lua's
// sources below. Lua's llex.c #defines a macro literally named "next(ls)",
// and since extern "C" only affects linkage (not the preprocessor), that
// macro escapes into this whole translation unit and clobbers any later
// use of std::next / ranges::next / ArduinoJson's own next() methods --
// which is exactly what happens if this order is reversed. The #undef
// below is a second safety net in case anything further down this file
// also needs the real "next".
#define MAKE_LIB
#define LUA_32BITS   // ints/floats instead of int64/double -- meaningfully
                     // less RAM per Lua value, worth it on ESP32's limited
                     // heap. Remove this line if you need 64-bit precision.

extern "C" {
#include "lua_src/lua-master/onelua.c"
}
#undef next

namespace LuaEngine {

static lua_State* L = nullptr;

// Overrides Lua's global print() to stream straight to whichever live
// Print& is currently running the script (the NoorShell TCP client) instead
// of buffering into a String that only gets flushed once the whole script
// finishes. This matters a lot for `run <app>`: an app like the tiny
// transformer runtime can take seconds per token, and a buffered print()
// would leave the terminal looking dead the entire time instead of showing
// output as it's actually generated.
//
// liveOut is only non-null for the duration of a single eval()/runApp()
// call (set at the top, cleared before returning) -- there is exactly one
// shell session at a time (see ShellServer::loop()), so this simple global
// is safe and avoids threading a Print& through every single Lua binding.
static Print* liveOut = nullptr;
static bool didPrint = false; // did this eval/runApp call print() at least once

// Guards against two lua_State entries at once: the state (and liveOut
// above) is a single shared global, not thread-safe, so a background job
// running "lua"/"run" and a foreground "lua"/"run" in the interactive
// shell must never both be inside luaL_dostring() at the same time.
static volatile bool busy = false;

// Cooperative cancellation checkpoint, installed as a Lua debug hook (see
// runChunk below) so a background job's `close <name>` can interrupt a
// running script between VM instructions instead of only being able to
// stop it before/after a whole chunk runs. Uses shouldCancelNow() (not the
// raw flag) so this can never misfire against an unrelated foreground
// lua/run command -- see task_manager.h for why that distinction matters.
static void cancelHook(lua_State* Ls, lua_Debug* ar) {
  if (TaskManager::shouldCancelNow()) {
    luaL_error(Ls, "cancelled"); // unwinds via lua_error, caught below
  }
}

static int capturePrint(lua_State* Lstate) {
  int n = lua_gettop(Lstate);
  didPrint = true;
  for (int i = 1; i <= n; i++) {
    size_t len;
    const char* s = luaL_tolstring(Lstate, i, &len);
    if (liveOut) liveOut->print(s);
    lua_pop(Lstate, 1); // pop the tostring() result pushed by luaL_tolstring
    if (i < n && liveOut) liveOut->print("\t");
  }
  if (liveOut) { liveOut->println(); liveOut->flush(); }
  return 0;
}

} // namespace LuaEngine

namespace LuaEngine {

// ── esp32.robot.* ─────────────────────────────────────────────────────────
static int l_robot_forward(lua_State* Ls) {
  int d = (int)luaL_optinteger(Ls, 1, 1);
  int s = (int)luaL_optinteger(Ls, 2, 255);
  lua_pushstring(Ls, RobotApi::forward(String(d), String(s)).c_str());
  return 1;
}
static int l_robot_backward(lua_State* Ls) {
  int d = (int)luaL_optinteger(Ls, 1, 1);
  int s = (int)luaL_optinteger(Ls, 2, 255);
  lua_pushstring(Ls, RobotApi::backward(String(d), String(s)).c_str());
  return 1;
}
static int l_robot_right(lua_State* Ls) {
  int a = (int)luaL_optinteger(Ls, 1, 90);
  lua_pushstring(Ls, RobotApi::right(String(a)).c_str());
  return 1;
}
static int l_robot_left(lua_State* Ls) {
  int a = (int)luaL_optinteger(Ls, 1, 90);
  lua_pushstring(Ls, RobotApi::left(String(a)).c_str());
  return 1;
}
static int l_robot_stop(lua_State* Ls) {
  lua_pushstring(Ls, RobotApi::stop().c_str());
  return 1;
}

} // namespace LuaEngine

namespace LuaEngine {

static int l_robot_distance(lua_State* Ls) {
  int a = (int)luaL_optinteger(Ls, 1, 90);
  lua_pushstring(Ls, RobotApi::distance(String(a)).c_str());
  return 1;
}
static int l_robot_temperature(lua_State* Ls) {
  const char* unit = luaL_optstring(Ls, 1, "c");
  lua_pushstring(Ls, RobotApi::temperature(String(unit)).c_str());
  return 1;
}
static int l_robot_fan(lua_State* Ls) {
  const char* q = luaL_optstring(Ls, 1, "status");
  lua_pushstring(Ls, RobotApi::fan(String(q)).c_str());
  return 1;
}
static int l_robot_clear(lua_State* Ls) {
  lua_pushstring(Ls, RobotApi::clearCmd().c_str());
  return 1;
}
static int l_robot_eyes(lua_State* Ls) {
  const char* t = luaL_optstring(Ls, 1, "Normal");
  lua_pushstring(Ls, RobotApi::eyes(String(t)).c_str());
  return 1;
}
static int l_robot_shutdown(lua_State* Ls) { RobotApi::shutdown(); return 0; }
static int l_robot_shuton(lua_State* Ls)   { RobotApi::shutOn();   return 0; }
static int l_robot_shutdownBySeconds(lua_State* Ls) {
  RobotApi::shutdownBySeconds(String((int)luaL_checkinteger(Ls, 1))); return 0;
}
static int l_robot_shutdownByTime(lua_State* Ls) {
  RobotApi::shutdownByTime(String(luaL_checkstring(Ls, 1))); return 0;
}
static int l_robot_shutonBySeconds(lua_State* Ls) {
  RobotApi::shutOnBySeconds(String((int)luaL_checkinteger(Ls, 1))); return 0;
}
static int l_robot_shutonByTime(lua_State* Ls) {
  RobotApi::shutOnByTime(String(luaL_checkstring(Ls, 1))); return 0;
}

} // namespace LuaEngine

namespace LuaEngine {

// ── esp32.fs.* -- paths are absolute real paths (e.g. "/apps"), resolved
// relative to filesystem root, mirroring the shell's own path handling. ──
static int l_fs_ls(lua_State* Ls) {
  String p = FsManager::toRealPath("/", luaL_optstring(Ls, 1, "/"));
  lua_pushstring(Ls, FsManager::ls(p).c_str());
  return 1;
}
static int l_fs_mkdir(lua_State* Ls) {
  String p = FsManager::toRealPath("/", luaL_checkstring(Ls, 1));
  lua_pushboolean(Ls, FsManager::mkdir(p));
  return 1;
}
static int l_fs_rm(lua_State* Ls) {
  String p = FsManager::toRealPath("/", luaL_checkstring(Ls, 1));
  lua_pushboolean(Ls, FsManager::remove(p));
  return 1;
}
static int l_fs_cat(lua_State* Ls) {
  String p = FsManager::toRealPath("/", luaL_checkstring(Ls, 1));
  lua_pushstring(Ls, FsManager::cat(p).c_str());
  return 1;
}
static int l_fs_df(lua_State* Ls) {
  lua_pushstring(Ls, FsManager::df().c_str());
  return 1;
}

} // namespace LuaEngine

namespace LuaEngine {

// ── esp32.wifi.* ────────────────────────────────────────────────────────
static int l_wifi_ip(lua_State* Ls)   { lua_pushstring(Ls, WiFi.localIP().toString().c_str()); return 1; }
static int l_wifi_ssid(lua_State* Ls) { lua_pushstring(Ls, WiFi.SSID().c_str()); return 1; }

// ── esp32.storage.* ─────────────────────────────────────────────────────
static int l_storage_df(lua_State* Ls) { lua_pushstring(Ls, FsManager::df().c_str()); return 1; }
static int l_storage_change(lua_State* Ls) {
  lua_pushstring(Ls, FsManager::changeStorage(String(luaL_checkstring(Ls, 1))).c_str());
  return 1;
}

// installOfficial() streams every install stage live to a Print& (the
// shell's TCP client during a normal shell session) instead of returning a
// String -- there's no such live client when the call originates from Lua,
// so this tiny Print buffers everything written to it into a String, which
// we then hand back to Lua as a single return value.
class StringPrint : public Print {
 public:
  size_t write(uint8_t c) override { buf += (char)c; return 1; }
  size_t write(const uint8_t* buffer, size_t size) override {
    buf.reserve(buf.length() + size);
    for (size_t i = 0; i < size; i++) buf += (char)buffer[i];
    return size;
  }
  String buf;
};

// ── esp32.apt.* ──────────────────────────────────────────────────────────
static int l_apt_list(lua_State* Ls)          { lua_pushstring(Ls, PackageManager::listOfficial(APT_REPO).c_str()); return 1; }
static int l_apt_listInstalled(lua_State* Ls) { lua_pushstring(Ls, PackageManager::listInstalled("/pkgs").c_str()); return 1; }
static int l_apt_install(lua_State* Ls) {
  String name = luaL_checkstring(Ls, 1);
  StringPrint sp;
  PackageManager::installOfficial(APT_REPO, name, "/pkgs", sp);
  lua_pushstring(Ls, sp.buf.c_str());
  return 1;
}

// ── esp32.appinstaller.* ─────────────────────────────────────────────────
static int l_app_list(lua_State* Ls)          { lua_pushstring(Ls, PackageManager::listOfficial(APP_REPO).c_str()); return 1; }
static int l_app_listInstalled(lua_State* Ls) { lua_pushstring(Ls, PackageManager::listInstalled("/apps").c_str()); return 1; }
static int l_app_install(lua_State* Ls) {
  String name = luaL_checkstring(Ls, 1);
  StringPrint sp;
  PackageManager::installOfficial(APP_REPO, name, "/apps", sp);
  lua_pushstring(Ls, sp.buf.c_str());
  return 1;
}

} // namespace LuaEngine

namespace LuaEngine {

// ── esp32.sysinfo() / esp32.restart() ───────────────────────────────────
static int l_sysinfo(lua_State* Ls) {
  String out = capabilitiesReport();
  out += "\nInstalled OS packages (/pkgs):\n" + PackageManager::listInstalled("/pkgs");
  out += "\nInstalled apps (/apps):\n" + PackageManager::listInstalled("/apps");
  lua_pushstring(Ls, out.c_str());
  return 1;
}
static int l_restart(lua_State* Ls) { ESP.restart(); return 0; } // does not return

// ── esp32.sensor.* ────────────────────────────────────────────────────────────
static int l_sensor_temp(lua_State* Ls) {
  const char* u = luaL_optstring(Ls, 1, "c");
  lua_pushnumber(Ls, (u[0]=='f'||u[0]=='F') ? SensorManager::tempF() : SensorManager::tempC());
  return 1;
}
static int l_sensor_humidity(lua_State* Ls)   { lua_pushnumber(Ls, SensorManager::humidity());    return 1; }
static int l_sensor_distance(lua_State* Ls)   { lua_pushnumber(Ls, SensorManager::distance());    return 1; }
static int l_sensor_light(lua_State* Ls)      { lua_pushinteger(Ls, SensorManager::lightPercent()); return 1; }
static int l_sensor_sound(lua_State* Ls)      { lua_pushinteger(Ls, SensorManager::soundPercent()); return 1; }
static int l_sensor_obstacle(lua_State* Ls)   { lua_pushboolean(Ls, SensorManager::obstacle());   return 1; }
static int l_sensor_flame(lua_State* Ls)      { lua_pushboolean(Ls, SensorManager::flame());      return 1; }
static int l_sensor_tracking(lua_State* Ls)   { lua_pushboolean(Ls, SensorManager::tracking());   return 1; }
static int l_sensor_magnetic(lua_State* Ls)   { lua_pushboolean(Ls, SensorManager::magnetic());   return 1; }
static int l_sensor_laser(lua_State* Ls) {
  if (lua_gettop(Ls) > 0) SensorManager::setLaser(lua_toboolean(Ls, 1));
  lua_pushboolean(Ls, SensorManager::laserOn());
  return 1;
}
static int l_sensor_servo(lua_State* Ls) {
  int angle = (int)luaL_checkinteger(Ls, 1);
  SensorManager::setServo(angle);
  return 0;
}
static int l_sensor_scan(lua_State* Ls) {
  // scan(angle) → distance at that angle
  int angle = (int)luaL_optinteger(Ls, 1, 90);
  lua_pushnumber(Ls, SensorManager::distanceAtAngle(angle));
  return 1;
}

// ── screen.* — easy TFT GUI API ───────────────────────────────────────────────
// Color name → RGB565
static uint16_t nameToColor(const char* name) {
  String s = String(name); s.toLowerCase();
  if (s == "black")   return 0x0000;
  if (s == "white")   return 0xFFFF;
  if (s == "red")     return 0xF800;
  if (s == "green")   return 0x07E0;
  if (s == "blue")    return 0x001F;
  if (s == "cyan")    return 0x07FF;
  if (s == "magenta") return 0xF81F;
  if (s == "yellow")  return 0xFFE0;
  if (s == "orange")  return 0xFC60;
  if (s == "purple")  return 0x801F;
  if (s == "pink")    return 0xF81F;
  if (s == "gold")    return 0xFEA0;
  if (s == "grey" || s == "gray") return 0x4208;
  if (s == "darkgrey" || s == "darkgray") return 0x2104;
  if (s == "neonblue")  return 0x07FF;
  if (s == "neongreen") return 0x07E0;
  if (s == "neonpink")  return 0xF81F;
  // Allow raw hex number as string e.g. "0xF800"
  if (s.startsWith("0x")) return (uint16_t)strtol(s.c_str(), nullptr, 16);
  return 0xFFFF; // default white
}

// screen.clear([color])
static int l_screen_clear(lua_State* Ls) {
  uint16_t c = 0x0000;
  if (lua_gettop(Ls) > 0) c = nameToColor(luaL_optstring(Ls, 1, "black"));
  tft.fillScreen(c);
  return 0;
}
// screen.text(str, x, y, [color], [size])
static int l_screen_text(lua_State* Ls) {
  const char* str  = luaL_checkstring(Ls, 1);
  int x            = (int)luaL_checkinteger(Ls, 2);
  int y            = (int)luaL_checkinteger(Ls, 3);
  uint16_t color   = nameToColor(luaL_optstring(Ls, 4, "white"));
  int size         = (int)luaL_optinteger(Ls, 5, 1);
  tft.setTextColor(color);
  tft.setTextSize(size);
  tft.setCursor(x, y);
  tft.print(str);
  return 0;
}
// screen.rect(x, y, w, h, [color], [filled])
static int l_screen_rect(lua_State* Ls) {
  int x = (int)luaL_checkinteger(Ls, 1);
  int y = (int)luaL_checkinteger(Ls, 2);
  int w = (int)luaL_checkinteger(Ls, 3);
  int h = (int)luaL_checkinteger(Ls, 4);
  uint16_t c = nameToColor(luaL_optstring(Ls, 5, "white"));
  bool filled = lua_toboolean(Ls, 6);
  if (filled) tft.fillRect(x, y, w, h, c);
  else        tft.drawRect(x, y, w, h, c);
  return 0;
}
// screen.rrect(x, y, w, h, r, [color], [filled])
static int l_screen_rrect(lua_State* Ls) {
  int x = (int)luaL_checkinteger(Ls, 1);
  int y = (int)luaL_checkinteger(Ls, 2);
  int w = (int)luaL_checkinteger(Ls, 3);
  int h = (int)luaL_checkinteger(Ls, 4);
  int r = (int)luaL_checkinteger(Ls, 5);
  uint16_t c = nameToColor(luaL_optstring(Ls, 6, "white"));
  bool filled = lua_toboolean(Ls, 7);
  if (filled) tft.fillRoundRect(x, y, w, h, r, c);
  else        tft.drawRoundRect(x, y, w, h, r, c);
  return 0;
}
// screen.circle(x, y, r, [color], [filled])
static int l_screen_circle(lua_State* Ls) {
  int x = (int)luaL_checkinteger(Ls, 1);
  int y = (int)luaL_checkinteger(Ls, 2);
  int r = (int)luaL_checkinteger(Ls, 3);
  uint16_t c = nameToColor(luaL_optstring(Ls, 4, "white"));
  bool filled = lua_toboolean(Ls, 5);
  if (filled) tft.fillCircle(x, y, r, c);
  else        tft.drawCircle(x, y, r, c);
  return 0;
}
// screen.line(x1, y1, x2, y2, [color])
static int l_screen_line(lua_State* Ls) {
  int x1 = (int)luaL_checkinteger(Ls, 1);
  int y1 = (int)luaL_checkinteger(Ls, 2);
  int x2 = (int)luaL_checkinteger(Ls, 3);
  int y2 = (int)luaL_checkinteger(Ls, 4);
  uint16_t c = nameToColor(luaL_optstring(Ls, 5, "white"));
  tft.drawLine(x1, y1, x2, y2, c);
  return 0;
}
// screen.triangle(x1,y1, x2,y2, x3,y3, [color], [filled])
static int l_screen_triangle(lua_State* Ls) {
  int x1=(int)luaL_checkinteger(Ls,1), y1=(int)luaL_checkinteger(Ls,2);
  int x2=(int)luaL_checkinteger(Ls,3), y2=(int)luaL_checkinteger(Ls,4);
  int x3=(int)luaL_checkinteger(Ls,5), y3=(int)luaL_checkinteger(Ls,6);
  uint16_t c = nameToColor(luaL_optstring(Ls, 7, "white"));
  bool filled = lua_toboolean(Ls, 8);
  if (filled) tft.fillTriangle(x1,y1,x2,y2,x3,y3,c);
  else        tft.drawTriangle(x1,y1,x2,y2,x3,y3,c);
  return 0;
}
// screen.button(x, y, w, h, label, [color], [textColor]) → draws a nice button
static int l_screen_button(lua_State* Ls) {
  int x        = (int)luaL_checkinteger(Ls, 1);
  int y        = (int)luaL_checkinteger(Ls, 2);
  int w        = (int)luaL_checkinteger(Ls, 3);
  int h        = (int)luaL_checkinteger(Ls, 4);
  const char* label = luaL_checkstring(Ls, 5);
  uint16_t bg  = nameToColor(luaL_optstring(Ls, 6, "blue"));
  uint16_t fg  = nameToColor(luaL_optstring(Ls, 7, "white"));
  tft.fillRoundRect(x, y, w, h, 6, bg);
  tft.drawRoundRect(x, y, w, h, 6, fg);
  tft.setTextColor(fg, bg);
  tft.setTextSize(1);
  int tx = x + (w - strlen(label) * 6) / 2;
  int ty = y + (h - 8) / 2;
  tft.setCursor(tx, ty);
  tft.print(label);
  return 0;
}
// screen.card(x, y, w, h, title, body, [bgColor])
static int l_screen_card(lua_State* Ls) {
  int x        = (int)luaL_checkinteger(Ls, 1);
  int y        = (int)luaL_checkinteger(Ls, 2);
  int w        = (int)luaL_checkinteger(Ls, 3);
  int h        = (int)luaL_checkinteger(Ls, 4);
  const char* title = luaL_checkstring(Ls, 5);
  const char* body  = luaL_checkstring(Ls, 6);
  uint16_t bg  = nameToColor(luaL_optstring(Ls, 7, "darkgrey"));
  tft.fillRoundRect(x, y, w, h, 8, bg);
  tft.drawRoundRect(x, y, w, h, 8, 0x07FF);
  // Title bar
  tft.fillRoundRect(x, y, w, 20, 8, 0x07FF);
  tft.fillRect(x, y+12, w, 8, 0x07FF);
  tft.setTextColor(0x0000, 0x07FF);
  tft.setTextSize(1);
  tft.setCursor(x+4, y+6);
  tft.print(title);
  // Body text
  tft.setTextColor(0xFFFF, bg);
  tft.setCursor(x+4, y+26);
  tft.print(body);
  return 0;
}
// screen.bar(x, y, w, h, value, max, [fgColor], [bgColor]) → progress bar
static int l_screen_bar(lua_State* Ls) {
  int x   = (int)luaL_checkinteger(Ls, 1);
  int y   = (int)luaL_checkinteger(Ls, 2);
  int w   = (int)luaL_checkinteger(Ls, 3);
  int h   = (int)luaL_checkinteger(Ls, 4);
  float v = (float)luaL_checknumber(Ls, 5);
  float mx= (float)luaL_checknumber(Ls, 6);
  uint16_t fg = nameToColor(luaL_optstring(Ls, 7, "green"));
  uint16_t bg = nameToColor(luaL_optstring(Ls, 8, "darkgrey"));
  tft.fillRoundRect(x, y, w, h, 4, bg);
  int filled = (int)((v / mx) * w);
  if (filled > 0) tft.fillRoundRect(x, y, filled, h, 4, fg);
  tft.drawRoundRect(x, y, w, h, 4, 0xFFFF);
  return 0;
}
// screen.eyes(type, [ox], [oy]) → draw robot eyes
static int l_screen_eyes(lua_State* Ls) {
  const char* type = luaL_optstring(Ls, 1, "Normal");
  int ox = (int)luaL_optinteger(Ls, 2, 0);
  int oy = (int)luaL_optinteger(Ls, 3, 0);
  TftManager::setEyes(String(type), ox, oy);
  return 0;
}
// screen.width() / screen.height()
static int l_screen_width(lua_State* Ls)  { lua_pushinteger(Ls, 240); return 1; }
static int l_screen_height(lua_State* Ls) { lua_pushinteger(Ls, 320); return 1; }

// ── keyboard.* ────────────────────────────────────────────────────────────────
// keyboard.ask([prompt], [prefill]) → string
static int l_keyboard_ask(lua_State* Ls) {
  const char* prompt  = luaL_optstring(Ls, 1, "Input:");
  const char* prefill = luaL_optstring(Ls, 2, "");
  String result = VKeyboard::open(String(prompt), String(prefill));
  lua_pushstring(Ls, result.c_str());
  return 1;
}

static const luaL_Reg sensorFuncs[] = {
  {"temp",      l_sensor_temp},
  {"humidity",  l_sensor_humidity},
  {"distance",  l_sensor_distance},
  {"light",     l_sensor_light},
  {"sound",     l_sensor_sound},
  {"obstacle",  l_sensor_obstacle},
  {"flame",     l_sensor_flame},
  {"tracking",  l_sensor_tracking},
  {"magnetic",  l_sensor_magnetic},
  {"laser",     l_sensor_laser},
  {"servo",     l_sensor_servo},
  {"scan",      l_sensor_scan},
  {NULL, NULL}
};

static const luaL_Reg screenFuncs[] = {
  {"clear",    l_screen_clear},
  {"text",     l_screen_text},
  {"rect",     l_screen_rect},
  {"rrect",    l_screen_rrect},
  {"circle",   l_screen_circle},
  {"line",     l_screen_line},
  {"triangle", l_screen_triangle},
  {"button",   l_screen_button},
  {"card",     l_screen_card},
  {"bar",      l_screen_bar},
  {"eyes",     l_screen_eyes},
  {"width",    l_screen_width},
  {"height",   l_screen_height},
  {NULL, NULL}
};

static const luaL_Reg keyboardFuncs[] = {
  {"ask", l_keyboard_ask},
  {NULL, NULL}
};

// ════════════════════════════════════════════════════════════════════════════
// NoorUI Lua bindings — full Qt-like widget system
// ════════════════════════════════════════════════════════════════════════════

// Widget userdata tag
#define WIDGET_META "NoorUI.Widget"
#define APP_META    "NoorUI.App"
#define VBOX_META   "NoorUI.VBox"
#define HBOX_META   "NoorUI.HBox"
#define GRID_META   "NoorUI.Grid"
#define STACK_META  "NoorUI.Stack"
#define FORM_META   "NoorUI.Form"

// ── Push a Widget* as Lua userdata with metatable ─────────────────────────────
static void pushWidget(lua_State* Ls, NoorUI::Widget* w) {
  NoorUI::Widget** ud = (NoorUI::Widget**)lua_newuserdata(Ls, sizeof(NoorUI::Widget*));
  *ud = w;
  luaL_getmetatable(Ls, WIDGET_META);
  lua_setmetatable(Ls, -2);
}

static NoorUI::Widget* checkWidget(lua_State* Ls, int idx=1) {
  return *(NoorUI::Widget**)luaL_checkudata(Ls, idx, WIDGET_META);
}

// ── Widget method dispatchers ─────────────────────────────────────────────────

// Fluent geometry
static int w_move(lua_State* Ls)   { checkWidget(Ls)->move(luaL_checkinteger(Ls,2),luaL_checkinteger(Ls,3)); lua_pushvalue(Ls,1); return 1; }
static int w_size(lua_State* Ls)   { checkWidget(Ls)->size(luaL_checkinteger(Ls,2),luaL_checkinteger(Ls,3)); lua_pushvalue(Ls,1); return 1; }
static int w_geo(lua_State* Ls)    { checkWidget(Ls)->setGeometry(luaL_checkinteger(Ls,2),luaL_checkinteger(Ls,3),luaL_checkinteger(Ls,4),luaL_checkinteger(Ls,5)); lua_pushvalue(Ls,1); return 1; }

// Fluent style (method chain)
static int w_bg(lua_State* Ls)     { checkWidget(Ls)->bg(luaL_checkstring(Ls,2));    lua_pushvalue(Ls,1); return 1; }
static int w_color(lua_State* Ls)  { checkWidget(Ls)->color(luaL_checkstring(Ls,2)); lua_pushvalue(Ls,1); return 1; }
static int w_radius(lua_State* Ls) { checkWidget(Ls)->radius(luaL_checkinteger(Ls,2)); lua_pushvalue(Ls,1); return 1; }
static int w_padding(lua_State* Ls){ checkWidget(Ls)->padding(luaL_checkinteger(Ls,2)); lua_pushvalue(Ls,1); return 1; }
static int w_margin(lua_State* Ls) { checkWidget(Ls)->margin(luaL_checkinteger(Ls,2)); lua_pushvalue(Ls,1); return 1; }
static int w_shadow(lua_State* Ls) { checkWidget(Ls)->shadow(lua_gettop(Ls)<2||lua_toboolean(Ls,2)); lua_pushvalue(Ls,1); return 1; }
static int w_bold(lua_State* Ls)   { checkWidget(Ls)->bold(lua_gettop(Ls)<2||lua_toboolean(Ls,2)); lua_pushvalue(Ls,1); return 1; }
static int w_fontSize(lua_State* Ls){ checkWidget(Ls)->fontSize(luaL_checkinteger(Ls,2)); lua_pushvalue(Ls,1); return 1; }
static int w_align(lua_State* Ls)  { checkWidget(Ls)->align(luaL_checkstring(Ls,2)); lua_pushvalue(Ls,1); return 1; }
static int w_hide(lua_State* Ls)   { checkWidget(Ls)->hide(); lua_pushvalue(Ls,1); return 1; }
static int w_show(lua_State* Ls)   { checkWidget(Ls)->show(); lua_pushvalue(Ls,1); return 1; }
static int w_enabled(lua_State* Ls){ checkWidget(Ls)->enabled(lua_toboolean(Ls,2)); lua_pushvalue(Ls,1); return 1; }
static int w_setId(lua_State* Ls)  { checkWidget(Ls)->setId(luaL_checkstring(Ls,2)); lua_pushvalue(Ls,1); return 1; }

// Apply LSS — chain terminator
static int w_lss_apply(lua_State* Ls) { checkWidget(Ls)->draw(); lua_pushvalue(Ls,1); return 1; }

// Animate
static int w_animate(lua_State* Ls) {
  const char* anim = luaL_checkstring(Ls,2);
  const char* param = luaL_optstring(Ls,3,"");
  checkWidget(Ls)->animate(String(anim),String(param));
  lua_pushvalue(Ls,1); return 1;
}

// Draw
static int w_draw(lua_State* Ls)   { checkWidget(Ls)->draw(); lua_pushvalue(Ls,1); return 1; }

// Focus
static int w_setFocus(lua_State* Ls)   { checkWidget(Ls)->setFocus();   lua_pushvalue(Ls,1); return 1; }
static int w_clearFocus(lua_State* Ls) { checkWidget(Ls)->clearFocus(); lua_pushvalue(Ls,1); return 1; }

// Signal connection
static int w_on(lua_State* Ls) {
  auto* w = checkWidget(Ls);
  const char* sig = luaL_checkstring(Ls,2);
  luaL_checktype(Ls,3,LUA_TFUNCTION);
  // Store function in Lua registry so it doesn't get GC'd
  lua_pushvalue(Ls,3);
  int ref = luaL_ref(Ls, LUA_REGISTRYINDEX);
  lua_State* Ls2 = Ls; // capture
  w->on(String(sig), [Ls2,ref](String val) {
    lua_rawgeti(Ls2, LUA_REGISTRYINDEX, ref);
    lua_pushstring(Ls2, val.c_str());
    lua_pcall(Ls2, 1, 0, 0);
  });
  lua_pushvalue(Ls,1); return 1;
}

// emit
static int w_emit(lua_State* Ls) {
  checkWidget(Ls)->emit(luaL_checkstring(Ls,2), luaL_optstring(Ls,3,""));
  return 0;
}

// setText (Label, Button, TextInput, Badge)
static int w_setText(lua_State* Ls) {
  auto* w = checkWidget(Ls);
  const char* t = luaL_checkstring(Ls,2);
  if (auto* l=dynamic_cast<NoorUI::Label*>(w))     l->setText(t);
  else if (auto* b=dynamic_cast<NoorUI::Button*>(w)) b->setText(t);
  else if (auto* ti=dynamic_cast<NoorUI::TextInput*>(w)) ti->setText(t);
  else if (auto* ba=dynamic_cast<NoorUI::Badge*>(w)) ba->setText(t);
  lua_pushvalue(Ls,1); return 1;
}

// getText
static int w_getText(lua_State* Ls) {
  auto* w = checkWidget(Ls);
  if (auto* ti=dynamic_cast<NoorUI::TextInput*>(w)) { lua_pushstring(Ls,ti->getText().c_str()); return 1; }
  if (auto* l=dynamic_cast<NoorUI::Label*>(w)) { lua_pushstring(Ls,l->text.c_str()); return 1; }
  lua_pushstring(Ls,""); return 1;
}

// getValue (Slider, Spinner, ProgressBar)
static int w_getValue(lua_State* Ls) {
  auto* w = checkWidget(Ls);
  if (auto* s=dynamic_cast<NoorUI::Slider*>(w))   { lua_pushinteger(Ls,s->getValue()); return 1; }
  if (auto* s=dynamic_cast<NoorUI::Spinner*>(w))  { lua_pushinteger(Ls,s->getValue()); return 1; }
  lua_pushinteger(Ls,0); return 1;
}

// setValue
static int w_setValue(lua_State* Ls) {
  auto* w = checkWidget(Ls);
  int v = luaL_checkinteger(Ls,2);
  if (auto* s=dynamic_cast<NoorUI::Slider*>(w))      s->setValue(v);
  if (auto* s=dynamic_cast<NoorUI::Spinner*>(w))     s->value=v;
  if (auto* p=dynamic_cast<NoorUI::ProgressBar*>(w)) p->setValue(v);
  lua_pushvalue(Ls,1); return 1;
}

// isChecked / setChecked
static int w_isChecked(lua_State* Ls) {
  auto* w = checkWidget(Ls);
  if (auto* c=dynamic_cast<NoorUI::CheckBox*>(w)) { lua_pushboolean(Ls,c->isChecked()); return 1; }
  if (auto* s=dynamic_cast<NoorUI::Switch*>(w))   { lua_pushboolean(Ls,s->isOn()); return 1; }
  lua_pushboolean(Ls,0); return 1;
}
static int w_setChecked(lua_State* Ls) {
  auto* w = checkWidget(Ls);
  bool v = lua_toboolean(Ls,2);
  if (auto* c=dynamic_cast<NoorUI::CheckBox*>(w)) c->setChecked(v);
  if (auto* s=dynamic_cast<NoorUI::Switch*>(w))   s->setOn(v);
  lua_pushvalue(Ls,1); return 1;
}

// currentText (ComboBox, ListBox)
static int w_currentText(lua_State* Ls) {
  auto* w = checkWidget(Ls);
  if (auto* c=dynamic_cast<NoorUI::ComboBox*>(w)) { lua_pushstring(Ls,c->currentText().c_str()); return 1; }
  if (auto* l=dynamic_cast<NoorUI::ListBox*>(w))  { lua_pushstring(Ls,l->currentText().c_str()); return 1; }
  lua_pushstring(Ls,""); return 1;
}

// addItem (ComboBox, ListBox)
static int w_addItem(lua_State* Ls) {
  auto* w = checkWidget(Ls);
  const char* item = luaL_checkstring(Ls,2);
  if (auto* c=dynamic_cast<NoorUI::ComboBox*>(w)) c->addItem(item);
  if (auto* l=dynamic_cast<NoorUI::ListBox*>(w))  l->addItem(item);
  lua_pushvalue(Ls,1); return 1;
}

// addToPage (TabWidget)
static int w_addToPage(lua_State* Ls) {
  auto* w = checkWidget(Ls);
  int page = luaL_checkinteger(Ls,2)-1; // 1-indexed in Lua
  auto* child = checkWidget(Ls,3);
  if (auto* t=dynamic_cast<NoorUI::TabWidget*>(w)) t->addToPage(page,child);
  lua_pushvalue(Ls,1); return 1;
}

// add (Panel, Container, ScrollArea)
static int w_add(lua_State* Ls) {
  auto* w = checkWidget(Ls);
  auto* child = checkWidget(Ls,2);
  if (auto* p=dynamic_cast<NoorUI::Container*>(w)) p->add(child);
  lua_pushvalue(Ls,1); return 1;
}

// ── __newindex for .lss = ... and .style = ... ────────────────────────────────
static int w_newindex(lua_State* Ls) {
  auto* w = checkWidget(Ls);
  const char* key = luaL_checkstring(Ls,2);
  if (strcmp(key,"lss")==0) {
    if (lua_isstring(Ls,3)) {
      // CSS string syntax
      w->style.fromCss(lua_tostring(Ls,3));
      w->draw();
    } else if (lua_istable(Ls,3)) {
      // Table syntax: {bg="blue", radius=8}
      lua_pushnil(Ls);
      while (lua_next(Ls,3)) {
        String k = lua_tostring(Ls,-2);
        String v;
        if (lua_isstring(Ls,-1)) v=lua_tostring(Ls,-1);
        else if (lua_isinteger(Ls,-1)) v=String((int)lua_tointeger(Ls,-1));
        else if (lua_isnumber(Ls,-1)) v=String((float)lua_tonumber(Ls,-1));
        else if (lua_isboolean(Ls,-1)) v=lua_toboolean(Ls,-1)?"true":"false";
        w->style.applyProp(k,v);
        lua_pop(Ls,1);
      }
      w->draw();
    }
    return 0;
  }
  // Fall through: store in userdata env table
  lua_rawset(Ls,1);
  return 0;
}

// ── Widget metatable ──────────────────────────────────────────────────────────
static const luaL_Reg widgetMethods[] = {
  {"move",      w_move},    {"size",      w_size},    {"pos",       w_move},
  {"resize",    w_size},    {"geometry",  w_geo},
  {"bg",        w_bg},      {"color",     w_color},   {"radius",    w_radius},
  {"padding",   w_padding}, {"margin",    w_margin},  {"shadow",    w_shadow},
  {"bold",      w_bold},    {"fontSize",  w_fontSize},{"align",     w_align},
  {"hide",      w_hide},    {"show",      w_show},    {"enabled",   w_enabled},
  {"setId",     w_setId},   {"lss",       w_lss_apply},
  {"animate",   w_animate}, {"draw",      w_draw},
  {"setFocus",  w_setFocus},{"clearFocus",w_clearFocus},
  {"on",        w_on},      {"emit",      w_emit},
  {"setText",   w_setText}, {"getText",   w_getText},
  {"getValue",  w_getValue},{"setValue",  w_setValue},
  {"isChecked", w_isChecked},{"setChecked",w_setChecked},
  {"currentText",w_currentText},{"addItem",w_addItem},
  {"addToPage", w_addToPage},{"add",      w_add},
  {NULL,NULL}
};

// ── App userdata ──────────────────────────────────────────────────────────────
static void pushApp(lua_State* Ls, NoorUI::App* a) {
  NoorUI::App** ud=(NoorUI::App**)lua_newuserdata(Ls,sizeof(NoorUI::App*));
  *ud=a; luaL_getmetatable(Ls,APP_META); lua_setmetatable(Ls,-2);
}
static NoorUI::App* checkApp(lua_State* Ls,int idx=1) {
  return *(NoorUI::App**)luaL_checkudata(Ls,idx,APP_META);
}

static int app_add(lua_State* Ls)    { checkApp(Ls)->add(checkWidget(Ls,2)); lua_pushvalue(Ls,1); return 1; }
static int app_run(lua_State* Ls)    { checkApp(Ls)->run(); return 0; }
static int app_redraw(lua_State* Ls) { checkApp(Ls)->redraw(); return 0; }
static int app_theme(lua_State* Ls)  { checkApp(Ls)->theme(luaL_checkstring(Ls,2)); lua_pushvalue(Ls,1); return 1; }
static int app_focus(lua_State* Ls)  { checkApp(Ls)->setFocusPolicy(luaL_checkstring(Ls,2)); lua_pushvalue(Ls,1); return 1; }
static int app_timer(lua_State* Ls) {
  auto* a=checkApp(Ls);
  int ms=(int)luaL_checkinteger(Ls,2);
  luaL_checktype(Ls,3,LUA_TFUNCTION);
  lua_pushvalue(Ls,3); int ref=luaL_ref(Ls,LUA_REGISTRYINDEX);
  lua_State* Ls2=Ls;
  a->onTimer(ms,[Ls2,ref](){
    lua_rawgeti(Ls2,LUA_REGISTRYINDEX,ref);
    lua_pcall(Ls2,0,0,0);
  });
  lua_pushvalue(Ls,1); return 1;
}
static int app_addVBox(lua_State* Ls) {
  // addLayout(VBox) — take VBox userdata
  NoorUI::VBox** ud=(NoorUI::VBox**)luaL_checkudata(Ls,2,VBOX_META);
  checkApp(Ls)->addLayout(*ud); lua_pushvalue(Ls,1); return 1;
}
static int app_addHBox(lua_State* Ls) {
  NoorUI::HBox** ud=(NoorUI::HBox**)luaL_checkudata(Ls,2,HBOX_META);
  checkApp(Ls)->addLayout(*ud); lua_pushvalue(Ls,1); return 1;
}
static int app_addGrid(lua_State* Ls) {
  NoorUI::Grid** ud=(NoorUI::Grid**)luaL_checkudata(Ls,2,GRID_META);
  checkApp(Ls)->addLayout(*ud); lua_pushvalue(Ls,1); return 1;
}
static int app_addStack(lua_State* Ls) {
  NoorUI::Stack** ud=(NoorUI::Stack**)luaL_checkudata(Ls,2,STACK_META);
  checkApp(Ls)->addLayout(*ud); lua_pushvalue(Ls,1); return 1;
}
static int app_addForm(lua_State* Ls) {
  NoorUI::Form** ud=(NoorUI::Form**)luaL_checkudata(Ls,2,FORM_META);
  checkApp(Ls)->addLayout(*ud); lua_pushvalue(Ls,1); return 1;
}

static const luaL_Reg appMethods[] = {
  {"add",     app_add},   {"run",     app_run},   {"redraw",  app_redraw},
  {"theme",   app_theme}, {"focus",   app_focus}, {"onTimer", app_timer},
  {"addVBox", app_addVBox},{"addHBox",app_addHBox},{"addGrid",app_addGrid},
  {"addStack",app_addStack},{"addForm",app_addForm},
  {NULL,NULL}
};

// ── Layout userdata helpers ───────────────────────────────────────────────────
// VBox
static int vbox_add(lua_State* Ls) {
  NoorUI::VBox** ud=(NoorUI::VBox**)luaL_checkudata(Ls,1,VBOX_META);
  auto* w=checkWidget(Ls,2);
  int h=luaL_optinteger(Ls,3,-1);
  (*ud)->add(w,h); lua_pushvalue(Ls,1); return 1;
}
static int vbox_draw(lua_State* Ls) { (*(NoorUI::VBox**)luaL_checkudata(Ls,1,VBOX_META))->draw(); return 0; }

// HBox
static int hbox_add(lua_State* Ls) {
  NoorUI::HBox** ud=(NoorUI::HBox**)luaL_checkudata(Ls,1,HBOX_META);
  auto* w=checkWidget(Ls,2);
  int ww=luaL_optinteger(Ls,3,-1);
  (*ud)->add(w,ww); lua_pushvalue(Ls,1); return 1;
}
static int hbox_draw(lua_State* Ls) { (*(NoorUI::HBox**)luaL_checkudata(Ls,1,HBOX_META))->draw(); return 0; }

// Grid
static int grid_add(lua_State* Ls) {
  NoorUI::Grid** ud=(NoorUI::Grid**)luaL_checkudata(Ls,1,GRID_META);
  (*ud)->add(checkWidget(Ls,2)); lua_pushvalue(Ls,1); return 1;
}
static int grid_draw(lua_State* Ls) { (*(NoorUI::Grid**)luaL_checkudata(Ls,1,GRID_META))->draw(); return 0; }

// Stack
static int stack_addPage(lua_State* Ls) { (*(NoorUI::Stack**)luaL_checkudata(Ls,1,STACK_META))->addPage(); lua_pushvalue(Ls,1); return 1; }
static int stack_addToPage(lua_State* Ls) {
  NoorUI::Stack** ud=(NoorUI::Stack**)luaL_checkudata(Ls,1,STACK_META);
  (*ud)->addToPage(luaL_checkinteger(Ls,2)-1,checkWidget(Ls,3));
  lua_pushvalue(Ls,1); return 1;
}
static int stack_setPage(lua_State* Ls) {
  (*(NoorUI::Stack**)luaL_checkudata(Ls,1,STACK_META))->setPage(luaL_checkinteger(Ls,2)-1);
  lua_pushvalue(Ls,1); return 1;
}
static int stack_draw(lua_State* Ls) { (*(NoorUI::Stack**)luaL_checkudata(Ls,1,STACK_META))->draw(); return 0; }

// Form
static int form_addRow(lua_State* Ls) {
  NoorUI::Form** ud=(NoorUI::Form**)luaL_checkudata(Ls,1,FORM_META);
  (*ud)->addRow(luaL_checkstring(Ls,2),checkWidget(Ls,3));
  lua_pushvalue(Ls,1); return 1;
}
static int form_draw(lua_State* Ls) { (*(NoorUI::Form**)luaL_checkudata(Ls,1,FORM_META))->draw(); return 0; }

// ── ui.* factory functions ────────────────────────────────────────────────────

// ui.app(title) → App
static int l_ui_app(lua_State* Ls) {
  const char* t=luaL_optstring(Ls,1,"NoorOS App");
  pushApp(Ls,new NoorUI::App(String(t)));
  return 1;
}

// Widget factories
#define WIDGET_FACTORY(name, cls, ...) \
static int l_ui_##name(lua_State* Ls) { pushWidget(Ls,new NoorUI::cls(__VA_ARGS__)); return 1; }

static int l_ui_Button(lua_State* Ls)      { pushWidget(Ls,new NoorUI::Button(luaL_optstring(Ls,1,""))); return 1; }
static int l_ui_Label(lua_State* Ls)       { pushWidget(Ls,new NoorUI::Label(luaL_optstring(Ls,1,""))); return 1; }
static int l_ui_TextInput(lua_State* Ls)   { pushWidget(Ls,new NoorUI::TextInput(luaL_optstring(Ls,1,""))); return 1; }
static int l_ui_PasswordInput(lua_State* Ls){ pushWidget(Ls,new NoorUI::PasswordInput(luaL_optstring(Ls,1,"Password"))); return 1; }
static int l_ui_CheckBox(lua_State* Ls)    { pushWidget(Ls,new NoorUI::CheckBox(luaL_optstring(Ls,1,""),lua_toboolean(Ls,2))); return 1; }
static int l_ui_RadioButton(lua_State* Ls) { pushWidget(Ls,new NoorUI::RadioButton(luaL_checkstring(Ls,1),luaL_optstring(Ls,2,""))); return 1; }
static int l_ui_Slider(lua_State* Ls)      { pushWidget(Ls,new NoorUI::Slider(luaL_optinteger(Ls,1,0),luaL_optinteger(Ls,2,100),luaL_optinteger(Ls,3,50))); return 1; }
static int l_ui_Spinner(lua_State* Ls)     { pushWidget(Ls,new NoorUI::Spinner(luaL_optinteger(Ls,1,0),luaL_optinteger(Ls,2,100),luaL_optinteger(Ls,3,0),luaL_optinteger(Ls,4,1))); return 1; }
static int l_ui_ProgressBar(lua_State* Ls) { pushWidget(Ls,new NoorUI::ProgressBar(luaL_optinteger(Ls,1,100))); return 1; }
static int l_ui_Switch(lua_State* Ls)      { pushWidget(Ls,new NoorUI::Switch(lua_toboolean(Ls,1),luaL_optstring(Ls,2,"ON"),luaL_optstring(Ls,3,"OFF"))); return 1; }
static int l_ui_Badge(lua_State* Ls)       { pushWidget(Ls,new NoorUI::Badge(luaL_optstring(Ls,1,"1"),luaL_optstring(Ls,2,"red"))); return 1; }
static int l_ui_Separator(lua_State* Ls)   { pushWidget(Ls,new NoorUI::Separator()); return 1; }
static int l_ui_Spacer(lua_State* Ls)      { pushWidget(Ls,new NoorUI::Spacer(luaL_optinteger(Ls,1,8))); return 1; }
static int l_ui_Image(lua_State* Ls)       { pushWidget(Ls,new NoorUI::Image(luaL_optstring(Ls,1,""))); return 1; }
static int l_ui_Panel(lua_State* Ls)       { pushWidget(Ls,new NoorUI::Panel(luaL_optstring(Ls,1,""))); return 1; }
static int l_ui_ScrollArea(lua_State* Ls)  { pushWidget(Ls,new NoorUI::ScrollArea()); return 1; }

static int l_ui_ComboBox(lua_State* Ls) {
  std::vector<String> items;
  if (lua_istable(Ls,1)) {
    int n=lua_rawlen(Ls,1);
    for (int i=1;i<=n;i++) { lua_rawgeti(Ls,1,i); items.push_back(lua_tostring(Ls,-1)); lua_pop(Ls,1); }
  }
  pushWidget(Ls,new NoorUI::ComboBox(items)); return 1;
}
static int l_ui_ListBox(lua_State* Ls) {
  std::vector<String> items;
  if (lua_istable(Ls,1)) {
    int n=lua_rawlen(Ls,1);
    for (int i=1;i<=n;i++) { lua_rawgeti(Ls,1,i); items.push_back(lua_tostring(Ls,-1)); lua_pop(Ls,1); }
  }
  pushWidget(Ls,new NoorUI::ListBox(items)); return 1;
}
static int l_ui_TabWidget(lua_State* Ls) {
  std::vector<String> tabs;
  if (lua_istable(Ls,1)) {
    int n=lua_rawlen(Ls,1);
    for (int i=1;i<=n;i++) { lua_rawgeti(Ls,1,i); tabs.push_back(lua_tostring(Ls,-1)); lua_pop(Ls,1); }
  }
  pushWidget(Ls,new NoorUI::TabWidget(tabs)); return 1;
}

// Layout factories
static int l_ui_VBox(lua_State* Ls) {
  NoorUI::VBox** ud=(NoorUI::VBox**)lua_newuserdata(Ls,sizeof(NoorUI::VBox*));
  *ud=new NoorUI::VBox(luaL_optinteger(Ls,1,0),luaL_optinteger(Ls,2,30),luaL_optinteger(Ls,3,240),luaL_optinteger(Ls,4,4));
  luaL_getmetatable(Ls,VBOX_META); lua_setmetatable(Ls,-2); return 1;
}
static int l_ui_HBox(lua_State* Ls) {
  NoorUI::HBox** ud=(NoorUI::HBox**)lua_newuserdata(Ls,sizeof(NoorUI::HBox*));
  *ud=new NoorUI::HBox(luaL_optinteger(Ls,1,0),luaL_optinteger(Ls,2,30),luaL_optinteger(Ls,3,28),luaL_optinteger(Ls,4,4));
  luaL_getmetatable(Ls,HBOX_META); lua_setmetatable(Ls,-2); return 1;
}
static int l_ui_Grid(lua_State* Ls) {
  NoorUI::Grid** ud=(NoorUI::Grid**)lua_newuserdata(Ls,sizeof(NoorUI::Grid*));
  *ud=new NoorUI::Grid(luaL_optinteger(Ls,1,0),luaL_optinteger(Ls,2,30),luaL_optinteger(Ls,3,240),luaL_optinteger(Ls,4,2),luaL_optinteger(Ls,5,4),luaL_optinteger(Ls,6,28));
  luaL_getmetatable(Ls,GRID_META); lua_setmetatable(Ls,-2); return 1;
}
static int l_ui_Stack(lua_State* Ls) {
  NoorUI::Stack** ud=(NoorUI::Stack**)lua_newuserdata(Ls,sizeof(NoorUI::Stack*));
  *ud=new NoorUI::Stack(luaL_optinteger(Ls,1,0),luaL_optinteger(Ls,2,30),luaL_optinteger(Ls,3,240),luaL_optinteger(Ls,4,280));
  luaL_getmetatable(Ls,STACK_META); lua_setmetatable(Ls,-2); return 1;
}
static int l_ui_Form(lua_State* Ls) {
  NoorUI::Form** ud=(NoorUI::Form**)lua_newuserdata(Ls,sizeof(NoorUI::Form*));
  *ud=new NoorUI::Form(luaL_optinteger(Ls,1,0),luaL_optinteger(Ls,2,30),luaL_optinteger(Ls,3,240),luaL_optinteger(Ls,4,28),luaL_optinteger(Ls,5,4),luaL_optinteger(Ls,6,90));
  luaL_getmetatable(Ls,FORM_META); lua_setmetatable(Ls,-2); return 1;
}

// Dialog helpers
static int l_ui_alert(lua_State* Ls)   { NoorUI::uiAlert(luaL_checkstring(Ls,1),luaL_optstring(Ls,2,"Notice")); return 0; }
static int l_ui_confirm(lua_State* Ls) { lua_pushboolean(Ls,NoorUI::uiConfirm(luaL_checkstring(Ls,1),luaL_optstring(Ls,2,"Confirm?"))); return 1; }
static int l_ui_prompt(lua_State* Ls)  { lua_pushstring(Ls,NoorUI::uiPrompt(luaL_checkstring(Ls,1),luaL_optstring(Ls,2,"")).c_str()); return 1; }
static int l_ui_pick(lua_State* Ls) {
  std::vector<String> items;
  if (lua_istable(Ls,1)) { int n=lua_rawlen(Ls,1); for (int i=1;i<=n;i++) { lua_rawgeti(Ls,1,i); items.push_back(lua_tostring(Ls,-1)); lua_pop(Ls,1); } }
  lua_pushstring(Ls,NoorUI::uiPick(items,luaL_optstring(Ls,2,"Choose:")).c_str()); return 1;
}
static int l_ui_filepick(lua_State* Ls) { lua_pushstring(Ls,NoorUI::uiFilePick(luaL_optstring(Ls,1,"/")).c_str()); return 1; }
static int l_ui_toast(lua_State* Ls)    { NoorUI::showToast(luaL_checkstring(Ls,1),luaL_optinteger(Ls,2,2000)); return 0; }
static int l_ui_theme(lua_State* Ls)    { NoorUI::setTheme(luaL_checkstring(Ls,1)); return 0; }
static int l_ui_sleep(lua_State* Ls)    { delay(luaL_checkinteger(Ls,1)); return 0; }
static int l_ui_timestamp(lua_State* Ls){ lua_pushinteger(Ls,millis()); return 1; }
static int l_ui_clipboard_set(lua_State* Ls) { NoorUI::_clipboard=luaL_checkstring(Ls,1); return 0; }
static int l_ui_clipboard_get(lua_State* Ls) { lua_pushstring(Ls,NoorUI::_clipboard.c_str()); return 1; }

// screen.* under ui.screen
static int l_ui_screen_w(lua_State* Ls)     { lua_pushinteger(Ls,240); return 1; }
static int l_ui_screen_h(lua_State* Ls)     { lua_pushinteger(Ls,320); return 1; }
static int l_ui_screen_clear(lua_State* Ls) { tft.fillScreen(NoorUI::col(luaL_optstring(Ls,1,"black"))); return 0; }
static int l_ui_screen_pixel(lua_State* Ls) { tft.drawPixel(luaL_checkinteger(Ls,1),luaL_checkinteger(Ls,2),NoorUI::col(luaL_checkstring(Ls,3))); return 0; }
static int l_ui_screen_line(lua_State* Ls)  { tft.drawLine(luaL_checkinteger(Ls,1),luaL_checkinteger(Ls,2),luaL_checkinteger(Ls,3),luaL_checkinteger(Ls,4),NoorUI::col(luaL_optstring(Ls,5,"white"))); return 0; }

// ── Register all metatables and the ui.* global ───────────────────────────────
static void registerNoorUI(lua_State* Ls) {
  // Widget metatable
  luaL_newmetatable(Ls, WIDGET_META);
  luaL_newlib(Ls, widgetMethods);
  lua_setfield(Ls,-2,"__index");
  lua_pushcfunction(Ls,w_newindex); lua_setfield(Ls,-2,"__newindex");
  lua_pop(Ls,1);

  // App metatable
  luaL_newmetatable(Ls,APP_META);
  luaL_newlib(Ls,appMethods);
  lua_setfield(Ls,-2,"__index"); lua_pop(Ls,1);

  // VBox metatable
  luaL_newmetatable(Ls,VBOX_META);
  lua_newtable(Ls);
  lua_pushcfunction(Ls,vbox_add);  lua_setfield(Ls,-2,"add");
  lua_pushcfunction(Ls,vbox_draw); lua_setfield(Ls,-2,"draw");
  lua_setfield(Ls,-2,"__index"); lua_pop(Ls,1);

  // HBox metatable
  luaL_newmetatable(Ls,HBOX_META);
  lua_newtable(Ls);
  lua_pushcfunction(Ls,hbox_add);  lua_setfield(Ls,-2,"add");
  lua_pushcfunction(Ls,hbox_draw); lua_setfield(Ls,-2,"draw");
  lua_setfield(Ls,-2,"__index"); lua_pop(Ls,1);

  // Grid metatable
  luaL_newmetatable(Ls,GRID_META);
  lua_newtable(Ls);
  lua_pushcfunction(Ls,grid_add);  lua_setfield(Ls,-2,"add");
  lua_pushcfunction(Ls,grid_draw); lua_setfield(Ls,-2,"draw");
  lua_setfield(Ls,-2,"__index"); lua_pop(Ls,1);

  // Stack metatable
  luaL_newmetatable(Ls,STACK_META);
  lua_newtable(Ls);
  lua_pushcfunction(Ls,stack_addPage);   lua_setfield(Ls,-2,"addPage");
  lua_pushcfunction(Ls,stack_addToPage); lua_setfield(Ls,-2,"addToPage");
  lua_pushcfunction(Ls,stack_setPage);   lua_setfield(Ls,-2,"setPage");
  lua_pushcfunction(Ls,stack_draw);      lua_setfield(Ls,-2,"draw");
  lua_setfield(Ls,-2,"__index"); lua_pop(Ls,1);

  // Form metatable
  luaL_newmetatable(Ls,FORM_META);
  lua_newtable(Ls);
  lua_pushcfunction(Ls,form_addRow); lua_setfield(Ls,-2,"addRow");
  lua_pushcfunction(Ls,form_draw);   lua_setfield(Ls,-2,"draw");
  lua_setfield(Ls,-2,"__index"); lua_pop(Ls,1);

  // ui table
  lua_newtable(Ls);

  // Factory functions
  lua_pushcfunction(Ls,l_ui_app);          lua_setfield(Ls,-2,"app");
  lua_pushcfunction(Ls,l_ui_Button);       lua_setfield(Ls,-2,"Button");
  lua_pushcfunction(Ls,l_ui_Label);        lua_setfield(Ls,-2,"Label");
  lua_pushcfunction(Ls,l_ui_TextInput);    lua_setfield(Ls,-2,"TextInput");
  lua_pushcfunction(Ls,l_ui_PasswordInput);lua_setfield(Ls,-2,"PasswordInput");
  lua_pushcfunction(Ls,l_ui_CheckBox);     lua_setfield(Ls,-2,"CheckBox");
  lua_pushcfunction(Ls,l_ui_RadioButton);  lua_setfield(Ls,-2,"RadioButton");
  lua_pushcfunction(Ls,l_ui_Slider);       lua_setfield(Ls,-2,"Slider");
  lua_pushcfunction(Ls,l_ui_Spinner);      lua_setfield(Ls,-2,"Spinner");
  lua_pushcfunction(Ls,l_ui_ProgressBar);  lua_setfield(Ls,-2,"ProgressBar");
  lua_pushcfunction(Ls,l_ui_Switch);       lua_setfield(Ls,-2,"Switch");
  lua_pushcfunction(Ls,l_ui_Badge);        lua_setfield(Ls,-2,"Badge");
  lua_pushcfunction(Ls,l_ui_Separator);    lua_setfield(Ls,-2,"Separator");
  lua_pushcfunction(Ls,l_ui_Spacer);       lua_setfield(Ls,-2,"Spacer");
  lua_pushcfunction(Ls,l_ui_Image);        lua_setfield(Ls,-2,"Image");
  lua_pushcfunction(Ls,l_ui_Panel);        lua_setfield(Ls,-2,"Panel");
  lua_pushcfunction(Ls,l_ui_ScrollArea);   lua_setfield(Ls,-2,"ScrollArea");
  lua_pushcfunction(Ls,l_ui_ComboBox);     lua_setfield(Ls,-2,"ComboBox");
  lua_pushcfunction(Ls,l_ui_ListBox);      lua_setfield(Ls,-2,"ListBox");
  lua_pushcfunction(Ls,l_ui_TabWidget);    lua_setfield(Ls,-2,"TabWidget");
  lua_pushcfunction(Ls,l_ui_VBox);         lua_setfield(Ls,-2,"VBox");
  lua_pushcfunction(Ls,l_ui_HBox);         lua_setfield(Ls,-2,"HBox");
  lua_pushcfunction(Ls,l_ui_Grid);         lua_setfield(Ls,-2,"Grid");
  lua_pushcfunction(Ls,l_ui_Stack);        lua_setfield(Ls,-2,"Stack");
  lua_pushcfunction(Ls,l_ui_Form);         lua_setfield(Ls,-2,"Form");

  // Dialogs
  lua_pushcfunction(Ls,l_ui_alert);        lua_setfield(Ls,-2,"alert");
  lua_pushcfunction(Ls,l_ui_confirm);      lua_setfield(Ls,-2,"confirm");
  lua_pushcfunction(Ls,l_ui_prompt);       lua_setfield(Ls,-2,"prompt");
  lua_pushcfunction(Ls,l_ui_pick);         lua_setfield(Ls,-2,"pick");
  lua_pushcfunction(Ls,l_ui_filepick);     lua_setfield(Ls,-2,"filepick");
  lua_pushcfunction(Ls,l_ui_toast);        lua_setfield(Ls,-2,"toast");
  lua_pushcfunction(Ls,l_ui_theme);        lua_setfield(Ls,-2,"theme");
  lua_pushcfunction(Ls,l_ui_sleep);        lua_setfield(Ls,-2,"sleep");
  lua_pushcfunction(Ls,l_ui_timestamp);    lua_setfield(Ls,-2,"timestamp");

  // ui.clipboard table
  lua_newtable(Ls);
  lua_pushcfunction(Ls,l_ui_clipboard_set); lua_setfield(Ls,-2,"set");
  lua_pushcfunction(Ls,l_ui_clipboard_get); lua_setfield(Ls,-2,"get");
  lua_setfield(Ls,-2,"clipboard");

  // ui.screen table
  lua_newtable(Ls);
  lua_pushcfunction(Ls,l_ui_screen_w);     lua_setfield(Ls,-2,"w");
  lua_pushcfunction(Ls,l_ui_screen_h);     lua_setfield(Ls,-2,"h");
  lua_pushcfunction(Ls,l_ui_screen_clear); lua_setfield(Ls,-2,"clear");
  lua_pushcfunction(Ls,l_ui_screen_pixel); lua_setfield(Ls,-2,"pixel");
  lua_pushcfunction(Ls,l_ui_screen_line);  lua_setfield(Ls,-2,"line");
  lua_setfield(Ls,-2,"screen");

  lua_setglobal(Ls,"ui");
}

static const luaL_Reg vboxMeta[]  = {{"add",vbox_add},{"draw",vbox_draw},{NULL,NULL}};
static const luaL_Reg hboxMeta[]  = {{"add",hbox_add},{"draw",hbox_draw},{NULL,NULL}};

// ════════════════════════════════════════════════════════════════════════════

static const luaL_Reg robotFuncs[] = {
  {"forward", l_robot_forward}, {"backward", l_robot_backward},
  {"right", l_robot_right},     {"left", l_robot_left}, {"stop", l_robot_stop},
  {"distance", l_robot_distance}, {"temperature", l_robot_temperature},
  {"fan", l_robot_fan}, {"clear", l_robot_clear}, {"eyes", l_robot_eyes},
  {"shutdown", l_robot_shutdown}, {"shuton", l_robot_shuton},
  {"shutdownBySeconds", l_robot_shutdownBySeconds}, {"shutdownByTime", l_robot_shutdownByTime},
  {"shutonBySeconds", l_robot_shutonBySeconds}, {"shutonByTime", l_robot_shutonByTime},
  {NULL, NULL}
};
static const luaL_Reg fsFuncs[] = {
  {"ls", l_fs_ls}, {"mkdir", l_fs_mkdir}, {"rm", l_fs_rm}, {"cat", l_fs_cat}, {"df", l_fs_df},
  {NULL, NULL}
};
static const luaL_Reg wifiFuncs[] = { {"ip", l_wifi_ip}, {"ssid", l_wifi_ssid}, {NULL, NULL} };
static const luaL_Reg storageFuncs[] = { {"df", l_storage_df}, {"change", l_storage_change}, {NULL, NULL} };
static const luaL_Reg aptFuncs[] = {
  {"list", l_apt_list}, {"listInstalled", l_apt_listInstalled}, {"install", l_apt_install}, {NULL, NULL}
};
static const luaL_Reg appFuncs[] = {
  {"list", l_app_list}, {"listInstalled", l_app_listInstalled}, {"install", l_app_install}, {NULL, NULL}
};

static void pushSubtable(lua_State* Ls, const luaL_Reg* funcs) {
  lua_newtable(Ls);
  luaL_setfuncs(Ls, funcs, 0);
}

} // namespace LuaEngine

namespace LuaEngine {

void begin() {
  if (L) return; // already initialized
  L = luaL_newstate();
  if (!L) return; // out of memory creating the state
  luaL_openlibs(L);
  lua_pushcfunction(L, capturePrint);
  lua_setglobal(L, "print");

  // esp32.* table -- every shell capability, callable from Lua scripts.
  // NOTE: this deliberately does NOT expose curl/"install from arbitrary
  // URL" -- that path has an interactive [Y/n] untrusted-source warning in
  // the shell (shell_server.h), and a script has no human to ask. Silently
  // wiring that up here would let any Lua code fetch+run arbitrary remote
  // files with zero confirmation, which isn't a safe default to ship.
  lua_newtable(L);
  pushSubtable(L, robotFuncs);   lua_setfield(L, -2, "robot");
  pushSubtable(L, fsFuncs);      lua_setfield(L, -2, "fs");
  pushSubtable(L, wifiFuncs);    lua_setfield(L, -2, "wifi");
  pushSubtable(L, storageFuncs); lua_setfield(L, -2, "storage");
  pushSubtable(L, aptFuncs);     lua_setfield(L, -2, "apt");
  pushSubtable(L, appFuncs);     lua_setfield(L, -2, "appinstaller");
  lua_pushcfunction(L, l_sysinfo); lua_setfield(L, -2, "sysinfo");
  lua_pushcfunction(L, l_restart); lua_setfield(L, -2, "restart");
  lua_setglobal(L, "esp32");
  registerNoorUI(L);
}

// Shared runner: executes one chunk of Lua source with print() streaming
// live to `out`, used by both eval() (raw "lua <code>" shell command) and
// runApp() (reads an app's main.lua first, then hands it the same source).
static String runChunk(const String& src, Print& out) {
  if (!L) begin();
  if (!L) return "error: could not allocate Lua state (out of memory)";
  if (busy) return "error: lua engine busy (a background job is currently running lua code)";
  busy = true;

  liveOut = &out;
  didPrint = false;
  // Check for cancellation every 1000 VM instructions -- frequent enough
  // that `close` on a background lua job feels near-immediate, rare
  // enough that the overhead is not worth worrying about.
  lua_sethook(L, cancelHook, LUA_MASKCOUNT, 1000);
  int status = luaL_dostring(L, src.c_str());
  lua_sethook(L, nullptr, 0, 0); // clear so it never leaks into an unrelated later call
  liveOut = nullptr; // always clear -- luaL_dostring is protected (lua_pcall
                      // under the hood), so it returns normally even on a
                      // Lua-side error instead of longjmp'ing past this line.
  busy = false;

  if (status != LUA_OK) {
    String err = "lua error: " + String(lua_tostring(L, -1));
    lua_pop(L, 1); // pop the error message off the stack
    return err;
  }
  // Output (if any) already streamed live via out during execution -- only
  // fall back to a friendly status line when the script printed nothing at
  // all, so a silent script (or one that just calls esp32.* side effects)
  // doesn't leave the shell looking like it hung.
  return didPrint ? "" : "ok";
}

String eval(const String& code, Print& out) {
  return runChunk(code, out);
}

String runApp(const String& appName, Print& out) {
  if (appName.length() == 0) return "error: run needs an app name, e.g. run esp32-cpp";

  String path = "/apps/" + appName + "/main.lua";
  if (!FsManager::activeFs().exists(path))
    return "error: '" + appName + "' is not installed (no " + path + " found -- "
           "check 'app-installer list --installed')";

  String src = FsManager::cat(path);
  if (src.length() == 0 || src.startsWith("error:"))
    return "error: could not read " + path;

  String result = runChunk(src, out);
  return result == "ok" ? "" : result; // "run" itself doesn't need the "ok" filler
                                        // eval() prints for bare one-liners --
                                        // an app finishing with no output is
                                        // just normal, silent completion.
}

} // namespace LuaEngine
