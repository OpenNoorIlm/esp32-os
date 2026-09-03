// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorQt — QObject.h                                                      ║
// ║  Base class for all NoorQt objects.                                      ║
// ║  Mirrors Qt6 QObject API exactly.                                        ║
// ║  MIT License — NoorRobot / OpenNoorIlm                                  ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once
#include <Arduino.h>
#include <functional>
#include <vector>
#include <map>
#include <memory>
#include <typeinfo>
#include <algorithm>

// Arduino defines min/max as macros that conflict with std::min/max and
// std::vector iterator arithmetic. Undefine them here so C++ templates work.
#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif
using std::min;
using std::max;

// ── Primitive typedefs (mirrors Qt's global typedefs) ─────────────────────────
typedef long long  qint64;
typedef int        qint32;
typedef short      qint16;
typedef signed char qint8;
typedef unsigned long long  quint64;
typedef unsigned int        quint32;
typedef unsigned short      quint16;
typedef unsigned char       quint8;
typedef double     qreal;

// ── Chip detection — scales memory usage automatically ────────────────────────
#ifndef NOORQT_CHIP_TIER
  #if CONFIG_IDF_TARGET_ESP32P4
    #define NOORQT_CHIP_TIER 3   // ESP32-P4: 32MB RAM — full Qt-level
    #define NOORQT_MAX_CHILDREN  256
    #define NOORQT_MAX_SIGNALS   128
    #define NOORQT_MAX_PROPS     256
    #define NOORQT_SIGNAL_QUEUE  64
  #elif CONFIG_IDF_TARGET_ESP32S3
    #define NOORQT_CHIP_TIER 2   // ESP32-S3: 512KB — moderate
    #define NOORQT_MAX_CHILDREN  64
    #define NOORQT_MAX_SIGNALS   32
    #define NOORQT_MAX_PROPS     64
    #define NOORQT_SIGNAL_QUEUE  16
  #else
    #define NOORQT_CHIP_TIER 1   // ESP32 classic: 240KB — lean
    #define NOORQT_MAX_CHILDREN  16
    #define NOORQT_MAX_SIGNALS   16
    #define NOORQT_MAX_PROPS     32
    #define NOORQT_SIGNAL_QUEUE  8
  #endif
#endif

// ── Version ───────────────────────────────────────────────────────────────────
#define NOORQT_VERSION_MAJOR 1
#define NOORQT_VERSION_MINOR 0
#define NOORQT_VERSION_PATCH 0
#define NOORQT_VERSION_STR   "1.0.0"

namespace NoorQt {

// ── Forward declarations ──────────────────────────────────────────────────────
class QObject;
class QEvent;
class QChildEvent;
class QTimerEvent;
class QMetaObject;

// ── QVariant — type-erased value (mirrors Qt6 QVariant) ──────────────────────
class QVariant {
public:
  enum Type { Invalid, Bool, Int, UInt, LongLong, ULongLong,
              Double, Float, Char, StringType, ByteArray, Void };

  QVariant() : _type(Invalid) {}
  explicit QVariant(bool v)        : _type(Bool),   _i(v) {}
  explicit QVariant(int v)         : _type(Int),    _i(v) {}
  explicit QVariant(unsigned int v): _type(UInt),   _i(v) {}
  explicit QVariant(long long v)   : _type(LongLong),_i(v) {}
  explicit QVariant(double v)      : _type(Double), _d(v) {}
  explicit QVariant(float v)       : _type(Float),  _d(v) {}
  explicit QVariant(const ::String& v): _type(StringType), _s(v) {}
  explicit QVariant(const char* v) : _type(StringType), _s(v) {}

  Type    type()      const { return _type; }
  bool    isValid()   const { return _type != Invalid; }
  bool    isNull()    const { return _type == Invalid; }

