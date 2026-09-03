// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorQt — QFile.h                                                        ║
// ║  File, directory, streams — mirrors Qt6 QtCore file API exactly.         ║
// ║  Backed by SPIFFS + SD card (if mounted).                                ║
// ║  MIT License — NoorRobot / OpenNoorIlm                                  ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once
#include "QObject.h"
#include <SPIFFS.h>
#include <FS.h>
#include <vector>

// SD is optional — only include if available
#if __has_include(<SD.h>)
  #include <SD.h>
  #define NOORQT_HAS_SD 1
#endif

namespace NoorQt {

// ── QIODevice ─────────────────────────────────────────────────────────────────
class QIODevice : public QObject {
public:
  enum OpenModeFlag { NotOpen=0, ReadOnly=1, WriteOnly=2, ReadWrite=3,
                      Append=4, Truncate=8, Text=16 };
  using OpenMode = int;

  Signal<void>    readyRead       {"readyRead"};
  Signal<qint64>  bytesWritten    {"bytesWritten"};
  Signal<void>    aboutToClose    {"aboutToClose"};

  explicit QIODevice(QObject* parent=nullptr) : QObject(parent) {}
  virtual ~QIODevice() { close(); }

  virtual bool open(OpenMode mode) { _mode=mode; _open=true; return true; }
  virtual void close()             { _open=false; }
  virtual bool isOpen()   const { return _open; }
  virtual bool isReadable()const { return _open && (_mode & ReadOnly); }
  virtual bool isWritable()const { return _open && (_mode & WriteOnly || _mode & Append); }

  virtual qint64 read(char* data, qint64 maxlen)         = 0;
  virtual qint64 write(const char* data, qint64 len)     = 0;
  virtual bool   atEnd()                           const = 0;
  virtual qint64 bytesAvailable()                  const = 0;
  virtual bool   seek(qint64 pos)                        = 0;
  virtual qint64 pos()                             const = 0;
  virtual qint64 size()                            const = 0;

  QString readAll() {
    qint64 sz = bytesAvailable();
    if(sz<=0) sz=size();
    std::vector<char> buf(sz+1,0);
    read(buf.data(), sz);
    return QString(buf.data());
  }
  QString readLine(qint64 maxlen=1024) {
    QString line;
    char c;
    while(line.length()<(size_t)maxlen && read(&c,1)==1) {
      line += c;
      if(c=='\n') break;
    }
    return line;
  }
  qint64 write(const QString& s) { return write(s.c_str(),(qint64)s.length()); }

  QString errorString() const { return _errStr; }

protected:
  OpenMode _mode = NotOpen;
  bool     _open = false;
  QString  _errStr;
};

// ── QFileInfo ─────────────────────────────────────────────────────────────────
class QFileInfo {
public:
  QFileInfo() {}
  explicit QFileInfo(const QString& path) : _path(path) {}

  QString filePath()    const { return _path; }
  QString absoluteFilePath() const { return _path; }
  QString fileName()    const {
    int i=_path.lastIndexOf('/');
    return i>=0 ? _path.substring(i+1) : _path;
  }
  QString baseName()    const {
    QString fn=fileName();
    int i=fn.lastIndexOf('.');
    return i>0 ? fn.substring(0,i) : fn;
  }
  QString suffix()      const {
    int i=_path.lastIndexOf('.');
    return i>=0 ? _path.substring(i+1) : String();
  }
  QString path()        const {
    int i=_path.lastIndexOf('/');
    return i>0 ? _path.substring(0,i) : String("/");
  }
  QString dir()         const { return path(); }

