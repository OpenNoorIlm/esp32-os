// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorQt — QThread.h                                                      ║
// ║  Threading, mutexes, semaphores — mirrors Qt6 QtCore thread API.         ║
// ║  Backed by FreeRTOS tasks on ESP32.                                       ║
// ║  MIT License — NoorRobot / OpenNoorIlm                                  ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once
#include "QObject.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>
#include <atomic>

namespace NoorQt {

// ── QMutex ────────────────────────────────────────────────────────────────────
class QMutex {
public:
  enum RecursionMode { NonRecursive, Recursive };

  explicit QMutex(RecursionMode mode=NonRecursive) {
    _sem = mode==Recursive ? xSemaphoreCreateRecursiveMutex()
                           : xSemaphoreCreateMutex();
  }
  ~QMutex() { if(_sem) vSemaphoreDelete(_sem); }

  void lock()    {
    if(xPortInIsrContext()) return;
    xSemaphoreTake(_sem, portMAX_DELAY);
  }
  bool tryLock(int ms=0) {
    return xSemaphoreTake(_sem, pdMS_TO_TICKS(ms))==pdTRUE;
  }
  void unlock()  { xSemaphoreGive(_sem); }
  bool isLocked()const { return uxSemaphoreGetCount(_sem)==0; }

private:
  SemaphoreHandle_t _sem = nullptr;
  QMutex(const QMutex&) = delete;
  QMutex& operator=(const QMutex&) = delete;
};

// ── QMutexLocker (RAII) ───────────────────────────────────────────────────────
class QMutexLocker {
public:
  explicit QMutexLocker(QMutex* m) : _m(m) { if(_m) _m->lock(); }
  ~QMutexLocker()                          { unlock(); }
  void unlock()   { if(_m&&_locked) { _m->unlock(); _locked=false; } }
  void relock()   { if(_m&&!_locked){ _m->lock();   _locked=true;  } }
  QMutex* mutex() const { return _m; }

private:
  QMutex* _m;
  bool    _locked = true;
  QMutexLocker(const QMutexLocker&)=delete;
  QMutexLocker& operator=(const QMutexLocker&)=delete;
};

// ── QReadWriteLock ────────────────────────────────────────────────────────────
class QReadWriteLock {
public:
  QReadWriteLock()  { _sem=xSemaphoreCreateMutex(); }
  ~QReadWriteLock() { vSemaphoreDelete(_sem); }
  void lockForRead()          { xSemaphoreTake(_sem,portMAX_DELAY); }
  void lockForWrite()         { xSemaphoreTake(_sem,portMAX_DELAY); }
  bool tryLockForRead(int ms) { return xSemaphoreTake(_sem,pdMS_TO_TICKS(ms))==pdTRUE; }
  bool tryLockForWrite(int ms){ return xSemaphoreTake(_sem,pdMS_TO_TICKS(ms))==pdTRUE; }
  void unlock()               { xSemaphoreGive(_sem); }

private:
  SemaphoreHandle_t _sem=nullptr;
};

// ── QSemaphore ────────────────────────────────────────────────────────────────
class QSemaphore {
public:
  explicit QSemaphore(int n=0) {
    _sem = xSemaphoreCreateCounting(n+1024, n);
  }
  ~QSemaphore() { vSemaphoreDelete(_sem); }

  void acquire(int n=1) {
    for(int i=0;i<n;i++) xSemaphoreTake(_sem,portMAX_DELAY);
  }
  bool tryAcquire(int n=1, int ms=0) {
    for(int i=0;i<n;i++)
      if(xSemaphoreTake(_sem,pdMS_TO_TICKS(ms))!=pdTRUE) return false;
    return true;
  }
  void release(int n=1) {
    for(int i=0;i<n;i++) xSemaphoreGive(_sem);
  }
  int available() const { return (int)uxSemaphoreGetCount(_sem); }

private:
  SemaphoreHandle_t _sem=nullptr;
};

// ── QWaitCondition ────────────────────────────────────────────────────────────
class QWaitCondition {
public:
  QWaitCondition()  { _eg=xEventGroupCreate(); }
  ~QWaitCondition() { vEventGroupDelete(_eg); }

  bool wait(QMutex* m, unsigned long ms=ULONG_MAX) {
    m->unlock();
    bool r = xEventGroupWaitBits(_eg,1,pdTRUE,pdFALSE,
                                 ms==ULONG_MAX?portMAX_DELAY:pdMS_TO_TICKS(ms)) & 1;
    m->lock();
    return r;
  }
  void wakeOne() { xEventGroupSetBits(_eg,1); }
  void wakeAll() { xEventGroupSetBits(_eg,1); }

private:
  EventGroupHandle_t _eg=nullptr;
};

// ── QThread ───────────────────────────────────────────────────────────────────
class QThread : public QObject {
public:
  enum Priority {
    IdlePriority=0, LowestPriority=1, LowPriority=2, NormalPriority=3,
    HighPriority=4, HighestPriority=5, TimeCriticalPriority=6,
    InheritPriority=7
  };

  Signal<void> started  {"started"};
  Signal<void> finished {"finished"};

  explicit QThread(QObject* parent=nullptr) : QObject(parent) {}
  virtual ~QThread() { quit(); wait(); }

  // ── Main entry point — override in subclass ───────────────────────────────
  virtual void run() {}

  // ── Thread control ────────────────────────────────────────────────────────
  void start(Priority p=NormalPriority) {
    if(_running) return;
    _running=true;
    UBaseType_t prio=_mapPriority(p);
    uint32_t stack=_stackSize>0?_stackSize:4096;
    xTaskCreate(_threadFunc, _objectName.c_str(), stack, this, prio, &_handle);
  }