  bool        toBool()   const { return _type==Bool||_type==Int ? (bool)_i : _s=="true"; }
  int         toInt()    const { return _type==StringType ? _s.toInt() : (int)_i; }
  long long   toLongLong()const{ return _i; }
  double      toDouble() const { return _type==StringType ? _s.toDouble() : _d; }
  float       toFloat()  const { return (float)toDouble(); }
  ::String    toString() const {
    switch(_type) {
      case Bool:   return _i?"true":"false";
      case Int: case UInt: case LongLong: return ::String((long)_i);
      case Double: case Float: return ::String(_d);
      case StringType: return _s;
      default: return "";
    }
  }

  bool operator==(const QVariant& o) const {
    if (_type!=o._type) return false;
    if (_type==StringType) return _s==o._s;
    if (_type==Double||_type==Float) return _d==o._d;
    return _i==o._i;
  }
  bool operator!=(const QVariant& o) const { return !(*this==o); }

private:
  Type      _type;
  long long _i = 0;
  double    _d = 0;
  ::String  _s;
};

// ── QMetaProperty ─────────────────────────────────────────────────────────────
struct QMetaProperty {
  String name;
  QVariant::Type type;
  bool readable   = true;
  bool writable   = true;
  bool notifiable = false;
  String notifySignal;
  std::function<QVariant()>          getter;
  std::function<void(QVariant)>      setter;
};

// ── Connection handle (like QMetaObject::Connection) ─────────────────────────
struct QConnection {
  int id = -1;
  bool connected = false;
  explicit operator bool() const { return connected; }
};

// ── QObject ───────────────────────────────────────────────────────────────────
class QObject {
public:
  // ── Constructor / Destructor ───────────────────────────────────────────────
  explicit QObject(QObject* parent = nullptr) : _parent(parent) {
    static int _idCounter = 0;
    _objectId = ++_idCounter;
    if (parent) parent->_addChild(this);
  }

  virtual ~QObject() {
    // Disconnect all signals/slots
    disconnectAll();
    // Remove from parent
    if (_parent) _parent->_removeChild(this);
    // Delete children
    for (auto* c : _children) { c->_parent = nullptr; delete c; }
    _children.clear();
    emit_destroyed();
  }

  // ── Object name (objectName property) ─────────────────────────────────────
  void    setObjectName(const String& name) { _objectName = name; emit_propertyChanged("objectName"); }
  String  objectName() const { return _objectName; }

  // ── Parent / Children ──────────────────────────────────────────────────────
  QObject* parent()                              const { return _parent; }
  void     setParent(QObject* p) {
    if (_parent) _parent->_removeChild(this);
    _parent = p;
    if (p) p->_addChild(this);
  }
  const std::vector<QObject*>& children()       const { return _children; }
  bool isAncestorOf(const QObject* child)        const {
    for (auto* c : _children) { if (c==child||c->isAncestorOf(child)) return true; }
    return false;
  }

  // ── findChild / findChildren (mirrors Qt6) ─────────────────────────────────
  // ESP32 uses -fno-rtti so dynamic_cast is unavailable.
  // These are stub implementations — not called by any Lua/shell code.
  template<typename T>
  T* findChild(const String& name="") const { return nullptr; }

  template<typename T>
  std::vector<T*> findChildren(const String& name="") const { return {}; }

  // ── Properties (Q_PROPERTY equivalent) ────────────────────────────────────
  void registerProperty(const QMetaProperty& prop) {
    _properties[prop.name] = prop;
  }

  QVariant property(const String& name) const {
    auto it=_properties.find(name);
    if (it!=_properties.end()&&it->second.getter) return it->second.getter();
    auto it2=_dynamicProps.find(name);
    if (it2!=_dynamicProps.end()) return it2->second;
    return QVariant();
  }

  bool setProperty(const String& name, const QVariant& value) {
    auto it=_properties.find(name);
    if (it!=_properties.end()&&it->second.writable&&it->second.setter) {
      it->second.setter(value);
      if (it->second.notifiable) emit_signal(it->second.notifySignal,{value});
      emit_propertyChanged(name);
      return true;
    }
    // Dynamic property
    _dynamicProps[name]=value;
    emit_propertyChanged(name);
    return true;
  }