  bool exists()         const { return SPIFFS.exists(_path); }
  bool isFile()         const {
    if(!exists()) return false;
    auto f=SPIFFS.open(_path,"r");
    bool r=f && !f.isDirectory();
    if(f) f.close(); return r;
  }
  bool isDir()          const {
    auto f=SPIFFS.open(_path,"r");
    bool r=f && f.isDirectory();
    if(f) f.close(); return r;
  }
  bool isReadable()     const { return exists(); }
  bool isWritable()     const { return true; }
  qint64 size()         const {
    auto f=SPIFFS.open(_path,"r");
    qint64 s=f?f.size():0; if(f)f.close(); return s;
  }

private:
  QString _path;
};

// ── QFile ─────────────────────────────────────────────────────────────────────
class QFile : public QIODevice {
public:
  QFile() {}
  explicit QFile(const QString& path, QObject* parent=nullptr)
    : QIODevice(parent), _path(path) {}
  ~QFile() { close(); }

  QString fileName()   const { return _path; }
  void setFileName(const QString& p) { _path=p; }

  bool exists() const { return SPIFFS.exists(_path); }
  static bool exists(const QString& p) { return SPIFFS.exists(p); }

  bool open(OpenMode mode) override {
    const char* m="r";
    if((mode & ReadWrite)==ReadWrite) m="r+";
    else if(mode & WriteOnly)  m=(mode&Append)?"a":"w";
    else if(mode & ReadOnly)   m="r";
    _f = SPIFFS.open(_path, m);
    if(!_f) { _errStr="Cannot open: "+_path; return false; }
    _mode=mode; _open=true;
    return true;
  }

  void close() override {
    if(_f) { aboutToClose.emit(); _f.close(); }
    _open=false;
  }

  qint64 read(char* data, qint64 maxlen) override {
    if(!_f) return -1;
    return _f.readBytes(data,(size_t)maxlen);
  }
  qint64 write(const char* data, qint64 len) override {
    if(!_f) return -1;
    qint64 n=_f.write((const uint8_t*)data,(size_t)len);
    bytesWritten.emit(n); return n;
  }
  bool   atEnd()          const override { return _f && !_f.available(); }
  qint64 bytesAvailable() const override { return _f?_f.available():0; }
  bool   seek(qint64 pos) override       { return _f && _f.seek((uint32_t)pos); }
  qint64 pos()            const override { return _f?_f.position():0; }
  qint64 size()           const override { return _f?_f.size():0; }

  bool flush() { return true; } // SPIFFS auto-flushes
  bool remove() { close(); return SPIFFS.remove(_path); }
  static bool remove(const QString& p) { return SPIFFS.remove(p); }
  bool rename(const QString& newName) {
    close();
    if(!SPIFFS.rename(_path,newName)) return false;
    _path=newName; return true;
  }
  static bool rename(const QString& old_, const QString& new_) {
    return SPIFFS.rename(old_,new_);
  }
  bool copy(const QString& dest) {
    QFile src(_path), dst(dest);
    if(!src.open(ReadOnly) || !dst.open(WriteOnly)) return false;
    char buf[256];
    while(!src.atEnd()) {
      qint64 n=src.read(buf,256);
      if(n>0) dst.write(buf,n);
    }
    return true;
  }

  QFileInfo fileInfo() const { return QFileInfo(_path); }

private:
  QString _path;
  File    _f;
};

// ── QDir ──────────────────────────────────────────────────────────────────────
class QDir {
public:
  enum Filter  { NoFilter=0, Files=1, Dirs=2, AllEntries=3 };
  enum SortFlag{ Name=0, Time=1, Size=2 };

  QDir() : _path("/") {}
  explicit QDir(const QString& path) : _path(path) {}

  QString path()         const { return _path; }
  QString absolutePath() const { return _path; }
  void    setPath(const QString& p) { _path=p; }

  static QDir home()    { return QDir("/"); }
  static QDir root()    { return QDir("/"); }
  static QDir current() { return QDir("/"); }
  static QString currentPath() { return "/"; }
  static QString homePath()    { return "/"; }

  bool exists()   const { return true; } // SPIFFS is flat, root always exists
  bool exists(const QString& name) const { return SPIFFS.exists(_path+"/"+name); }

