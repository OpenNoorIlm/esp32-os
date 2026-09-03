#pragma once
// ── lua_widgets.h ─────────────────────────────────────────────────────────────
// NoorUI — 100% Qt-like widget system for NoorOS TFT
// Designed to be as simple and non-headache as possible.
//
// ── QUICK START ───────────────────────────────────────────────────────────────
//   local app = ui.app("My App")         -- create app (clears screen, draws title)
//   local btn = ui.Button("Click me!")   -- create widget
//   btn:move(10, 50):size(200, 40)       -- position and size (chainable)
//   btn:on("clicked", function()         -- connect signal
//     ui.alert("Hello!")
//   end)
//   app:add(btn)                         -- add to app
//   app:run()                            -- start event loop (blocks until exit)
//
// ── LSS (3 syntaxes all work) ─────────────────────────────────────────────────
//   btn.lss = "bg: blue; color: white; radius: 8; shadow: true"   -- CSS string
//   btn.lss = {bg="blue", color="white", radius=8, shadow=true}    -- table
//   btn:bg("blue"):color("white"):radius(8):shadow(true):lss()     -- chain
//
// ── ALL WIDGETS ───────────────────────────────────────────────────────────────
//   ui.Button(text)            ui.Label(text)
//   ui.TextInput(placeholder)  ui.PasswordInput(placeholder)
//   ui.CheckBox(text, checked) ui.RadioButton(group, text)
//   ui.Slider(min, max, value) ui.Spinner(min, max, value, step)
//   ui.ProgressBar(max)        ui.Switch(on, labelOn, labelOff)
//   ui.ComboBox({items})       ui.ListBox({items})
//   ui.Panel(title)            ui.TabWidget({tab names})
//   ui.ScrollArea()            ui.Spacer(h)
//   ui.Image(path)             ui.ColorPicker()
//   ui.Separator()             ui.Badge(text, color)
//   ui.Toast(text, ms)         -- auto-dismiss notification
//
// ── ALL LAYOUTS ───────────────────────────────────────────────────────────────
//   ui.VBox(x,y,w,spacing)    -- vertical stack
//   ui.HBox(x,y,h,spacing)    -- horizontal stack
//   ui.Grid(x,y,w,cols,gap)   -- grid layout
//   ui.Stack(x,y,w,h)         -- stacked pages (like QStackedWidget)
//   ui.Form(x,y,w)            -- label+widget pairs like a form
//   ui.Absolute()             -- manual x,y positioning
//
// ── SIGNALS ───────────────────────────────────────────────────────────────────
//   :on("clicked",   fn)       :on("changed",   fn)
//   :on("toggled",   fn)       :on("selected",  fn)
//   :on("submitted", fn)       :on("focused",   fn)
//   :on("hovered",   fn)       :on("released",  fn)
//
// ── STYLE PROPERTIES (all LSS keys) ──────────────────────────────────────────
//   bg, color, border-color, border-width, radius, size (font),
//   padding, margin, shadow, shadow-color, bold, align (left/center/right),
//   animation (fade/pulse/bounce/slide), opacity, width, height,
//   visible, enabled, min-width, max-width
//
// ── THEMES ────────────────────────────────────────────────────────────────────
//   ui.theme("dark")   ui.theme("light")  ui.theme("neon")
//   ui.theme("ocean")  ui.theme("fire")   ui.theme("candy")
//   ui.theme("hacker") ui.theme("nord")
//   ui.theme({bg=0x0000, fg=0xFFFF, accent=0x07FF, ...})  -- custom
//
// ── STYLE INHERITANCE ────────────────────────────────────────────────────────
//   When a widget is added to a Panel or Layout, it automatically inherits
//   the parent's color scheme unless overridden. Same as Qt's palette system.
//
// ── FOCUS & KEYBOARD NAV ─────────────────────────────────────────────────────
//   app:setFocusPolicy("touch")   -- default: tap to focus
//   app:setFocusPolicy("cycle")   -- swipe right = next widget, left = prev
//   widget:setFocus()             -- programmatically focus a widget
//   widget:clearFocus()
//
// ── ANIMATIONS ───────────────────────────────────────────────────────────────
//   widget:animate("fade")        widget:animate("pulse")
//   widget:animate("bounce")      widget:animate("slide", "left")
//   widget:animate("shake")       widget:animate("glow", color)
//   widget:animate("typewriter", "Hello!")  -- text typing effect
//
// ── DIALOGS ──────────────────────────────────────────────────────────────────
//   ui.alert("Message")                    -- OK dialog
//   ui.confirm("Sure?")                    -- returns true/false
//   ui.prompt("Enter name:", "default")    -- returns string
//   ui.pick({items}, "Choose:")            -- returns selected item
//   ui.colorpick()                         -- returns RGB565 color
//   ui.filepick("/")                       -- returns file path
//   ui.progress("Loading", fn)             -- progress dialog, fn(update)
//
// ── GLOBAL HELPERS ────────────────────────────────────────────────────────────
//   ui.screen.w()  ui.screen.h()   ui.screen.clear(color)
//   ui.screen.pixel(x,y,color)     ui.screen.line(x1,y1,x2,y2,color)
//   ui.sleep(ms)   ui.timestamp()  ui.vibrate(ms)
//   ui.toast("Saved!", 2000)       -- bottom toast notification
//   ui.clipboard.set(text)         ui.clipboard.get()
//
// ── FULL EXAMPLE ─────────────────────────────────────────────────────────────
//
//   local app = ui.app("NoorRobot Control")
//   app:theme("neon")
//
//   local tabs = ui.TabWidget({"Control","Sensors","Settings"})
//   tabs:move(0,30):size(240,290)
//
//   -- Tab 1: Control
//   local vbox = ui.VBox(4, 70, 232, 6)
//
//   local fwdBtn = ui.Button("▲ Forward")
//   fwdBtn.lss = {bg="green", color="black", radius=8, shadow=true}
//   fwdBtn:on("clicked", function() robot.forward(1) end)
//
//   local stopBtn = ui.Button("■ STOP")
//   stopBtn.lss = "bg: red; color: white; radius: 4; size: 2"
//   stopBtn:on("clicked", function() robot.stop() end)
//
//   local speedSlider = ui.Slider(0, 255, 200)
//   speedSlider:on("changed", function(v) robot.setSpeed(v) end)
//
//   vbox:add(fwdBtn, 36)
//   vbox:add(stopBtn, 36)
//   vbox:add(ui.Label("Speed:"), 16)
//   vbox:add(speedSlider, 24)
//
//   tabs:page(1):setLayout(vbox)
//
//   -- Tab 2: Sensors (auto-refresh)
//   local tempLbl = ui.Label("Temp: --")
//   tempLbl.lss = "color: orange; size: 2; align: center"
//   tempLbl:move(4,80):size(232,30)
//   tabs:page(2):add(tempLbl)
//   app:onTimer(2000, function()
//     tempLbl:setText("Temp: " .. sensor.temp() .. "C")
//   end)
//
//   app:add(tabs)
//   app:run()
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <functional>
#include <map>
#include <vector>
#include <memory>
#include <algorithm>

extern TFT_eSPI tft;
extern XPT2046_Touchscreen touch;