  std::vector<String> dynamicPropertyNames() const {
    std::vector<String> names;
    for (auto& kv:_dynamicProps) names.push_back(kv.first);
    return names;
  }

  // ── Signal / Slot system ───────────────────────────────────────────────────
  // connect(sender, "signal", receiver, "slot")
  // connect(sender, "signal", fn)
  // Returns a QConnection handle

  using SlotFn = std::function<void(std::vector<QVariant>)>;

  QConnection connect(const String& signal, QObject* receiver, const String& slot) {
    return connect(signal, [receiver, slot](std::vector<QVariant> args) {
      receiver->invoke(slot, args);
    });
  }

  QConnection connect(const String& signal, SlotFn fn) {
    static int connId = 0;
    int id = ++connId;
    _connections[signal].push_back({id, fn, true});
    QConnection c; c.id=id; c.connected=true;
    return c;
  }

  // Static connect (mirrors Qt::connect)
  static QConnection connect(QObject* sender, const String& signal,
                              QObject* receiver, const String& slot) {
    return sender->connect(signal, receiver, slot);
  }
  static QConnection connect(QObject* sender, const String& signal, SlotFn fn) {
    return sender->connect(signal, fn);
  }

  // Disconnect by connection handle
  bool disconnect(const QConnection& conn) {
    for (auto& [sig, conns] : _connections) {
      for (auto& c : conns) {
        if (c.id==conn.id) { c.active=false; return true; }
      }
    }
    return false;
  }

  // Disconnect all slots for a signal
  bool disconnect(const String& signal="") {
    if (signal.isEmpty()) { _connections.clear(); return true; }
    auto it=_connections.find(signal);
    if (it!=_connections.end()) { it->second.clear(); return true; }
    return false;
  }

  void disconnectAll() { _connections.clear(); }

  bool isSignalConnected(const String& signal) const {
    auto it=_connections.find(signal);
    if (it==_connections.end()) return false;
    for (auto& c:it->second) if (c.active) return true;
    return false;
  }

  // Emit a signal
  void emit_signal(const String& signal, std::vector<QVariant> args={}) {
    auto it=_connections.find(signal);
    if (it==_connections.end()) return;
    // Queue or direct depending on connection type
    for (auto& c : it->second) {
      if (c.active) c.fn(args);
    }
  }

  // ── Slots — invoke by name ─────────────────────────────────────────────────
  void registerSlot(const String& name, SlotFn fn) {
    _slots[name] = fn;
  }

  bool invoke(const String& slotName, std::vector<QVariant> args={}) {
    auto it=_slots.find(slotName);
    if (it!=_slots.end()) { it->second(args); return true; }
    return false;
  }

  // ── Timers (moveToThread-safe on ESP32 via FreeRTOS) ──────────────────────
  int startTimer(int intervalMs) {
    _timers.push_back({++_timerIdCounter, intervalMs, millis(), true});
    return _timerIdCounter;
  }

  void killTimer(int id) {
    for (auto& t:_timers) if (t.id==id) t.active=false;
  }

  // Call this from loop() or App event loop
  virtual void timerTick();

  // ── Event handling (mirrors QObject::event) ───────────────────────────────
  virtual bool event(QEvent* e);
  virtual void timerEvent(QTimerEvent* e) { (void)e; }
  virtual void childEvent(QChildEvent* e) { (void)e; }
  virtual void customEvent(QEvent* e)     { (void)e; }

  bool installEventFilter(QObject* filterObj) {
    _eventFilters.push_back(filterObj);
    return true;
  }
  bool removeEventFilter(QObject* filterObj) {
    _eventFilters.erase(
      std::remove(_eventFilters.begin(),_eventFilters.end(),filterObj),
      _eventFilters.end());
    return true;
  }

  // ── deleteLater ───────────────────────────────────────────────────────────
  void deleteLater() { _pendingDelete=true; }
  bool isPendingDelete() const { return _pendingDelete; }