  void quit()  { _shouldStop=true; }
  void exit(int=0) { quit(); }

  bool wait(unsigned long ms=ULONG_MAX) {
    unsigned long start=millis();
    while(_running) {
      if(ms!=ULONG_MAX && millis()-start>ms) return false;
      delay(10);
    }
    return true;
  }

  bool isRunning()  const { return _running; }
  bool isFinished() const { return !_running; }

  void setPriority(Priority p) {
    if(_handle) vTaskPrioritySet(_handle,_mapPriority(p));
  }
  Priority priority() const { return _priority; }

  void setStackSize(uint32_t s) { _stackSize=s; }

  // ── Static helpers (mirrors Qt6) ──────────────────────────────────────────
  static void msleep(unsigned long ms) { delay(ms); }
  static void sleep(unsigned long s)   { delay(s*1000); }
  static void usleep(unsigned long us) { delayMicroseconds(us); }
  static void yieldCurrentThread()     { taskYIELD(); }
  static QThread* currentThread()      { return nullptr; } // no per-task mapping
  static int idealThreadCount()        { return 2; }       // ESP32 has 2 cores

  // ── Task can check this to know when to stop ──────────────────────────────
  bool isInterruptionRequested() const { return _shouldStop; }
  void requestInterruption()     { _shouldStop=true; }

protected:
  TaskHandle_t        _handle    = nullptr;
  bool                _running   = false;
  std::atomic<bool>   _shouldStop{false};
  Priority            _priority  = NormalPriority;
  uint32_t            _stackSize = 0;

private:
  static void _threadFunc(void* arg) {
    QThread* t = static_cast<QThread*>(arg);
    t->started.emit();
    t->run();
    t->_running=false;
    t->finished.emit();
    t->_handle=nullptr;
    vTaskDelete(nullptr);
  }

  static UBaseType_t _mapPriority(Priority p) {
    // FreeRTOS priorities: higher = more urgent (opposite of POSIX nice)
    switch(p) {
      case IdlePriority:         return 0;
      case LowestPriority:       return 1;
      case LowPriority:          return 2;
      case NormalPriority:       return 3;
      case HighPriority:         return 4;
      case HighestPriority:      return 5;
      case TimeCriticalPriority: return configMAX_PRIORITIES-1;
      default:                   return 3;
    }
  }
};

// ── QRunnable ────────────────────────────────────────────────────────────────
class QRunnable {
public:
  virtual ~QRunnable() {}
  virtual void run() = 0;
  bool autoDelete() const { return _autoDelete; }
  void setAutoDelete(bool d) { _autoDelete=d; }
private:
  bool _autoDelete=true;
};

// ── QThreadPool ───────────────────────────────────────────────────────────────
class QThreadPool : public QObject {
public:
  static QThreadPool* globalInstance() {
    static QThreadPool pool;
    return &pool;
  }

  void start(QRunnable* r, int /*priority*/=0) {
    // Spin up a one-shot FreeRTOS task per runnable
    struct Ctx { QRunnable* r; };
    auto* ctx = new Ctx{r};
    xTaskCreate([](void* arg){
      auto* c=static_cast<Ctx*>(arg);
      c->r->run();
      if(c->r->autoDelete()) delete c->r;
      delete c;
      vTaskDelete(nullptr);
    }, "pool", 4096, ctx, 3, nullptr);
  }

  bool tryStart(QRunnable* r) { start(r); return true; }
  void waitForDone(int=INT_MAX) {
    // No pool tracking — tasks self-delete; just yield
    delay(10);
  }

  int maxThreadCount()       const { return _maxThreads; }
  void setMaxThreadCount(int n)    { _maxThreads=n; }
  int activeThreadCount()    const { return 0; } // not tracked
  int expiryTimeout()        const { return 30000; }
  void setExpiryTimeout(int)       {}
  void reserveThread()             {}
  void releaseThread()             {}
  void clear()                     {}

private:
  int _maxThreads=4;
};

// ── QFuture (minimal — Qt Concurrent not available on ESP32) ─────────────────
template<typename T>
class QFuture {
public:
  QFuture() {}
  explicit QFuture(T val) : _val(val), _done(true) {}
  bool    isFinished() const { return _done; }
  bool    isRunning()  const { return !_done; }
  T       result()     const { return _val; }
  void    waitForFinished(){ while(!_done) delay(10); }
  void    _setResult(T v)  { _val=v; _done=true; }
private:
  T    _val{};
  bool _done=false;
};

// ── QtConcurrent::run shim ────────────────────────────────────────────────────
namespace QtConcurrent {
  template<typename F>
  auto run(F&& fn) -> QFuture<decltype(fn())> {
    using R=decltype(fn());
    auto* fut=new QFuture<R>();
    struct Ctx { F fn; QFuture<R>* fut; };
    auto* ctx=new Ctx{std::forward<F>(fn),fut};
    xTaskCreate([](void* arg){
      auto* c=static_cast<Ctx*>(arg);
      c->fut->_setResult(c->fn());
      delete c;
      vTaskDelete(nullptr);
    },"concurrent",4096,ctx,3,nullptr);
    return *fut;
  }
}

} // namespace NoorQt

using NoorQt::QMutex;
using NoorQt::QMutexLocker;
using NoorQt::QReadWriteLock;
using NoorQt::QSemaphore;
using NoorQt::QWaitCondition;
using NoorQt::QThread;
using NoorQt::QRunnable;
using NoorQt::QThreadPool;
using NoorQt::QFuture;
namespace QtConcurrent = NoorQt::QtConcurrent;