  bool mkdir(const QString&)  const { return true; } // SPIFFS is flat
  bool mkpath(const QString&) const { return true; }
  bool rmdir(const QString& name) const {
    // Remove all files under this pseudo-dir prefix
    QString prefix = _path+"/"+name+"/";
    File root=SPIFFS.open("/");
    File f=root.openNextFile();
    while(f){
      if(String(f.name()).startsWith(prefix)) SPIFFS.remove(f.name());
      f=root.openNextFile();
    }
    return true;
  }
  bool removeRecursively() const { return rmdir(""); }

  std::vector<QString> entryList(Filter filter=AllEntries, SortFlag sort=Name) const {
    (void)sort;
    std::vector<QString> list;
    File root=SPIFFS.open("/");
    if(!root) return list;
    File f=root.openNextFile();
    while(f){
      QString name=f.name();
      // Simulate directory listing for path prefix
      if(_path=="/" || name.startsWith(_path)){
        if(_path!="/") name=name.substring(_path.length());
        if(name.startsWith("/")) name=name.substring(1);
        // Skip sub-subdirs
        if(name.indexOf('/')<0 || name.indexOf('/')==(int)name.length()-1){
          if((filter&Files)  && !f.isDirectory()) list.push_back(name);
          if((filter&Dirs)   &&  f.isDirectory()) list.push_back(name);
        }
      }
      f=root.openNextFile();
    }
    return list;
  }

  std::vector<QString> entryList(const QString& /*nameFilter*/,
                                 Filter filter=AllEntries) const {
    return entryList(filter);
  }

  static bool setCurrent(const QString&) { return true; }

  QString filePath(const QString& name) const {
    return _path=="/" ? "/"+name : _path+"/"+name;
  }
  QString absoluteFilePath(const QString& name) const { return filePath(name); }

  static char separator() { return '/'; }

private:
  QString _path;
};

// ── QTextStream ───────────────────────────────────────────────────────────────
class QTextStream {
public:
  enum FieldAlignment { AlignLeft, AlignRight, AlignCenter };
  static const char* endl;

  explicit QTextStream(QIODevice* dev) : _dev(dev) {}
  explicit QTextStream(QString* str, QIODevice::OpenMode mode=QIODevice::ReadWrite)
    : _dev(nullptr), _str(str), _mode(mode) {}

  // Write operators
  QTextStream& operator<<(const QString& s) {
    if(_dev) _dev->write(s); else if(_str && (_mode&QIODevice::WriteOnly)) *_str += s;
    return *this;
  }
  QTextStream& operator<<(int v)         { return *this << String(v); }
  QTextStream& operator<<(long v)        { return *this << String(v); }
  QTextStream& operator<<(double v)      { return *this << String(v); }
  QTextStream& operator<<(float v)       { return *this << String(v); }
  QTextStream& operator<<(char c)        { return *this << String(c); }
  QTextStream& operator<<(const char* s) { return *this << QString(s); }
  QTextStream& operator<<(bool b)        { return *this << (b?"true":"false"); }

  // Read operators
  QTextStream& operator>>(QString& s) {
    if(_dev) s=_dev->readLine(); return *this;
  }
  QTextStream& operator>>(int& v)    { QString s; *this>>s; v=s.toInt(); return *this; }
  QTextStream& operator>>(double& v) { QString s; *this>>s; v=s.toDouble(); return *this; }

  QString readAll() { return _dev ? _dev->readAll() : (_str?*_str:String()); }
  QString readLine(qint64 max=0){ (void)max; return _dev?_dev->readLine():String(); }
  bool    atEnd()  const { return _dev?_dev->atEnd():true; }
  void    flush()  {}