  // ── Introspection ─────────────────────────────────────────────────────────
  virtual const char* metaClassName() const { return "QObject"; }
  int objectId() const { return _objectId; }
  bool inherits(const char* className) const {
    return strcmp(metaClassName(), className)==0;
  }

  // ── blockSignals ──────────────────────────────────────────────────────────
  bool blockSignals(bool block) { bool old=_blocked; _blocked=block; return old; }
  bool signalsBlocked() const { return _blocked; }

  // ── moveToThread (stub — ESP32 uses FreeRTOS tasks) ──────────────────────
  void moveToThread(int coreId) { _coreId=coreId; }
  int thread() const { return _coreId; }

  // ── tr() — translation stub (always returns source) ──────────────────────
  static String tr(const char* src) { return String(src); }
  static String tr(const String& src) { return src; }

  // ── dumpObjectInfo / dumpObjectTree ──────────────────────────────────────
  void dumpObjectInfo(Print& out=Serial) const {
    out.print("QObject("); out.print(metaClassName()); out.print(") \"");
    out.print(_objectName); out.print("\" id="); out.println(_objectId);
    out.print("  Signals connected: "); out.println(_connections.size());
    out.print("  Properties: "); out.println(_properties.size());
    out.print("  Dynamic props: "); out.println(_dynamicProps.size());
    out.print("  Children: "); out.println(_children.size());
  }

  void dumpObjectTree(Print& out=Serial, int depth=0) const {
    for (int i=0;i<depth;i++) out.print("  ");
    out.print(metaClassName()); out.print("(\""); out.print(_objectName); out.println("\")");
    for (auto* c:_children) c->dumpObjectTree(out,depth+1);
  }

protected:
  // ── Common signals (emit via emit_signal) ─────────────────────────────────
  void emit_destroyed()                   { emit_signal("destroyed"); }
  void emit_objectNameChanged(String n)   { emit_signal("objectNameChanged",{QVariant(n)}); }
  void emit_propertyChanged(String name)  { emit_signal("propertyChanged",{QVariant(name)}); }

  // ── Child management ──────────────────────────────────────────────────────
  void _addChild(QObject* c) {
    if (_children.size()<NOORQT_MAX_CHILDREN) _children.push_back(c);
  }
  void _removeChild(QObject* c) {
    _children.erase(std::remove(_children.begin(),_children.end(),c),_children.end());
  }

  // ── Members ───────────────────────────────────────────────────────────────
  String _objectName;
  QObject* _parent = nullptr;
  std::vector<QObject*> _children;
  bool _blocked = false;
  bool _pendingDelete = false;
  int  _objectId = 0;
  int  _coreId   = 0;

  struct ConnectionEntry { int id; SlotFn fn; bool active; };
  std::map<String, std::vector<ConnectionEntry>> _connections;
  std::map<String, SlotFn> _slots;
  std::map<String, QMetaProperty> _properties;
  std::map<String, QVariant> _dynamicProps;
  std::vector<QObject*> _eventFilters;

  struct TimerEntry { int id; int intervalMs; unsigned long lastFire; bool active; };
  std::vector<TimerEntry> _timers;
  int _timerIdCounter = 0;
};

// ── QEvent ────────────────────────────────────────────────────────────────────
class QEvent {
public:
  enum Type {
    None=0, Timer=1, MouseButtonPress=2, MouseButtonRelease=3,
    MouseMove=5, TouchBegin=194, TouchUpdate=195, TouchEnd=196,
    KeyPress=6, KeyRelease=7, FocusIn=8, FocusOut=9,
    Enter=10, Leave=11, Paint=12, Move=13, Resize=14,
    Show=17, Hide=18, Close=19, Destroy=20,
    ChildAdded=68, ChildRemoved=71, ChildPolished=69,
    ApplicationActivated=121, ApplicationDeactivated=122,
    User=1000, MaxUser=65535
  };

  explicit QEvent(Type t) : _type(t), _accepted(true) {}
  virtual ~QEvent() {}

