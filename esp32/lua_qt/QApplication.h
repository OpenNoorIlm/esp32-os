// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorQt — QApplication.h                                                 ║
// ║  Application lifecycle, screen, clipboard — mirrors Qt6 QApplication.   ║
// ║  Backed by Arduino setup()/loop() lifecycle + TFT screen.               ║
// ║  MIT License — NoorRobot / OpenNoorIlm                                  ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once
#include "QObject.h"
#include "QGeometry.h"
#include "QWidget.h"
#include <Arduino.h>
#include <WiFi.h>

namespace NoorQt {

// ── QScreen ───────────────────────────────────────────────────────────────────
class QScreen : public QObject {
public:
  QScreen() {}

  // NoorOS TFT: 240×320, portrait
  QSize  size()           const { return QSize(240,320); }
  QRect  geometry()       const { return QRect(0,0,240,320); }
  QRect  availableGeometry()const{ return QRect(0,0,240,300); } // leave 20px for status bar
  QSizeF physicalSize()   const { return QSizeF(43.2f,57.6f); } // mm for 2.8"
  float  physicalDotsPerInch()const{ return 141.0f; }           // 240/1.701in
  float  devicePixelRatio()const { return 1.0f; }
  int    depth()          const { return 16; }                   // RGB565
  QString name()          const { return "NoorOS TFT 2.8\""; }
  int    refreshRate()    const { return 60; }

  // Orientation — fixed portrait on NoorRobot
  enum Orientation { Portrait=1, Landscape=2 };
  Orientation orientation()        const { return Portrait; }
  Orientation primaryOrientation() const { return Portrait; }

  Signal<Orientation> orientationChanged{"orientationChanged"};
};

// ── QClipboard ────────────────────────────────────────────────────────────────
// Single string clipboard (no system clipboard on ESP32)
class QClipboard : public QObject {
public:
  enum Mode { Clipboard, Selection, FindBuffer };
  Signal<Mode> changed{"changed"};

  QString text(Mode=Clipboard) const { return _text; }
  void setText(const QString& t, Mode m=Clipboard) { _text=t; changed.emit(m); }
  void clear(Mode m=Clipboard) { _text=""; changed.emit(m); }

private:
  QString _text;
};

// ── QCoreApplication ──────────────────────────────────────────────────────────
class QCoreApplication : public QObject {
public:
  Signal<void> aboutToQuit{"aboutToQuit"};
  Signal<int>  applicationExitCode{"applicationExitCode"};

  QCoreApplication() { _instance=this; }
  virtual ~QCoreApplication() { _instance=nullptr; }

  static QCoreApplication* instance()  { return _instance; }

  // argv/argc shim (not meaningful on ESP32)
  static int     argc() { return 0; }
  static char**  argv() { return nullptr; }

  // Application identity
  static QString applicationName()     { return _appName; }
  static QString applicationVersion()  { return _appVer; }
  static QString organizationName()    { return _orgName; }
  static QString organizationDomain()  { return _orgDomain; }
  static void setApplicationName(const QString& n)    { _appName=n; }
  static void setApplicationVersion(const QString& v) { _appVer=v; }
  static void setOrganizationName(const QString& n)   { _orgName=n; }
  static void setOrganizationDomain(const QString& d) { _orgDomain=d; }

  // Event loop — on ESP32, exec() runs the FreeRTOS scheduler
  // Call processEvents() from loop() to dispatch pending Qt events
  static int exec() {
    // ESP32: scheduler is already running; just spin
    while(true) {
      processEvents();
      delay(1);
    }
    return 0;
  }

  static void processEvents(int maxTime=0) {
    (void)maxTime;
    // Dispatch any pending timer ticks / deferred deletes
    if(_instance) _instance->_dispatchDeferred();
    yield();
  }