namespace NoorUI {

// ════════════════════════════════════════════════════════════════════════════
// COLOR SYSTEM
// ════════════════════════════════════════════════════════════════════════════

static uint16_t col(const String& name) {
  String s = name; s.toLowerCase(); s.trim();
  if (s=="black")     return 0x0000; if (s=="white")     return 0xFFFF;
  if (s=="red")       return 0xF800; if (s=="green")     return 0x07E0;
  if (s=="blue")      return 0x001F; if (s=="cyan")      return 0x07FF;
  if (s=="magenta")   return 0xF81F; if (s=="yellow")    return 0xFFE0;
  if (s=="orange")    return 0xFC60; if (s=="purple")    return 0x801F;
  if (s=="pink")      return 0xFB56; if (s=="gold")      return 0xFEA0;
  if (s=="lime")      return 0x87E0; if (s=="teal")      return 0x0410;
  if (s=="navy")      return 0x000F; if (s=="maroon")    return 0x7800;
  if (s=="silver")    return 0xC618; if (s=="brown")     return 0xA145;
  if (s=="grey"||s=="gray")         return 0x4208;
  if (s=="darkgrey"||s=="darkgray") return 0x2104;
  if (s=="lightgrey"||s=="lightgray") return 0x8410;
  if (s=="transparent") return 0x0000;
  if (s.startsWith("0x")) return (uint16_t)strtol(s.c_str(),nullptr,16);
  if (s.startsWith("#")&&s.length()==7) {
    uint32_t rgb=strtol(s.c_str()+1,nullptr,16);
    return (((rgb>>16)&0xFF)>>3)<<11|(((rgb>>8)&0xFF)>>2)<<5|((rgb&0xFF)>>3);
  }
  return 0xFFFF;
}

static uint16_t dimColor(uint16_t c, float factor) {
  uint8_t r=((c>>11)&0x1F)*factor, g=((c>>5)&0x3F)*factor, b=(c&0x1F)*factor;
  return (r<<11)|(g<<5)|b;
}

// ════════════════════════════════════════════════════════════════════════════
// THEME SYSTEM (8 built-in + custom)
// ════════════════════════════════════════════════════════════════════════════

struct ThemePalette {
  uint16_t bg, fg, accent, accentFg, input, inputFg, border,
           shadow, danger, success, warning, disabled, disabledFg;
  String name;
};

static ThemePalette THEMES[] = {
  // bg      fg      accent  acFg    input   inFg    border  shadow  danger  success warning disabled disFg
  {0x0000,0xFFFF,0x07FF,0x0000,0x0821,0xFFFF,0x2104,0x0000,0xF800,0x07E0,0xFFE0,0x2104,0x4208,"dark"},
  {0xFFFF,0x0000,0x001F,0xFFFF,0xEF7D,0x0000,0xC618,0x8410,0xF800,0x03E0,0xFD20,0xC618,0x8410,"light"},
  {0x0000,0xF81F,0x07FF,0x0000,0x0821,0xF81F,0xF81F,0x000F,0xF800,0x07E0,0xFFE0,0x2104,0x8008,"neon"},
  {0x0010,0xAFFF,0x07FF,0x0000,0x0020,0xAFFF,0x001F,0x0000,0xF800,0x07E0,0xFFE0,0x0821,0x4208,"ocean"},
  {0x0800,0xFD20,0xF800,0xFFFF,0x2000,0xFD20,0xFC00,0x1000,0xF800,0x07E0,0xFFE0,0x2000,0x8200,"fire"},
  {0x0000,0xFD9F,0xF81F,0x0000,0x0821,0xFD9F,0xFB56,0x0000,0xF800,0x07E0,0xFFE0,0x2104,0x4208,"candy"},
  {0x0000,0x07E0,0x0200,0x07E0,0x0021,0x07E0,0x0200,0x0000,0xF800,0x07E0,0xFFE0,0x0821,0x0200,"hacker"},
  {0x18C3,0xE71C,0x657B,0xFFFF,0x2104,0xE71C,0x4228,0x1082,0xC924,0x4CA0,0xED80,0x2104,0x6B4D,"nord"},
};
static int _theme = 0;
static ThemePalette _custom;
static bool _useCustom = false;

ThemePalette& T() { return _useCustom ? _custom : THEMES[_theme]; }

void setTheme(const String& name) {
  _useCustom = false;
  for (int i=0;i<8;i++) if(THEMES[i].name==name){_theme=i;return;}
}

void setThemeCustom(ThemePalette p) { _custom=p; _useCustom=true; }

// ════════════════════════════════════════════════════════════════════════════
// LSS STYLE (Lua Style Sheets)
// ════════════════════════════════════════════════════════════════════════════

struct Style {
  // -1 = inherit from theme
  int      bg          = -1;
  int      color       = -1;
  int      borderColor = -1;
  int      borderWidth = 1;
  int      radius      = 4;
  int      fontSize    = 1;
  int      padding     = 4;
  int      margin      = 2;
  bool     shadow      = false;
  bool     bold        = false;
  bool     visible     = true;
  bool     enabled     = true;
  String   align       = "center";
  String   animation   = "";
  int      shadowColor = -1;
  int      minWidth    = -1;
  int      maxWidth    = -1;
  bool     _dirty      = true;  // needs redraw

  uint16_t getBg()          { return bg<0          ? T().input          : (uint16_t)bg; }
  uint16_t getColor()       { return color<0        ? T().inputFg        : (uint16_t)color; }
  uint16_t getBorderColor() { return borderColor<0  ? T().border         : (uint16_t)borderColor; }
  uint16_t getShadowColor() { return shadowColor<0  ? T().shadow         : (uint16_t)shadowColor; }

  void applyProp(const String& k, const String& v) {
    String key=k; key.trim(); key.toLowerCase();
    String val=v; val.trim();
    if (key=="bg"||key=="background"||key=="background-color") bg=col(val);
    else if (key=="color"||key=="foreground"||key=="text-color") color=col(val);
    else if (key=="border-color") borderColor=col(val);
    else if (key=="border-width") borderWidth=val.toInt();
    else if (key=="border") { borderWidth=1; borderColor=col(val); }
    else if (key=="radius"||key=="border-radius") radius=val.toInt();
    else if (key=="size"||key=="font-size") fontSize=val.toInt();
    else if (key=="padding") padding=val.toInt();
    else if (key=="margin")  margin=val.toInt();
    else if (key=="shadow")  shadow=(val=="true"||val=="1"||val=="yes");
    else if (key=="bold")    bold=(val=="true"||val=="1"||val=="yes");
    else if (key=="align"||key=="text-align") align=val;
    else if (key=="animation") animation=val;
    else if (key=="shadow-color") shadowColor=col(val);
    else if (key=="min-width") minWidth=val.toInt();
    else if (key=="max-width") maxWidth=val.toInt();
    else if (key=="visible")  visible=(val=="true"||val=="1");
    else if (key=="enabled")  enabled=(val!="false"&&val!="0");
    _dirty=true;
  }

  void fromCss(const String& css) {
    int i=0;
    while (i<(int)css.length()) {
      int colon=css.indexOf(':',i); if(colon<0) break;
      String key=css.substring(i,colon);
      int semi=css.indexOf(';',colon+1);
      String val=semi<0?css.substring(colon+1):css.substring(colon+1,semi);
      applyProp(key,val);
      i=semi<0?(int)css.length():semi+1;
    }
  }

  // Inherit unset values from parent
  void inherit(const Style& parent) {
    if (bg<0)          bg=parent.bg;
    if (color<0)       color=parent.color;
    if (borderColor<0) borderColor=parent.borderColor;
    if (shadowColor<0) shadowColor=parent.shadowColor;
  }
};

// ════════════════════════════════════════════════════════════════════════════
// CLIPBOARD
// ════════════════════════════════════════════════════════════════════════════
static String _clipboard;

// ════════════════════════════════════════════════════════════════════════════
// TOAST NOTIFICATION
// ════════════════════════════════════════════════════════════════════════════
static String  _toastText;
static unsigned long _toastUntil = 0;

void showToast(const String& text, int ms=2000) {
  _toastText  = text;
  _toastUntil = millis() + ms;
  // Draw immediately
  int tw = text.length()*6+16;
  int tx = (240-tw)/2;
  tft.fillRoundRect(tx, 295, tw, 20, 6, T().accent);
  tft.setTextColor(T().accentFg, T().accent);
  tft.setTextSize(1);
  tft.setCursor(tx+8, 302);
  tft.print(text);
}

void tickToast() {
  if (_toastUntil > 0 && millis() > _toastUntil) {
    _toastUntil = 0;
    tft.fillRect(0, 293, 240, 27, T().bg);
  }
}

// ════════════════════════════════════════════════════════════════════════════
// WIDGET BASE
// ════════════════════════════════════════════════════════════════════════════

class Widget {
public:
  int x=0,y=0,w=80,h=28;
  Style style;
  String id;
  bool focused = false;
  Widget* parent = nullptr;
  std::map<String, std::vector<std::function<void(String)>>> _signals;

  virtual ~Widget() {}
  virtual void draw() = 0;
  virtual bool onTouch(int tx,int ty) { return false; }
  virtual String type() { return "Widget"; }

  // ── Fluent positioning ────────────────────────────────────────────────────
  Widget* move(int nx,int ny)   { x=nx; y=ny; style._dirty=true; return this; }
  Widget* size(int nw,int nh)   { w=nw; h=nh; style._dirty=true; return this; }
  Widget* pos(int nx,int ny)    { return move(nx,ny); }
  Widget* resize(int nw,int nh) { return size(nw,nh); }
  Widget* setGeometry(int nx,int ny,int nw,int nh) { x=nx;y=ny;w=nw;h=nh; return this; }