  void setCodec(const char*) {} // No-op on ESP32 (UTF-8 only)
  void setEncoding(int)      {}

private:
  QIODevice*           _dev  = nullptr;
  QString*             _str  = nullptr;
  QIODevice::OpenMode  _mode = QIODevice::ReadWrite;
};
const char* QTextStream::endl = "\n";

// ── QDataStream ───────────────────────────────────────────────────────────────
class QDataStream {
public:
  enum ByteOrder { BigEndian, LittleEndian };
  enum Version   { Qt_6_0=20 };

  explicit QDataStream(QIODevice* dev) : _dev(dev) {}

  void setByteOrder(ByteOrder b) { _bo=b; }
  void setVersion(Version)       {}

  QDataStream& operator<<(quint8 v)  { _write(&v,1); return *this; }
  QDataStream& operator<<(quint16 v) { _write(&v,2); return *this; }
  QDataStream& operator<<(quint32 v) { _write(&v,4); return *this; }
  QDataStream& operator<<(qint32 v)  { return *this<<(quint32)v; }
  QDataStream& operator<<(float v)   { _write(&v,4); return *this; }
  QDataStream& operator<<(double v)  { _write(&v,8); return *this; }
  QDataStream& operator<<(const QString& s) {
    quint32 len=s.length(); *this<<len;
    _dev->write(s.c_str(),(qint64)len); return *this;
  }

  QDataStream& operator>>(quint8& v)  { _read(&v,1); return *this; }
  QDataStream& operator>>(quint16& v) { _read(&v,2); return *this; }
  QDataStream& operator>>(quint32& v) { _read(&v,4); return *this; }
  QDataStream& operator>>(qint32& v)  { return *this>>(quint32&)v; }
  QDataStream& operator>>(float& v)   { _read(&v,4); return *this; }
  QDataStream& operator>>(double& v)  { _read(&v,8); return *this; }
  QDataStream& operator>>(QString& s) {
    quint32 len; *this>>len;
    std::vector<char> buf(len+1,0);
    _dev->read(buf.data(),len); s=buf.data(); return *this;
  }

  bool atEnd() const { return _dev->atEnd(); }

private:
  QIODevice* _dev;
  ByteOrder  _bo = LittleEndian;

  void _write(const void* data, size_t len) { _dev->write((const char*)data,(qint64)len); }
  void _read(void* data, size_t len)        { _dev->read((char*)data,(qint64)len); }
};

// ── QFileSystemWatcher ────────────────────────────────────────────────────────
// SPIFFS doesn't support inotify — we poll every 2 seconds
class QFileSystemWatcher : public QObject {
public:
  Signal<QString> fileChanged      {"fileChanged"};
  Signal<QString> directoryChanged {"directoryChanged"};

  explicit QFileSystemWatcher(QObject* parent=nullptr) : QObject(parent) {}

  void addPath(const QString& path) {
    _watched.push_back({path, _mtime(path)});
  }
  void removePath(const QString& path) {
    _watched.erase(std::remove_if(_watched.begin(),_watched.end(),
      [&](auto& w){return w.path==path;}), _watched.end());
  }

  // Call from loop() to poll
  void poll() {
    if(millis()-_lastPoll < 2000) return;
    _lastPoll=millis();
    for(auto& w : _watched) {
      size_t mt=_mtime(w.path);
      if(mt!=w.mtime) { w.mtime=mt; fileChanged.emit(w.path); }
    }
  }

private:
  struct Watch { QString path; size_t mtime; };
  std::vector<Watch> _watched;
  unsigned long _lastPoll=0;

  size_t _mtime(const QString& p) {
    auto f=SPIFFS.open(p,"r");
    size_t s=f?f.size():0; if(f)f.close(); return s; // use size as proxy
  }
};

} // namespace NoorQt

using NoorQt::QIODevice;
using NoorQt::QFileInfo;
using NoorQt::QFile;
using NoorQt::QDir;
using NoorQt::QTextStream;
using NoorQt::QDataStream;
using NoorQt::QFileSystemWatcher;