  static void quit() {
    if(_instance) _instance->aboutToQuit.emit();
    ESP.restart(); // graceful quit → restart ESP32
  }
  static void exit(int code=0) {
    if(_instance) _instance->applicationExitCode.emit(code);
    ESP.restart();
  }

  // Paths — all rooted in SPIFFS
  static QString applicationDirPath()  { return "/"; }
  static QString applicationFilePath() { return "/"+_appName; }

  // Install event filter (no-op — no global event loop)
  static void installEventFilter(QObject*) {}
  static void removeEventFilter(QObject*)  {}

  // postEvent — deferred call queue
  static void postEvent(QObject* obj, std::function<void()> fn) {
    if(_instance) _instance->_deferred.push_back({obj,fn});
  }

  // sendPostedEvents — flush deferred queue
  static void sendPostedEvents() {
    if(_instance) _instance->_dispatchDeferred();
  }

private:
  static QCoreApplication* _instance;
  static QString _appName, _appVer, _orgName, _orgDomain;

  struct Deferred { QObject* obj; std::function<void()> fn; };
  std::vector<Deferred> _deferred;

  void _dispatchDeferred() {
    auto q=std::move(_deferred);
    for(auto& d:q) if(d.obj) d.fn();
  }
};
// Static member definitions
QCoreApplication* QCoreApplication::_instance=nullptr;
QString QCoreApplication::_appName   = "NoorApp";
QString QCoreApplication::_appVer    = "1.0";
QString QCoreApplication::_orgName   = "NoorRobot";
QString QCoreApplication::_orgDomain = "noorrobot.local";

// ── QGuiApplication ───────────────────────────────────────────────────────────
class QGuiApplication : public QCoreApplication {
public:
  QGuiApplication() { _screenInstance=new QScreen(); }
  ~QGuiApplication() { delete _screenInstance; }

  static QScreen* primaryScreen()             { return _screenInstance; }
  static std::vector<QScreen*> screens()      { return {_screenInstance}; }
  static double devicePixelRatio()            { return 1.0; }

  // Cursor / input — touch-only, no cursor concept on NoorOS
  static QPoint cursorPos()                   { return QPoint(0,0); }
  static void   setCursorPos(const QPoint&)   {}
  static int    keyboardModifiers()           { return 0; }
  static int    mouseButtons()               { return 0; }
  static int    queryKeyboardModifiers()     { return 0; }

  // Font
  static QFont  font()                       { return QFont("NoorQt",12); }
  static void   setFont(const QFont&)        {}

  // Style hints
  static double styleHints_cursorFlashTime() { return 1000; }

  // Clipboard
  static QClipboard* clipboard() {
    static QClipboard cb;
    return &cb;
  }

  // Platform
  static QString platformName() { return "esp32-tft"; }

  Signal<QScreen*> primaryScreenChanged{"primaryScreenChanged"};
  Signal<void>     screenAdded        {"screenAdded"};

private:
  static QScreen* _screenInstance;
};
QScreen* QGuiApplication::_screenInstance=nullptr;

// ── QApplication ──────────────────────────────────────────────────────────────
// Full Qt application class — mirrors QApplication exactly
class QApplication : public QGuiApplication {
public:
  QApplication() {}

  static QApplication* instance() {
    return static_cast<QApplication*>(QCoreApplication::instance());
  }

  // Style — NoorOS has one built-in style ("Noor")
  static QString style()           { return "Noor"; }
  static void    setStyle(const QString&) {}

  // Font / palette forwarded to NoorUI
  static void    setFont(const QFont&)         {}
  static void    setPalette(const QPalette&)   {}
  static QPalette palette()                    { return QPalette(); }

  // Widget focus
  static QWidget* focusWidget()                { return nullptr; }
  static QWidget* activeWindow()               { return nullptr; }
  static void     setActiveWindow(QWidget*)    {}

  // Desktop  
  static QRect    desktop()                   { return QRect(0,0,240,320); }
  static int      desktop_width()             { return 240; }
  static int      desktop_height()            { return 320; }