  // ── Fluent style (method chain LSS) ──────────────────────────────────────
  Widget* bg(const String& c)     { style.bg=col(c);          style._dirty=true; return this; }
  Widget* color(const String& c)  { style.color=col(c);       style._dirty=true; return this; }
  Widget* border(const String& c, int width=1) { style.borderColor=col(c); style.borderWidth=width; style._dirty=true; return this; }
  Widget* radius(int r)           { style.radius=r;           style._dirty=true; return this; }
  Widget* padding(int p)          { style.padding=p;          style._dirty=true; return this; }
  Widget* margin(int m)           { style.margin=m;           style._dirty=true; return this; }
  Widget* shadow(bool s=true)     { style.shadow=s;           style._dirty=true; return this; }
  Widget* bold(bool b=true)       { style.bold=b;             style._dirty=true; return this; }
  Widget* fontSize(int s)         { style.fontSize=s;         style._dirty=true; return this; }
  Widget* align(const String& a)  { style.align=a;            style._dirty=true; return this; }
  Widget* visible(bool v)         { style.visible=v;          if(!v) tft.fillRect(x,y,w,h,T().bg); return this; }
  Widget* enabled(bool e)         { style.enabled=e;          style._dirty=true; return this; }
  Widget* hide()                  { return visible(false); }
  Widget* show()                  { style.visible=true;       style._dirty=true; return this; }
  Widget* setId(const String& i)  { id=i;                     return this; }

  // Apply LSS string or table (called from Lua via __newindex on .lss)
  Widget* lss(const String& css)  { style.fromCss(css); return this; }
  Widget* lss()                   { draw(); return this; } // chain terminator

  // ── Signals ───────────────────────────────────────────────────────────────
  Widget* on(const String& sig, std::function<void(String)> fn) {
    _signals[sig].push_back(fn);
    return this;
  }
  Widget* on(const String& sig, std::function<void()> fn) {
    _signals[sig].push_back([fn](String){fn();});
    return this;
  }

  void emit(const String& sig, const String& val="") {
    if (_signals.count(sig)) for (auto& f:_signals[sig]) f(val);
  }

  // ── Focus ─────────────────────────────────────────────────────────────────
  Widget* setFocus()   { focused=true;  style._dirty=true; draw(); return this; }
  Widget* clearFocus() { focused=false; style._dirty=true; draw(); return this; }

  // ── Geometry helpers ──────────────────────────────────────────────────────
  bool contains(int tx,int ty) { return tx>=x&&tx<=x+w&&ty>=y&&ty<=y+h; }
  int right()  { return x+w; }
  int bottom() { return y+h; }
  int centerX(){ return x+w/2; }
  int centerY(){ return y+h/2; }

  // ── Drawing primitives ────────────────────────────────────────────────────
  void drawBase(uint16_t bg2=-1, uint16_t bc=-1) {
    if (!style.visible) return;
    uint16_t bg3  = bg2==(uint16_t)-1 ? style.getBg()          : bg2;
    uint16_t brc  = bc ==(uint16_t)-1 ? style.getBorderColor()  : bc;
    if (style.shadow) {
      tft.fillRoundRect(x+2,y+2,w,h,style.radius,style.getShadowColor());
    }
    tft.fillRoundRect(x,y,w,h,style.radius,bg3);
    if (style.borderWidth>0) {
      for (int i=0;i<style.borderWidth;i++)
        tft.drawRoundRect(x-i,y-i,w+i*2,h+i*2,style.radius+i,focused?T().accent:brc);
    }
  }

  void drawLabel(const String& text, uint16_t fg=-1, uint16_t bg2=-1) {
    uint16_t fgc = fg==(uint16_t)-1 ? style.getColor() : fg;
    uint16_t bgc = bg2==(uint16_t)-1 ? style.getBg()   : bg2;
    if (!style.enabled) fgc = T().disabledFg;
    tft.setTextColor(fgc, bgc);
    tft.setTextSize(style.fontSize);
    if (style.bold) tft.setTextSize(style.fontSize); // bold via double draw offset
    int fw = text.length()*6*style.fontSize;
    int fh = 8*style.fontSize;
    int tx2, ty2 = y+(h-fh)/2;
    if (style.align=="center") tx2=x+(w-fw)/2;
    else if (style.align=="right") tx2=x+w-fw-style.padding;
    else tx2=x+style.padding;
    tft.setCursor(tx2,ty2);
    tft.print(text);
    if (style.bold) { tft.setCursor(tx2+1,ty2); tft.print(text); } // pseudo-bold
  }