  Type type()      const { return _type; }
  bool isAccepted()const { return _accepted; }
  void accept()          { _accepted=true; }
  void ignore()          { _accepted=false; }
  void setAccepted(bool a){ _accepted=a; }

  bool spontaneous() const { return _spontaneous; }

protected:
  Type _type;
  bool _accepted;
  bool _spontaneous = false;
};

// ── QTimerEvent ───────────────────────────────────────────────────────────────
class QTimerEvent : public QEvent {
public:
  explicit QTimerEvent(int id) : QEvent(QEvent::Timer), _timerId(id) {}
  int timerId() const { return _timerId; }
private:
  int _timerId;
};

// ── QChildEvent ───────────────────────────────────────────────────────────────
class QChildEvent : public QEvent {
public:
  QChildEvent(QEvent::Type t, QObject* child) : QEvent(t), _child(child) {}
  QObject* child() const { return _child; }
  bool added()    const { return _type==QEvent::ChildAdded; }
  bool removed()  const { return _type==QEvent::ChildRemoved; }
  bool polished() const { return _type==QEvent::ChildPolished; }
private:
  QObject* _child;
};

// ── QObject::event default impl ───────────────────────────────────────────────
inline bool QObject::event(QEvent* e) {
  // Run through event filters first
  for (auto* f:_eventFilters) {
    if (f->invoke("eventFilter",{QVariant((int)(intptr_t)e)})) return true;
  }
  switch (e->type()) {
    case QEvent::Timer:     timerEvent(static_cast<QTimerEvent*>(e)); return true;
    case QEvent::ChildAdded:
    case QEvent::ChildRemoved: childEvent(static_cast<QChildEvent*>(e)); return true;
    default: customEvent(e); return false;
  }
}

// ── QObject::timerTick — defined here so QTimerEvent is complete ──────────────
inline void QObject::timerTick() {
  unsigned long now=millis();
  for (auto& t:_timers) {
    if (t.active && now-t.lastFire >= (unsigned long)t.intervalMs) {
      t.lastFire=now;
      QTimerEvent ev(t.id);
      timerEvent(&ev);
    }
  }
  for (auto* c:_children) c->timerTick();
}

// eventFilter — can be overridden
inline bool eventFilter_default(QObject* /*watched*/, QEvent* /*event*/) { return false; }

// ── QScopedPointer (mirrors Qt6) ─────────────────────────────────────────────
template<typename T>
class QScopedPointer {
public:
  explicit QScopedPointer(T* p=nullptr) : _p(p) {}
  ~QScopedPointer() { delete _p; }
  T* get()    const { return _p; }
  T* data()   const { return _p; }
  T* operator->() const { return _p; }
  T& operator*()  const { return *_p; }
  explicit operator bool() const { return _p!=nullptr; }
  void reset(T* p=nullptr) { delete _p; _p=p; }
  T*   take()  { T* t=_p; _p=nullptr; return t; }
  bool isNull()const { return _p==nullptr; }
private:
  T* _p;
  QScopedPointer(const QScopedPointer&)=delete;
  QScopedPointer& operator=(const QScopedPointer&)=delete;
};

// ── QSharedPointer (simplified) ───────────────────────────────────────────────
template<typename T>
using QSharedPointer = std::shared_ptr<T>;

template<typename T>
using QWeakPointer = std::weak_ptr<T>;

// ── QList / QVector / QMap / QHash (thin wrappers over std) ──────────────────
template<typename T>
class QList : public std::vector<T> {
public:
  using std::vector<T>::vector;
  void append(const T& v)  { this->push_back(v); }
  void prepend(const T& v) { this->insert(this->begin(),v); }
  void removeAll(const T& v) {
    this->erase(std::remove(this->begin(),this->end(),v),this->end());
  }
  bool contains(const T& v) const {
    return std::find(this->begin(),this->end(),v)!=this->end();
  }
  int  count()   const { return (int)this->size(); }
  int  length()  const { return (int)this->size(); }
  bool isEmpty() const { return this->empty(); }
  T    first()   const { return this->front(); }
  T    last()    const { return this->back(); }
  T    takeFirst(){ T v=this->front(); this->erase(this->begin()); return v; }
  T    takeLast() { T v=this->back();  this->pop_back(); return v; }
  T    takeAt(int i){ T v=(*this)[i]; this->erase(this->begin()+i); return v; }
  void insert(int i,const T& v){ this->std::vector<T>::insert(this->begin()+i,v); }
  void removeAt(int i){ this->erase(this->begin()+i); }
  void move(int from,int to){ T v=takeAt(from); insert(to,v); }
  QList<T> mid(int pos,int len=-1) const {
    QList<T> r;
    int end=len<0?(int)this->size():min((int)this->size(),pos+len);
    for (int i=pos;i<end;i++) r.push_back((*this)[i]);
    return r;
  }
  void swap(int i,int j){ std::swap((*this)[i],(*this)[j]); }
  QList<T>& operator<<(const T& v){ append(v); return *this; }
  QList<T>  operator+(const QList<T>& o) const {
    QList<T> r=*this; for (auto& v:o) r.push_back(v); return r;
  }
};

template<typename T> using QVector = QList<T>;
template<typename K,typename V> using QMap    = std::map<K,V>;
template<typename K,typename V> using QHash   = std::map<K,V>; // FreeRTOS safe
template<typename T>            using QSet    = std::vector<T>; // ordered
template<typename T1,typename T2>
struct QPair { T1 first; T2 second;
  QPair(){}
  QPair(T1 a,T2 b):first(a),second(b){}
};
template<typename T1,typename T2>
QPair<T1,T2> qMakePair(T1 a,T2 b){ return {a,b}; }

// ── QString alias (Arduino String is already QObject-compatible) ──────────────
using QString = String;

// ── QByteArray ────────────────────────────────────────────────────────────────
class QByteArray {
public:
  QByteArray() {}
  QByteArray(const char* d, int size=-1) {
    if (size<0) _data=String(d);
    else { for (int i=0;i<size;i++) _data+=(char)d[i]; }
  }
  QByteArray(int size, char fill) { for (int i=0;i<size;i++) _data+=fill; }