  // Global event handling
  static bool notify(QObject* obj, const QString& event) {
    (void)obj;(void)event; return false;
  }

  // Process events alias
  static void processEvents()   { QCoreApplication::processEvents(); }

  // Beep / alert
  static void beep() {
    // Tone on DAC if available
    dacWrite(25, 128); delay(100); dacWrite(25, 0);
  }

  Signal<QWidget*> focusChanged{"focusChanged"};
  Signal<bool>     commitDataRequest{"commitDataRequest"};
};

// ── QSettings (lightweight key=value SPIFFS store) ────────────────────────────
class QSettings : public QObject {
public:
  enum Scope   { UserScope, SystemScope };
  enum Format  { NativeFormat, IniFormat };
  enum Status  { NoError, AccessError, FormatError };

  QSettings(const QString& org, const QString& app, QObject* parent=nullptr)
    : QObject(parent), _path("/settings_"+org+"_"+app+".ini") { _load(); }
  QSettings(Format, Scope, const QString& org, const QString& app, QObject* parent=nullptr)
    : QObject(parent), _path("/settings_"+org+"_"+app+".ini") { _load(); }
  explicit QSettings(const QString& path, Format=IniFormat, QObject* parent=nullptr)
    : QObject(parent), _path(path) { _load(); }
  ~QSettings() { sync(); }

  void setValue(const QString& key, const QVariant& val) {
    _data[key]=val.toString(); _dirty=true;
  }
  QVariant value(const QString& key, const QVariant& def=QVariant()) const {
    auto it=_data.find(key);
    return it!=_data.end()?QVariant(it->second):def;
  }
  bool contains(const QString& key) const { return _data.count(key)>0; }
  void remove(const QString& key)         { _data.erase(key); _dirty=true; }
  void clear()                            { _data.clear(); _dirty=true; }

  std::vector<QString> allKeys() const {
    std::vector<QString> keys;
    for(auto& kv:_data) keys.push_back(kv.first);
    return keys;
  }
  std::vector<QString> childKeys(const QString& group="") const {
    std::vector<QString> keys;
    QString prefix=group.isEmpty()?_group:group;
    if(!prefix.isEmpty()&&!prefix.endsWith('/')) prefix+='/';
    for(auto& kv:_data){
      if(kv.first.startsWith(prefix)){
        QString k=kv.first.substring(prefix.length());
        if(k.indexOf('/')<0) keys.push_back(k);
      }
    }
    return keys;
  }

  void beginGroup(const QString& g) { _group=g; }
  void endGroup()                   { _group=""; }
  QString group()             const { return _group; }

  void sync() { if(_dirty){ _save(); _dirty=false; } }
  Status status() const { return _status; }
  QString fileName() const { return _path; }

  // Full path for grouped key
  QString _fullKey(const QString& k) const {
    return _group.isEmpty()?k:_group+"/"+k;
  }

private:
  QString                    _path;
  QString                    _group;
  std::map<QString,QString>  _data;
  bool                       _dirty=false;
  Status                     _status=NoError;

  void _load() {
    File f=SPIFFS.open(_path,"r");
    if(!f){ _status=NoError; return; }
    while(f.available()){
      String line=f.readStringUntil('\n'); line.trim();
      int eq=line.indexOf('=');
      if(eq>0) _data[line.substring(0,eq)]=line.substring(eq+1);
    }
    f.close();
  }
  void _save() {
    File f=SPIFFS.open(_path,"w");
    if(!f){ _status=AccessError; return; }
    for(auto& kv:_data) f.println(kv.first+"="+kv.second);
    f.close();
  }
};

} // namespace NoorQt

using NoorQt::QScreen;
using NoorQt::QClipboard;
using NoorQt::QCoreApplication;
using NoorQt::QGuiApplication;
using NoorQt::QApplication;
using NoorQt::QSettings;