  // ── Animation ─────────────────────────────────────────────────────────────
  Widget* animate(const String& anim, const String& param="") {
    if (anim=="pulse") {
      for (int i=0;i<3;i++) {
        tft.drawRoundRect(x-2,y-2,w+4,h+4,style.radius+2,T().accent);
        delay(80);
        tft.drawRoundRect(x-2,y-2,w+4,h+4,style.radius+2,T().bg);
        delay(80);
      }
    } else if (anim=="shake") {
      int ox=x;
      for (int i=0;i<6;i++) {
        tft.fillRect(x-4,y,w+8,h,T().bg);
        x=ox+(i%2==0?4:-4); draw(); delay(40);
      }
      tft.fillRect(x-4,y,w+8,h,T().bg);
      x=ox; draw();
    } else if (anim=="bounce") {
      int oy=y;
      for (int i=0;i<6;i++) {
        tft.fillRect(x,y-6,w,h+6,T().bg);
        y=oy+(i%2==0?-5:5); draw(); delay(50);
      }
      tft.fillRect(x,y-6,w,h+6,T().bg);
      y=oy; draw();
    } else if (anim=="fade") {
      for (int step=10;step>=0;step--) {
        tft.fillRoundRect(x,y,w,h,style.radius,dimColor(style.getBg(),step*0.1f));
        delay(20);
      }
      draw();
    } else if (anim=="slide") {
      int ox=x; int dir=(param=="left")?-1:1;
      x=ox-dir*w;
      for (int i=0;i<=10;i++) {
        tft.fillRect(min(x,ox)-4,y,w+8,h,T().bg);
        x=ox-dir*w+dir*w*i/10;
        draw(); delay(16);
      }
      x=ox; draw();
    } else if (anim=="glow") {
      uint16_t gc=param.isEmpty()?T().accent:col(param);
      for (int r2=1;r2<=8;r2++) {
        tft.drawRoundRect(x-r2,y-r2,w+r2*2,h+r2*2,style.radius+r2,dimColor(gc,(8-r2)/8.0f));
      }
      delay(300);
      for (int r2=8;r2>=1;r2--) {
        tft.drawRoundRect(x-r2,y-r2,w+r2*2,h+r2*2,style.radius+r2,T().bg);
      }
    }
    return this;
  }
};

// ════════════════════════════════════════════════════════════════════════════
// CONCRETE WIDGETS
// ════════════════════════════════════════════════════════════════════════════

// ── Label ─────────────────────────────────────────────────────────────────────
class Label : public Widget {
public:
  String text;
  Label(const String& t="") : text(t) {
    style.bg=T().bg; style.borderWidth=0; style.radius=0; style.align="left";
  }
  String type() override { return "Label"; }
  void draw() override {
    if (!style.visible) return;
    tft.fillRect(x,y,w,h,style.getBg());
    drawLabel(text);
  }
  Label* setText(const String& t) { text=t; style._dirty=true; draw(); return this; }
  Label* setHTML(const String& t) { text=t; draw(); return this; } // future rich text
};

// ── Button ────────────────────────────────────────────────────────────────────
class Button : public Widget {
public:
  String text;
  bool   _pressed=false;
  Button(const String& t="") : text(t) {
    style.bg=T().accent; style.color=T().accentFg;
    style.radius=6; style.shadow=true; style.align="center";
  }
  String type() override { return "Button"; }
  void draw() override {
    if (!style.visible) return;
    uint16_t bg2 = _pressed ? dimColor(style.getBg(),0.7f) : style.getBg();
    uint16_t fg  = !style.enabled ? T().disabledFg : style.getColor();
    drawBase(bg2);
    drawLabel(text,fg,bg2);
  }
  bool onTouch(int tx,int ty) override {
    if (!style.enabled||!style.visible) return false;
    if (contains(tx,ty)) {
      _pressed=true; draw(); delay(70); _pressed=false; draw();
      emit("clicked"); emit("pressed");
      if (!style.animation.isEmpty()) animate(style.animation);
      return true;
    }
    return false;
  }
  Button* setText(const String& t) { text=t; draw(); return this; }
};

// ── TextInput ─────────────────────────────────────────────────────────────────
class TextInput : public Widget {
public:
  String text, placeholder;
  bool password=false;
  TextInput(const String& ph="") : placeholder(ph) {
    style.bg=T().input; style.color=T().inputFg;
    style.borderColor=T().border; style.radius=4; style.align="left";
  }
  String type() override { return "TextInput"; }
  void draw() override {
    if (!style.visible) return;
    drawBase();
    bool empty=text.isEmpty();
    String display=empty?placeholder:(password?String("").substring(0)+String(text.length(),'*'):text);
    // Scroll if too long
    int maxChars=(w-style.padding*2)/(6*style.fontSize);
    if ((int)display.length()>maxChars) display=display.substring(display.length()-maxChars);
    uint16_t fg=empty?T().disabled:style.getColor();
    drawLabel(display,fg);
    if (focused) {
      // Cursor blink
      int cx=x+style.padding+(display.length()>0?display.length()*6*style.fontSize:0)+1;
      tft.fillRect(min(cx,x+w-4),y+4,2,h-8,T().accent);
    }
  }
  bool onTouch(int tx,int ty) override {
    if (!style.enabled||!style.visible) return false;
    if (contains(tx,ty)) {
      setFocus();
      text=VKeyboard::open(placeholder,text);
      clearFocus(); draw();
      emit("changed",text); emit("submitted",text);
      return true;
    }
    return false;
  }
  TextInput* setText(const String& t) { text=t; draw(); return this; }
  String getText() { return text; }
};

// ── PasswordInput ─────────────────────────────────────────────────────────────
class PasswordInput : public TextInput {
public:
  PasswordInput(const String& ph="Password") : TextInput(ph) { password=true; }
  String type() override { return "PasswordInput"; }
};

// ── CheckBox ──────────────────────────────────────────────────────────────────
class CheckBox : public Widget {
public:
  String text;
  bool   checked;
  CheckBox(const String& t="", bool c=false) : text(t),checked(c) {
    style.bg=T().bg; style.borderWidth=0; style.radius=0;
  }
  String type() override { return "CheckBox"; }
  void draw() override {
    if (!style.visible) return;
    tft.fillRect(x,y,w,h,style.getBg());
    int bx=x,by=y+(h-16)/2;
    tft.drawRoundRect(bx,by,16,16,3,style.getBorderColor());
    if (checked) {
      tft.fillRoundRect(bx+2,by+2,12,12,2,T().accent);
      tft.setTextColor(T().accentFg,T().accent);
      tft.setTextSize(1); tft.setCursor(bx+4,by+4); tft.print("v");
    }
    drawLabel(text,(uint16_t)(style.color<0?T().fg:(uint16_t)style.color));
    // Override cursor for label (after checkbox)
    tft.setCursor(x+22,y+(h-8)/2);
    tft.setTextColor(style.color<0?T().fg:(uint16_t)style.color,style.getBg());
    tft.print(text);
  }
  bool onTouch(int tx,int ty) override {
    if (!style.enabled||!style.visible) return false;
    if (contains(tx,ty)) { checked=!checked; draw(); emit("toggled",checked?"true":"false"); return true; }
    return false;
  }
  bool isChecked() { return checked; }
  CheckBox* setChecked(bool c) { checked=c; draw(); return this; }
};

// ── RadioButton ───────────────────────────────────────────────────────────────
class RadioButton : public Widget {
public:
  String group,text;
  bool   checked=false;
  static std::map<String,RadioButton*> _groupSelected;
  RadioButton(const String& grp, const String& t) : group(grp),text(t) {
    style.bg=T().bg; style.borderWidth=0;
  }
  String type() override { return "RadioButton"; }
  void draw() override {
    if (!style.visible) return;
    tft.fillRect(x,y,w,h,style.getBg());
    int cx2=x+8, cy2=y+h/2;
    tft.drawCircle(cx2,cy2,8,style.getBorderColor());
    if (checked) tft.fillCircle(cx2,cy2,5,T().accent);
    tft.setTextColor(style.color<0?T().fg:(uint16_t)style.color,style.getBg());
    tft.setTextSize(style.fontSize);
    tft.setCursor(x+20,y+(h-8*style.fontSize)/2);
    tft.print(text);
  }
  bool onTouch(int tx,int ty) override {
    if (!style.enabled||!style.visible) return false;
    if (contains(tx,ty)) {
      // Deselect other in group
      if (_groupSelected.count(group) && _groupSelected[group]!=this) {
        _groupSelected[group]->checked=false;
        _groupSelected[group]->draw();
      }
      checked=true; _groupSelected[group]=this;
      draw(); emit("toggled","true"); emit("selected",text);
      return true;
    }
    return false;
  }
};
std::map<String,RadioButton*> RadioButton::_groupSelected;

// ── Slider ────────────────────────────────────────────────────────────────────
class Slider : public Widget {
public:
  int value,minV,maxV;
  bool showValue=true;
  Slider(int mn=0,int mx=100,int v=50) : minV(mn),maxV(mx),value(v) {
    style.bg=T().input; style.color=T().accent; style.radius=3;
  }
  String type() override { return "Slider"; }
  void draw() override {
    if (!style.visible) return;
    int track_y=y+h/2;
    tft.fillRoundRect(x,track_y-3,w,6,3,style.getBg());
    int filled=(int)((float)(value-minV)/(maxV-minV)*w);
    if (filled>0) tft.fillRoundRect(x,track_y-3,filled,6,3,style.getColor());
    int kx=x+filled;
    if (style.shadow) tft.fillCircle(kx+1,track_y+1,9,dimColor(T().shadow,0.5f));
    tft.fillCircle(kx,track_y,8,style.getColor());
    tft.drawCircle(kx,track_y,8,T().bg);
    if (showValue) {
      tft.setTextColor(T().fg,T().bg); tft.setTextSize(1);
      String v=String(value);
      tft.fillRect(kx-v.length()*3,y,v.length()*6+2,10,T().bg);
      tft.setCursor(kx-v.length()*3,y+1); tft.print(v);
    }
  }
  bool onTouch(int tx,int ty) override {
    if (!style.enabled||!style.visible) return false;
    if (contains(tx,ty)) {
      value=minV+(int)((float)(tx-x)/w*(maxV-minV));
      value=constrain(value,minV,maxV);
      draw(); emit("changed",String(value)); return true;
    }
    return false;
  }
  int getValue() { return value; }
  Slider* setValue(int v) { value=constrain(v,minV,maxV); draw(); return this; }
};

// ── ProgressBar ───────────────────────────────────────────────────────────────
class ProgressBar : public Widget {
public:
  int value=0,maxV=100;
  String label;
  bool striped=false;
  ProgressBar(int mx=100) : maxV(mx) {
    style.bg=T().input; style.color=T().accent; style.radius=4;
  }
  String type() override { return "ProgressBar"; }
  void draw() override {
    if (!style.visible) return;
    tft.fillRoundRect(x,y,w,h,style.radius,style.getBg());
    int filled=(int)((float)value/maxV*w);
    if (filled>0) {
      tft.fillRoundRect(x,y,filled,h,style.radius,style.getColor());
      // Striped effect
      if (striped) for (int i=x;i<x+filled;i+=8) tft.drawFastVLine(i,y,h,dimColor(style.getColor(),0.7f));
    }
    tft.drawRoundRect(x,y,w,h,style.radius,style.getBorderColor());
    String pct=(label.isEmpty()?"":label+" ")+String((int)((float)value/maxV*100))+"%";
    tft.setTextColor(T().fg,filled>w/2?style.getColor():style.getBg());
    tft.setTextSize(1); tft.setCursor(x+(w-pct.length()*6)/2,y+(h-8)/2); tft.print(pct);
  }
  ProgressBar* setValue(int v) { value=constrain(v,0,maxV); draw(); return this; }
  ProgressBar* setLabel(const String& l) { label=l; draw(); return this; }
  ProgressBar* setStriped(bool s) { striped=s; draw(); return this; }
};

// ── Switch ────────────────────────────────────────────────────────────────────
class Switch : public Widget {
public:
  bool on2=false;
  String labelOn="ON",labelOff="OFF";
  Switch(bool init=false,const String& lon="ON",const String& loff="OFF")
    : on2(init),labelOn(lon),labelOff(loff) { style.radius=h/2; }
  String type() override { return "Switch"; }
  void draw() override {
    if (!style.visible) return;
    uint16_t track=on2?T().success:T().disabled;
    tft.fillRoundRect(x,y,w,h,h/2,track);
    tft.drawRoundRect(x,y,w,h,h/2,style.getBorderColor());
    int knobX=on2?x+w-h+2:x+2;
    if (style.shadow) tft.fillCircle(knobX+h/2-2,y+h/2+1,h/2-2,T().shadow);
    tft.fillCircle(knobX+h/2-2,y+h/2,h/2-2,0xFFFF);
    tft.setTextColor(0xFFFF,track); tft.setTextSize(1);
    if (on2) { tft.setCursor(x+4,y+(h-8)/2); tft.print(labelOn); }
    else     { tft.setCursor(x+h+2,y+(h-8)/2); tft.print(labelOff); }
  }
  bool onTouch(int tx,int ty) override {
    if (!style.enabled||!style.visible) return false;
    if (contains(tx,ty)) { on2=!on2; draw(); emit("toggled",on2?"true":"false"); return true; }
    return false;
  }
  bool isOn() { return on2; }
  Switch* setOn(bool v) { on2=v; draw(); return this; }
};

// ── Spinner ───────────────────────────────────────────────────────────────────
class Spinner : public Widget {
public:
  int value,minV,maxV,step;
  Spinner(int mn=0,int mx=100,int v=0,int s=1)
    : minV(mn),maxV(mx),value(v),step(s) {
    style.bg=T().input; style.color=T().inputFg; style.radius=4;
  }
  String type() override { return "Spinner"; }
  void draw() override {
    if (!style.visible) return;
    drawBase();
    // Minus
    tft.fillRoundRect(x+2,y+2,22,h-4,4,T().danger);
    tft.setTextColor(0xFFFF,T().danger); tft.setTextSize(2);
    tft.setCursor(x+8,y+(h-16)/2); tft.print("-");
    // Plus
    tft.fillRoundRect(x+w-24,y+2,22,h-4,4,T().success);
    tft.setTextColor(0xFFFF,T().success); tft.setTextSize(2);
    tft.setCursor(x+w-18,y+(h-16)/2); tft.print("+");
    // Value
    String v=String(value);
    tft.setTextColor(style.getColor(),style.getBg()); tft.setTextSize(style.fontSize);
    tft.setCursor(x+26+(w-52-v.length()*6*style.fontSize)/2,y+(h-8*style.fontSize)/2);
    tft.print(v);
  }
  bool onTouch(int tx,int ty) override {
    if (!style.enabled||!style.visible) return false;
    if (tx>=x&&tx<=x+24&&contains(x,ty)) { value=max(minV,value-step); draw(); emit("changed",String(value)); return true; }
    if (tx>=x+w-24&&tx<=x+w&&ty>=y&&ty<=y+h) { value=min(maxV,value+step); draw(); emit("changed",String(value)); return true; }
    return false;
  }
  int getValue() { return value; }
};

// ── ComboBox ──────────────────────────────────────────────────────────────────
class ComboBox : public Widget {
public:
  std::vector<String> items;
  int selectedIdx=0;
  bool expanded=false;
  ComboBox(std::vector<String> items2={}) : items(items2) {
    style.bg=T().input; style.color=T().inputFg;
    style.borderColor=T().border; style.radius=4;
  }
  String type() override { return "ComboBox"; }
  void draw() override {
    if (!style.visible) return;
    drawBase();
    String sel=items.empty()?"—":items[selectedIdx];
    drawLabel(sel);
    // Arrow
    int ax=x+w-14,ay=y+h/2;
    tft.fillTriangle(ax,ay-3,ax+8,ay-3,ax+4,ay+4,style.getColor());
    if (expanded) drawDropdown();
  }
  void drawDropdown() {
    int rows=min((int)items.size(),5);
    int dy=y+h;
    tft.fillRoundRect(x,dy,w,rows*20+4,4,T().input);
    tft.drawRoundRect(x,dy,w,rows*20+4,4,T().border);
    for (int i=0;i<rows;i++) {
      bool sel=(i==selectedIdx);
      if (sel) tft.fillRect(x+2,dy+2+i*20,w-4,20,T().accent);
      tft.setTextColor(sel?T().accentFg:T().inputFg,sel?T().accent:T().input);
      tft.setTextSize(style.fontSize);
      tft.setCursor(x+style.padding,dy+6+i*20);
      tft.print(items[i]);
    }
  }
  bool onTouch(int tx,int ty) override {
    if (!style.enabled||!style.visible) return false;
    if (expanded) {
      int rows=min((int)items.size(),5);
      int dy=y+h;
      if (tx>=x&&tx<=x+w&&ty>=dy&&ty<dy+rows*20+4) {
        int idx=(ty-dy-2)/20;
        if (idx>=0&&idx<(int)items.size()) { selectedIdx=idx; emit("selected",items[idx]); emit("changed",items[idx]); }
        expanded=false; draw(); return true;
      }
      expanded=false; draw(); return true;
    }
    if (contains(tx,ty)) { expanded=true; draw(); return true; }
    return false;
  }
  String currentText() { return items.empty()?"":items[selectedIdx]; }
  int currentIndex()   { return selectedIdx; }
  ComboBox* addItem(const String& s) { items.push_back(s); return this; }
  ComboBox* setCurrentIndex(int i) { selectedIdx=constrain(i,0,(int)items.size()-1); draw(); return this; }
};

// ── ListBox ───────────────────────────────────────────────────────────────────
class ListBox : public Widget {
public:
  std::vector<String> items;
  int selectedIdx=-1, scrollTop=0;
  ListBox(std::vector<String> items2={}) : items(items2) {
    style.bg=T().input; style.color=T().inputFg; style.radius=4;
  }
  String type() override { return "ListBox"; }
  int rowHeight() { return 8*style.fontSize+8; }
  void draw() override {
    if (!style.visible) return;
    drawBase();
    int rh=rowHeight(), rows=(h-4)/rh;
    for (int i=0;i<rows&&(scrollTop+i)<(int)items.size();i++) {
      int idx=scrollTop+i;
      bool sel=(idx==selectedIdx);
      if (sel) tft.fillRect(x+2,y+2+i*rh,w-4,rh,T().accent);
      tft.setTextColor(sel?T().accentFg:T().inputFg,sel?T().accent:T().input);
      tft.setTextSize(style.fontSize);
      tft.setCursor(x+style.padding,y+2+i*rh+(rh-8*style.fontSize)/2);
      String item=items[idx];
      if ((int)item.length()*6*style.fontSize>w-style.padding*2) item=item.substring(0,(w-style.padding*2)/(6*style.fontSize)-1)+"~";
      tft.print(item);
    }
    // Scrollbar
    if ((int)items.size()>rows) {
      int sbH=h*(rows*rh)/(items.size()*rh);
      int sbY=y+h*scrollTop*rh/(items.size()*rh);
      tft.fillRect(x+w-4,y,4,h,T().disabled);
      tft.fillRect(x+w-4,sbY,4,sbH,T().accent);
    }
  }
  bool onTouch(int tx,int ty) override {
    if (!style.enabled||!style.visible) return false;
    if (contains(tx,ty)) {
      int rh=rowHeight();
      int idx=scrollTop+(ty-y-2)/rh;
      if (idx>=0&&idx<(int)items.size()) {
        selectedIdx=idx; draw();
        emit("selected",items[idx]); emit("clicked",items[idx]);
      }
      return true;
    }
    return false;
  }
  String currentText() { return selectedIdx>=0?items[selectedIdx]:""; }
  ListBox* addItem(const String& s) { items.push_back(s); draw(); return this; }
  ListBox* clear() { items.clear(); selectedIdx=-1; draw(); return this; }
};

// ── Separator ─────────────────────────────────────────────────────────────────
class Separator : public Widget {
public:
  Separator() { h=2; style.bg=T().border; style.borderWidth=0; }
  String type() override { return "Separator"; }
  void draw() override { tft.fillRect(x,y+h/2,w,1,style.getBg()); }
};

// ── Spacer ────────────────────────────────────────────────────────────────────
class Spacer : public Widget {
public:
  Spacer(int height=8) { h=height; style.visible=false; }
  String type() override { return "Spacer"; }
  void draw() override {}
};

// ── Badge ─────────────────────────────────────────────────────────────────────
class Badge : public Widget {
public:
  String text;
  Badge(const String& t="1", const String& c="red") : text(t) {
    style.bg=col(c); style.color=0xFFFF; style.radius=h;
    style.borderWidth=0; style.shadow=false;
    w=max(16,(int)t.length()*6+8); h=16;
  }
  String type() override { return "Badge"; }
  void draw() override {
    if (!style.visible) return;
    tft.fillRoundRect(x,y,w,h,h/2,style.getBg());
    tft.setTextColor(style.getColor(),style.getBg());
    tft.setTextSize(1);
    tft.setCursor(x+(w-text.length()*6)/2,y+(h-8)/2);
    tft.print(text);
  }
  Badge* setText(const String& t) { text=t; w=max(16,(int)t.length()*6+8); draw(); return this; }
};

// ── Image (placeholder — loads from SPIFFS raw RGB565) ───────────────────────
class Image : public Widget {
public:
  String path;
  Image(const String& p="") : path(p) { style.borderWidth=0; style.radius=0; }
  String type() override { return "Image"; }
  void draw() override {
    if (!style.visible) return;
    if (path.isEmpty()) { tft.fillRect(x,y,w,h,0x4208); tft.setTextColor(0xFFFF,0x4208); tft.setCursor(x+4,y+h/2-4); tft.print("IMG"); return; }
    File f=SPIFFS.open(path,"r");
    if (!f) { tft.fillRect(x,y,w,h,T().danger); return; }
    for (int py=0;py<h&&f.available();py++) {
      for (int px=0;px<w&&f.available();px++) {
        uint8_t hi=f.read(), lo=f.available()?f.read():0;
        tft.drawPixel(x+px,y+py,((uint16_t)hi<<8)|lo);
      }
    }
    f.close();
  }
  Image* setPath(const String& p) { path=p; draw(); return this; }
};

// ════════════════════════════════════════════════════════════════════════════
// CONTAINER WIDGETS (Panel, TabWidget, ScrollArea)
// ════════════════════════════════════════════════════════════════════════════

class Container : public Widget {
public:
  std::vector<Widget*> children;
  virtual void add(Widget* w2, int wh=-1) { w2->parent=this; children.push_back(w2); }
  virtual void remove(Widget* w2) { children.erase(std::remove(children.begin(),children.end(),w2),children.end()); }
  bool onTouch(int tx,int ty) override {
    for (auto it=children.rbegin();it!=children.rend();++it)
      if ((*it)->onTouch(tx,ty)) return true;
    return false;
  }
  void drawChildren() { for (auto* c:children) c->draw(); }
  // Style inheritance: propagate bg/fg to children that haven't set their own
  void propagateStyle() {
    for (auto* c:children) {
      c->style.inherit(style);
    }
  }
};

// ── Panel ─────────────────────────────────────────────────────────────────────
class Panel : public Container {
public:
  String title;
  Panel(const String& t="") : title(t) {
    style.bg=T().bg; style.borderColor=T().border; style.radius=6;
  }
  String type() override { return "Panel"; }
  void draw() override {
    if (!style.visible) return;
    drawBase();
    int contentY=y;
    if (!title.isEmpty()) {
      tft.fillRoundRect(x,y,w,22,style.radius,T().accent);
      tft.fillRect(x,y+style.radius,w,22-style.radius,T().accent);
      tft.setTextColor(T().accentFg,T().accent); tft.setTextSize(1);
      tft.setCursor(x+(w-title.length()*6)/2,y+7); tft.print(title);
      contentY=y+22;
    }
    propagateStyle();
    drawChildren();
  }
};

// ── ScrollArea ────────────────────────────────────────────────────────────────
class ScrollArea : public Container {
public:
  int scrollY=0, contentH=0;
  ScrollArea() { style.bg=T().bg; style.radius=4; style.borderColor=T().border; }
  String type() override { return "ScrollArea"; }
  void add(Widget* w2, int wh=-1) override {
    w2->parent=this;
    // Offset widget by scroll
    children.push_back(w2);
    contentH=max(contentH,w2->y+w2->h-y);
  }
  void draw() override {
    if (!style.visible) return;
    drawBase();
    // Clip area
    for (auto* c:children) {
      if (c->y-scrollY+c->h >= y && c->y-scrollY <= y+h) {
        int origY=c->y; c->y-=scrollY; c->draw(); c->y=origY;
      }
    }
    // Scrollbar
    if (contentH>h) {
      int sbH=h*h/contentH;
      int sbY=y+scrollY*h/contentH;
      tft.fillRect(x+w-4,y,4,h,T().disabled);
      tft.fillRect(x+w-4,sbY,4,sbH,T().accent);
    }
  }
  bool onTouch(int tx,int ty) override {
    if (!contains(tx,ty)) return false;
    int adjTY=ty+scrollY;
    for (auto it=children.rbegin();it!=children.rend();++it) {
      int origY=(*it)->y; (*it)->y-=scrollY;
      bool hit=(*it)->onTouch(tx,ty);
      (*it)->y=origY;
      if (hit) return true;
    }
    return false;
  }
  void scroll(int dy) { scrollY=constrain(scrollY+dy,0,max(0,contentH-h)); draw(); }
};

// ── TabWidget ─────────────────────────────────────────────────────────────────
class TabWidget : public Widget {
public:
  std::vector<String> tabNames;
  std::vector<std::vector<Widget*>> pages;
  int activeTab=0;
  int tabBarH=24;
  TabWidget(std::vector<String> tabs={}) : tabNames(tabs) {
    style.bg=T().bg;
    for (int i=0;i<(int)tabs.size();i++) pages.push_back({});
  }
  String type() override { return "TabWidget"; }
  Panel* page(int idx) {
    // Return a temporary panel-like accessor — for simplicity return this widget
    // In Lua this is used as tabs:page(1):add(widget)
    return nullptr; // handled in Lua binding
  }
  void addToPage(int idx, Widget* w2) {
    while ((int)pages.size()<=idx) pages.push_back({});
    pages[idx].push_back(w2);
  }
  void addTab(const String& name) { tabNames.push_back(name); pages.push_back({}); }
  void draw() override {
    if (!style.visible) return;
    // Tab bar
    int tabW=w/max(1,(int)tabNames.size());
    for (int i=0;i<(int)tabNames.size();i++) {
      bool sel=(i==activeTab);
      uint16_t tbg=sel?T().accent:T().input;
      uint16_t tfg=sel?T().accentFg:T().inputFg;
      tft.fillRoundRect(x+i*tabW,y,tabW,tabBarH,4,tbg);
      tft.drawRoundRect(x+i*tabW,y,tabW,tabBarH,4,T().border);
      tft.setTextColor(tfg,tbg); tft.setTextSize(1);
      int tx2=x+i*tabW+(tabW-tabNames[i].length()*6)/2;
      tft.setCursor(tx2,y+(tabBarH-8)/2); tft.print(tabNames[i]);
    }
    // Content
    tft.fillRect(x,y+tabBarH,w,h-tabBarH,T().bg);
    if (activeTab<(int)pages.size()) for (auto* w2:pages[activeTab]) w2->draw();
  }
  bool onTouch(int tx,int ty) override {
    if (!style.visible) return false;
    if (tx>=x&&tx<=x+w&&ty>=y&&ty<y+tabBarH) {
      int tabW=w/max(1,(int)tabNames.size());
      int tab=(tx-x)/tabW;
      if (tab!=activeTab&&tab<(int)tabNames.size()) {
        activeTab=tab;
        tft.fillRect(x,y+tabBarH,w,h-tabBarH,T().bg);
        draw(); emit("selected",String(tab));
      }
      return true;
    }
    if (activeTab<(int)pages.size())
      for (auto it=pages[activeTab].rbegin();it!=pages[activeTab].rend();++it)
        if ((*it)->onTouch(tx,ty)) return true;
    return false;
  }
  std::vector<Widget*>& currentPage() { return pages[activeTab]; }
};

// ════════════════════════════════════════════════════════════════════════════
// LAYOUTS
// ════════════════════════════════════════════════════════════════════════════

struct LayoutEntry { Widget* w; int fixedSize=-1; float stretch=1.0f; };

// ── VBox ──────────────────────────────────────────────────────────────────────
class VBox {
public:
  int x,y,w,spacing;
  std::vector<LayoutEntry> entries;
  VBox(int x2=0,int y2=0,int w2=240,int sp=4) : x(x2),y(y2),w(w2),spacing(sp) {}
  VBox* add(Widget* wg, int fixedH=-1, float stretch=1.0f) {
    entries.push_back({wg,fixedH,stretch}); reflow(); return this;
  }
  void reflow() {
    int curY=y;
    for (auto& e:entries) {
      int h2=e.fixedSize>0?e.fixedSize:24;
      e.w->x=x+e.w->style.margin; e.w->y=curY;
      e.w->w=w-e.w->style.margin*2;
      e.w->h=h2;
      curY+=h2+spacing;
    }
  }
  void draw() { for (auto& e:entries) e.w->draw(); }
  bool onTouch(int tx,int ty) { for (auto it=entries.rbegin();it!=entries.rend();++it) if (it->w->onTouch(tx,ty)) return true; return false; }
  int totalHeight() { int h=0; for (auto& e:entries) h+=max(e.fixedSize,24)+spacing; return h; }
};

// ── HBox ──────────────────────────────────────────────────────────────────────
class HBox {
public:
  int x,y,h,spacing;
  std::vector<LayoutEntry> entries;
  HBox(int x2=0,int y2=0,int h2=28,int sp=4) : x(x2),y(y2),h(h2),spacing(sp) {}
  HBox* add(Widget* wg, int fixedW=-1, float stretch=1.0f) {
    entries.push_back({wg,fixedW,stretch}); reflow(); return this;
  }
  void reflow() {
    // Distribute stretch widgets
    int fixed=0, stretchCount=0;
    for (auto& e:entries) { if (e.fixedSize>0) fixed+=e.fixedSize+spacing; else stretchCount++; }
    int stretchW=stretchCount>0?(240-fixed-x)/stretchCount:0;
    int curX=x;
    for (auto& e:entries) {
      int w2=e.fixedSize>0?e.fixedSize:stretchW;
      e.w->x=curX; e.w->y=y+e.w->style.margin;
      e.w->w=w2; e.w->h=h-e.w->style.margin*2;
      curX+=w2+spacing;
    }
  }
  void draw() { for (auto& e:entries) e.w->draw(); }
  bool onTouch(int tx,int ty) { for (auto it=entries.rbegin();it!=entries.rend();++it) if (it->w->onTouch(tx,ty)) return true; return false; }
};

// ── Grid ──────────────────────────────────────────────────────────────────────
class Grid {
public:
  int x,y,w,cols,gap,rowH;
  std::vector<Widget*> cells;
  Grid(int x2=0,int y2=0,int w2=240,int cols2=2,int gap2=4,int rh=28)
    : x(x2),y(y2),w(w2),cols(cols2),gap(gap2),rowH(rh) {}
  Grid* add(Widget* wg) { cells.push_back(wg); reflow(); return this; }
  void reflow() {
    int cellW=(w-(cols+1)*gap)/cols;
    for (int i=0;i<(int)cells.size();i++) {
      int col=i%cols, row=i/cols;
      cells[i]->x=x+gap+col*(cellW+gap);
      cells[i]->y=y+gap+row*(rowH+gap);
      cells[i]->w=cellW; cells[i]->h=rowH;
    }
  }
  void draw() { for (auto* c:cells) c->draw(); }
  bool onTouch(int tx,int ty) { for (auto it=cells.rbegin();it!=cells.rend();++it) if ((*it)->onTouch(tx,ty)) return true; return false; }
  int totalHeight() { return ((cells.size()+cols-1)/cols)*(rowH+gap)+gap; }
};

// ── Stack (QStackedWidget equivalent) ────────────────────────────────────────
class Stack {
public:
  int x,y,w,h;
  std::vector<std::vector<Widget*>> pages;
  int currentPage=0;
  Stack(int x2=0,int y2=0,int w2=240,int h2=280) : x(x2),y(y2),w(w2),h(h2) {}
  Stack* addPage() { pages.push_back({}); return this; }
  Stack* addToPage(int idx,Widget* wg) { while((int)pages.size()<=idx)pages.push_back({}); pages[idx].push_back(wg); return this; }
  Stack* setPage(int idx) {
    currentPage=constrain(idx,0,(int)pages.size()-1);
    tft.fillRect(x,y,w,h,T().bg);
    draw(); return this;
  }
  void draw() { if (currentPage<(int)pages.size()) for (auto* wg:pages[currentPage]) wg->draw(); }
  bool onTouch(int tx,int ty) {
    if (currentPage<(int)pages.size())
      for (auto it=pages[currentPage].rbegin();it!=pages[currentPage].rend();++it)
        if ((*it)->onTouch(tx,ty)) return true;
    return false;
  }
};

// ── Form (label+widget pairs) ─────────────────────────────────────────────────
class Form {
public:
  int x,y,w,rowH,gap,labelW;
  std::vector<std::pair<Label*,Widget*>> rows;
  Form(int x2=0,int y2=0,int w2=240,int rh=28,int g=4,int lw=80)
    : x(x2),y(y2),w(w2),rowH(rh),gap(g),labelW(lw) {}
  Form* addRow(const String& label, Widget* wg) {
    Label* lbl=new Label(label);
    rows.push_back({lbl,wg}); reflow(); return this;
  }
  void reflow() {
    for (int i=0;i<(int)rows.size();i++) {
      auto [lbl,wg]=rows[i];
      lbl->x=x; lbl->y=y+i*(rowH+gap); lbl->w=labelW; lbl->h=rowH;
      lbl->style.align="right"; lbl->style.bg=T().bg; lbl->style.color=T().fg;
      wg->x=x+labelW+gap; wg->y=y+i*(rowH+gap); wg->w=w-labelW-gap; wg->h=rowH;
    }
  }
  void draw() { for (auto&[l,w2]:rows) { l->draw(); w2->draw(); } }
  bool onTouch(int tx,int ty) { for (auto&[l,w2]:rows) if (w2->onTouch(tx,ty)) return true; return false; }
  int totalHeight() { return rows.size()*(rowH+gap); }
};

// ════════════════════════════════════════════════════════════════════════════
// DIALOGS
// ════════════════════════════════════════════════════════════════════════════

inline void _drawDialogBase(const String& title, int dw=200, int dh=160) {
  int dx=(240-dw)/2, dy=(320-dh)/2;
  // Backdrop dimming
  for (int i=0;i<4;i++) tft.drawRect(i,i,240-i*2,320-i*2,dimColor(T().shadow,0.5f));
  tft.fillRoundRect(dx,dy,dw,dh,8,T().bg);
  tft.drawRoundRect(dx,dy,dw,dh,8,T().border);
  tft.fillRoundRect(dx,dy,dw,24,8,T().accent);
  tft.fillRect(dx,dy+12,dw,12,T().accent);
  tft.setTextColor(T().accentFg,T().accent); tft.setTextSize(1);
  tft.setCursor(dx+(dw-title.length()*6)/2,dy+8); tft.print(title);
}

inline void _dialogText(const String& msg, int dx, int dy, int dw) {
  tft.setTextColor(T().fg,T().bg); tft.setTextSize(1);
  int lx=dx+8, ly=dy+32;
  String word="", line="";
  for (int i=0;i<=(int)msg.length();i++) {
    char c=i<(int)msg.length()?msg[i]:' ';
    if (c==' '||c=='\n') {
      if ((int)(line.length()+word.length())*6>dw-16) { tft.setCursor(lx,ly); tft.print(line); ly+=14; line=""; }
      line+=word+" "; word=""; if (c=='\n') { tft.setCursor(lx,ly); tft.print(line); ly+=14; line=""; }
    } else word+=c;
  }
  if (!line.isEmpty()) { tft.setCursor(lx,ly); tft.print(line); }
}

inline void uiAlert(const String& msg, const String& title="Notice") {
  int dw=200,dh=140,dx=(240-dw)/2,dy=(320-dh)/2;
  _drawDialogBase(title,dw,dh);
  _dialogText(msg,dx,dy,dw);
  // OK button
  tft.fillRoundRect(dx+60,dy+dh-36,80,26,6,T().accent);
  tft.setTextColor(T().accentFg,T().accent); tft.setCursor(dx+82,dy+dh-24); tft.print("OK");
  unsigned long t=millis()+30000;
  while (millis()<t) {
    if (touch.tirqTouched()&&touch.touched()) {
      TS_Point p=touch.getPoint();
      int tx=map(p.x,200,3800,0,240), ty=map(p.y,200,3800,0,320);
      if (tx>=dx+60&&tx<=dx+140&&ty>=dy+dh-36&&ty<=dy+dh-10) { delay(80); return; }
      delay(80);
    }
    delay(20);
  }
}

inline bool uiConfirm(const String& msg, const String& title="Confirm?") {
  int dw=200,dh=150,dx=(240-dw)/2,dy=(320-dh)/2;
  _drawDialogBase(title,dw,dh);
  _dialogText(msg,dx,dy,dw);
  tft.fillRoundRect(dx+8,   dy+dh-36,86,26,6,T().success);
  tft.fillRoundRect(dx+106, dy+dh-36,86,26,6,T().danger);
  tft.setTextColor(0xFFFF,T().success); tft.setCursor(dx+34,dy+dh-24); tft.print("YES");
  tft.setTextColor(0xFFFF,T().danger);  tft.setCursor(dx+134,dy+dh-24); tft.print("NO");
  unsigned long t=millis()+30000;
  while (millis()<t) {
    if (touch.tirqTouched()&&touch.touched()) {
      TS_Point p=touch.getPoint(); delay(80);
      int tx=map(p.x,200,3800,0,240), ty=map(p.y,200,3800,0,320);
      if (ty>=dy+dh-36&&ty<=dy+dh-10) {
        if (tx>=dx+8&&tx<=dx+94) return true;
        if (tx>=dx+106) return false;
      }
    }
    delay(20);
  }
  return false;
}

inline String uiPrompt(const String& label, const String& defaultVal="") {
  int dw=220,dh=120,dx=(240-dw)/2,dy=(320-dh)/2;
  _drawDialogBase(label,dw,dh);
  tft.setTextColor(T().fg,T().bg); tft.setTextSize(1);
  tft.setCursor(dx+8,dy+32); tft.print(label);
  return VKeyboard::open(label,defaultVal);
}

inline String uiPick(std::vector<String> items, const String& label="Choose:") {
  int dw=220,dh=min(300,(int)(items.size()*24+60));
  int dx=(240-dw)/2,dy=(320-dh)/2;
  _drawDialogBase(label,dw,dh);
  for (int i=0;i<(int)items.size();i++) {
    tft.fillRoundRect(dx+4,dy+30+i*24,dw-8,22,4,T().input);
    tft.setTextColor(T().inputFg,T().input); tft.setTextSize(1);
    tft.setCursor(dx+12,dy+38+i*24); tft.print(items[i]);
  }
  unsigned long t=millis()+30000;
  while (millis()<t) {
    if (touch.tirqTouched()&&touch.touched()) {
      TS_Point p=touch.getPoint(); delay(80);
      int tx=map(p.x,200,3800,0,240), ty=map(p.y,200,3800,0,320);
      if (tx>=dx&&tx<=dx+dw) {
        int idx=(ty-dy-30)/24;
        if (idx>=0&&idx<(int)items.size()) { return items[idx]; }
      }
    }
    delay(20);
  }
  return "";
}

inline String uiFilePick(const String& startPath="/") {
  // Simple file browser dialog
  std::vector<String> files;
  File root=SPIFFS.open(startPath);
  if (root&&root.isDirectory()) {
    File f=root.openNextFile();
    while (f) { files.push_back(String(f.name())); f=root.openNextFile(); }
  }
  if (files.empty()) { uiAlert("No files in "+startPath); return ""; }
  return uiPick(files,"Select file:");
}

inline void uiProgress(const String& label, std::function<void(std::function<void(int)>)> fn) {
  int dw=200,dh=80,dx=(240-dw)/2,dy=(320-dh)/2;
  _drawDialogBase(label,dw,dh);
  tft.fillRoundRect(dx+8,dy+46,dw-16,20,4,T().input);
  tft.drawRoundRect(dx+8,dy+46,dw-16,20,4,T().border);
  auto update=[&](int pct) {
    pct=constrain(pct,0,100);
    int fw=(dw-16)*pct/100;
    tft.fillRoundRect(dx+8,dy+46,fw,20,4,T().accent);
    tft.setTextColor(T().fg,T().bg); tft.setTextSize(1);
    String ps=String(pct)+"%";
    tft.fillRect(dx+(dw-ps.length()*6)/2-2,dy+50,ps.length()*6+4,12,fw>(dw-16)/2?T().accent:T().input);
    tft.setTextColor(fw>(dw-16)/2?T().accentFg:T().inputFg,fw>(dw-16)/2?T().accent:T().input);
    tft.setCursor(dx+(dw-ps.length()*6)/2,dy+51); tft.print(ps);
  };
  fn(update);
  delay(300);
}

// ════════════════════════════════════════════════════════════════════════════
// APP (Main Window / Event Loop)
// ════════════════════════════════════════════════════════════════════════════

class App {
public:
  String title;
  std::vector<Widget*> widgets;
  std::vector<VBox*>   vboxes;
  std::vector<HBox*>   hboxes;
  std::vector<Grid*>   grids;
  std::vector<Stack*>  stacks;
  std::vector<Form*>   forms;
  bool running=false;
  bool showTitleBar=true;
  std::vector<std::pair<unsigned long,std::function<void()>>> timers;
  std::vector<unsigned long> timerLast;
  int swipeStartY=-1;
  Widget* focusedWidget=nullptr;
  String focusPolicy="touch"; // "touch" or "cycle"
  int focusIdx=-1;