  int         size()      const { return _data.length(); }
  int         length()    const { return _data.length(); }
  bool        isEmpty()   const { return _data.isEmpty(); }
  bool        isNull()    const { return _data.isEmpty(); }
  const char* data()      const { return _data.c_str(); }
  char        at(int i)   const { return _data[i]; }
  void        append(char c)    { _data+=c; }
  void        append(const QByteArray& b) { _data+=b._data; }
  void        clear()           { _data=""; }
  String      toHex()     const {
    String r; for (char c:_data) { r+=String((uint8_t)c,HEX); } return r;
  }
  String      toBase64()  const { return _data; } // stub
  QString     toStdString()const { return _data; }
  static QByteArray fromHex(const String& hex) {
    QByteArray r;
    for (int i=0;i+1<(int)hex.length();i+=2) {
      r.append((char)strtol(hex.substring(i,i+2).c_str(),nullptr,16));
    }
    return r;
  }
  QByteArray& operator+=(char c){ _data+=c; return *this; }
  QByteArray& operator+=(const QByteArray& b){ _data+=b._data; return *this; }
  bool operator==(const QByteArray& o) const { return _data==o._data; }
private:
  String _data;
};

// ── QStringList ───────────────────────────────────────────════════════════════
class QStringList : public QList<QString> {
public:
  using QList<QString>::QList;
  QString join(const QString& sep) const {
    QString r; for (int i=0;i<(int)this->size();i++) { if(i>0)r+=sep; r+=(*this)[i]; }
    return r;
  }
  QStringList filter(const QString& pattern) const {
    QStringList r;
    for (auto& s:*this) if (s.indexOf(pattern)>=0) r.append(s);
    return r;
  }
  QStringList& operator<<(const QString& s){ append(s); return *this; }
  static QStringList split(const QString& str, const QString& sep) {
    QStringList r; int start=0, idx;
    while ((idx=str.indexOf(sep,start))>=0) {
      r.append(str.substring(start,idx)); start=idx+sep.length();
    }
    r.append(str.substring(start)); return r;
  }
};

// ── Qt namespace (enums mirroring Qt::) ──────────────────────────────────────
namespace Qt {
  enum AlignmentFlag {
    AlignLeft=0x0001, AlignRight=0x0002, AlignHCenter=0x0004,
    AlignTop=0x0020,  AlignBottom=0x0040, AlignVCenter=0x0080,
    AlignCenter=AlignHCenter|AlignVCenter
  };
  enum Orientation    { Horizontal=0x1, Vertical=0x2 };
  enum ScrollBarPolicy{ ScrollBarAlwaysOn, ScrollBarAlwaysOff, ScrollBarAsNeeded };
  enum FocusPolicy    { NoFocus=0, TabFocus=1, ClickFocus=2, StrongFocus=11, WheelFocus=15 };
  enum ConnectionType { AutoConnection, DirectConnection, QueuedConnection,
                        BlockingQueuedConnection, UniqueConnection=0x80 };
  enum CheckState     { Unchecked=0, PartiallyChecked=1, Checked=2 };
  enum SortOrder      { AscendingOrder=0, DescendingOrder=1 };
  enum MatchFlag      { MatchExactly=0, MatchContains=1, MatchStartsWith=2, MatchEndsWith=3 };
  enum GlobalColor {
    white=3, black=2, red=7, darkRed=13, green=8, darkGreen=14,
    blue=9, darkBlue=15, cyan=10, darkCyan=16, magenta=11, darkMagenta=17,
    yellow=12, darkYellow=18, gray=5, darkGray=4, lightGray=6, transparent=19
  };
  enum Key {
    Key_Return=0x01000005, Key_Enter=0x01000005, Key_Escape=0x01000000,
    Key_Tab=0x01000001, Key_Backspace=0x01000003, Key_Delete=0x01000007,
    Key_Left=0x01000012, Key_Right=0x01000014, Key_Up=0x01000013, Key_Down=0x01000015,
    Key_Home=0x01000010, Key_End=0x01000011, Key_Space=0x20,
    Key_0=0x30,Key_1=0x31,Key_2=0x32,Key_3=0x33,Key_4=0x34,
    Key_5=0x35,Key_6=0x36,Key_7=0x37,Key_8=0x38,Key_9=0x39,
    Key_A=0x41,Key_B=0x42,Key_C=0x43,Key_D=0x44,Key_E=0x45,
    Key_F=0x46,Key_G=0x47,Key_H=0x48,Key_I=0x49,Key_J=0x4A,
    Key_K=0x4B,Key_L=0x4C,Key_M=0x4D,Key_N=0x4E,Key_O=0x4F,
    Key_P=0x50,Key_Q=0x51,Key_R=0x52,Key_S=0x53,Key_T=0x54,
    Key_U=0x55,Key_V=0x56,Key_W=0x57,Key_X=0x58,Key_Y=0x59,Key_Z=0x5A
  };
  enum AspectRatioMode { IgnoreAspectRatio=0, KeepAspectRatio=1, KeepAspectRatioByExpanding=2 };
  enum WindowModality  { NonModal=0, WindowModal=1, ApplicationModal=2 };
  enum FocusReason     { MouseFocusReason=0, TabFocusReason=1, BacktabFocusReason=2,
                         ActiveWindowFocusReason=3, PopupFocusReason=4, OtherFocusReason=7 };
  enum CursorShape     { ArrowCursor=0, CrossCursor=2, WaitCursor=3, IBeamCursor=1,
                         BlankCursor=10, SizeVerCursor=11, SizeHorCursor=12 };
  enum MouseButton     { NoButton=0, LeftButton=0x1, RightButton=0x2, MiddleButton=0x4 };
  using MouseButtons   = int;
  using Orientations   = int;
  using Alignment      = int;
  using WindowFlags    = int;
  using ItemFlags      = int;
  enum WindowFlag  { Widget=0x00000000, Window=0x00000001, Dialog=0x00000002,
                     Popup=0x00000008, ToolTip=0x00000010, Drawer=0x00000020 };
  enum WindowState { WindowNoState=0, WindowMinimized=1, WindowMaximized=2, WindowFullScreen=4 };
  enum ItemFlag    { ItemIsEnabled=1, ItemIsSelectable=2, ItemIsEditable=4,
                     ItemIsCheckable=16, ItemIsUserCheckable=256 };
  enum TextFormat  { PlainText=0, RichText=1, AutoText=2 };
  enum WhatsThis   {};
  const int  Ignored = 0;
  const bool Checked2 = true;
  // Corners
  enum Corner { TopLeftCorner=0, TopRightCorner=1, BottomLeftCorner=2, BottomRightCorner=3 };
  // Item data roles
  enum ItemDataRole {
    DisplayRole=0, DecorationRole=1, EditRole=2, ToolTipRole=3,
    StatusTipRole=4, WhatsThisRole=5, FontRole=6, TextAlignmentRole=7,
    CheckStateRole=10, SizeHintRole=13, UserRole=0x0100
  };
}

// ── Signal<T...> — declarative signal member (mirrors Qt6 Q_SIGNAL syntax) ───
// Usage inside a QObject subclass:
//   Signal<void>         finished   {"finished", this};
//   Signal<int, String>  dataReady  {"dataReady", this};
// Calling finished.emit() fires QObject::emit_signal("finished",{}).
// The POSIX header <signal.h> defines a typedef named `Signal` on some
// toolchains, so we guard against that collision with a macro undef here.
#ifdef Signal
#  undef Signal
#endif

template<typename... Args>
struct Signal {
  String   _name;
  QObject* _owner = nullptr;