  App(const String& t) : title(t) {}

  App* add(Widget* w2) { widgets.push_back(w2); return this; }
  App* addLayout(VBox* l) { vboxes.push_back(l); return this; }
  App* addLayout(HBox* l) { hboxes.push_back(l); return this; }
  App* addLayout(Grid* l) { grids.push_back(l); return this; }
  App* addLayout(Stack* l) { stacks.push_back(l); return this; }
  App* addLayout(Form* l) { forms.push_back(l); return this; }

  App* onTimer(int ms, std::function<void()> fn) {
    timers.push_back({(unsigned long)ms,fn});
    timerLast.push_back(millis());
    return this;
  }

  App* setFocusPolicy(const String& p) { focusPolicy=p; return this; }
  App* theme(const String& t) { setTheme(t); return this; }

  void drawAll() {
    if (showTitleBar) {
      tft.fillRect(0,0,240,28,T().accent);
      tft.setTextColor(T().accentFg,T().accent); tft.setTextSize(1);
      tft.setCursor((240-title.length()*6)/2,10); tft.print(title);
      // Exit X
      tft.fillRoundRect(216,4,20,20,4,T().danger);
      tft.setTextColor(0xFFFF,T().danger); tft.setCursor(221,9); tft.print("X");
      tft.drawFastHLine(0,28,240,T().border);
    }
    for (auto* w2:widgets) w2->draw();
    for (auto* l:vboxes)  l->draw();
    for (auto* l:hboxes)  l->draw();
    for (auto* l:grids)   l->draw();
    for (auto* l:stacks)  l->draw();
    for (auto* l:forms)   l->draw();
  }