  // Two-arg ctor: name + owning QObject (normal usage inside a class body)
  Signal(const char* name, QObject* owner) : _name(name), _owner(owner) {}
  // One-arg ctor: name only (owner set later via bind())
  explicit Signal(const char* name) : _name(name), _owner(nullptr) {}

  void bind(QObject* owner) { _owner = owner; }

  // emit with no args
  void emit() {
    if (_owner) _owner->emit_signal(_name, {});
  }
  // emit with one arg
  template<typename A>
  void emit(A a) {
    if (_owner) { std::vector<QVariant> v; v.push_back(QVariant(a)); _owner->emit_signal(_name, v); }
  }
  // emit with two args — args not QVariant-constructible (e.g. QModelIndex) are silently dropped
  template<typename A, typename B>
  void emit(A /*a*/, B /*b*/) {
    if (_owner) _owner->emit_signal(_name, {});
  }
  // emit with three args
  template<typename A, typename B, typename C>
  void emit(A /*a*/, B /*b*/, C /*c*/) {
    if (_owner) _owner->emit_signal(_name, {});
  }

  // connect a lambda/slot — must accept std::vector<QVariant>
  void connect(std::function<void(std::vector<QVariant>)> fn) {
    if (_owner) _owner->connect(_name, fn);
  }
  // convenience: connect a no-arg lambda
  void connect(std::function<void()> fn) {
    if (_owner) _owner->connect(_name, [fn](std::vector<QVariant>){ fn(); });
  }

  String name() const { return _name; }
};

// ── QFlags (mirrors Qt6 QFlags<T>) ───────────────────────────────────────────
template<typename Enum>
class QFlags {
public:
  int _val=0;
  QFlags() {}
  QFlags(Enum e) : _val((int)e) {}
  QFlags operator|(QFlags o) const { QFlags r; r._val=_val|o._val; return r; }
  QFlags operator&(QFlags o) const { QFlags r; r._val=_val&o._val; return r; }
  QFlags& operator|=(QFlags o){ _val|=o._val; return *this; }
  bool testFlag(Enum e) const { return (_val&(int)e)==(int)e; }
  explicit operator int() const { return _val; }
};

using Alignment = QFlags<Qt::AlignmentFlag>;

} // namespace NoorQt