  void redraw() { tft.fillScreen(T().bg); drawAll(); }

  void run() {
    running=true;
    tft.fillScreen(T().bg);
    drawAll();

    while (running) {
      // Timers
      for (int i=0;i<(int)timers.size();i++) {
        if (millis()-timerLast[i]>=timers[i].first) {
          timerLast[i]=millis();
          timers[i].second();
        }
      }

      tickToast();

      if (touch.tirqTouched()&&touch.touched()) {
        TS_Point p=touch.getPoint();
        int tx=map(p.x,200,3800,0,240);
        int ty=map(p.y,200,3800,0,320);
        swipeStartY=ty;
        delay(60);

        // Exit button
        if (showTitleBar&&tx>=216&&tx<=236&&ty>=4&&ty<=24) { running=false; break; }

        // Dispatch to widgets
        bool handled=false;
        for (auto* w2:widgets) if (w2->onTouch(tx,ty)) { handled=true; break; }
        if (!handled) for (auto* l:vboxes)  if (l->onTouch(tx,ty)) { handled=true; break; }
        if (!handled) for (auto* l:hboxes)  if (l->onTouch(tx,ty)) { handled=true; break; }
        if (!handled) for (auto* l:grids)   if (l->onTouch(tx,ty)) { handled=true; break; }
        if (!handled) for (auto* l:stacks)  if (l->onTouch(tx,ty)) { handled=true; break; }
        if (!handled) for (auto* l:forms)   if (l->onTouch(tx,ty)) { handled=true; break; }

        // Focus cycling (swipe right/left)
        if (!handled&&focusPolicy=="cycle") {
          delay(150);
          if (touch.touched()) {
            TS_Point p2=touch.getPoint();
            int ty2=map(p2.y,200,3800,0,320);
            int tx2=map(p2.x,200,3800,0,240);
            int dx=tx2-tx, dy2=ty2-swipeStartY;
            if (abs(dx)>abs(dy2)&&abs(dx)>30) {
              // Cycle focus
              if (focusedWidget) focusedWidget->clearFocus();
              if (dx>0) focusIdx=(focusIdx+1)%widgets.size();
              else focusIdx=(focusIdx-1+widgets.size())%widgets.size();
              if (!widgets.empty()) { focusedWidget=widgets[focusIdx]; focusedWidget->setFocus(); }
            }
          }
        }
      }
      delay(16);
    }
  }
};

} // namespace NoorUI
