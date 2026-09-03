// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorQt — QAnimation.h                                                   ║
// ║  Property animations — mirrors Qt6 QtCore animation API exactly.         ║
// ║  Backed by FreeRTOS timer tasks (no Qt event loop needed).               ║
// ║  MIT License — NoorRobot / OpenNoorIlm                                  ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once
#include "QObject.h"
#include "QGeometry.h"
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <vector>
#include <functional>
#include <cmath>

namespace NoorQt {

// ── QEasingCurve ─────────────────────────────────────────────────────────────
class QEasingCurve {
public:
  enum Type {
    Linear=0, InQuad, OutQuad, InOutQuad,
    InCubic, OutCubic, InOutCubic,
    InSine, OutSine, InOutSine,
    InExpo, OutExpo, InOutExpo,
    InCirc, OutCirc, InOutCirc,
    InElastic, OutElastic, InOutElastic,
    InBounce, OutBounce, InOutBounce,
    InBack, OutBack, InOutBack,
  };

  QEasingCurve(Type t=Linear) : _type(t) {}

  float valueForProgress(float t) const {
    t=t<0?0:t>1?1:t;
    switch(_type) {
      case Linear:      return t;
      case InQuad:      return t*t;
      case OutQuad:     return t*(2-t);
      case InOutQuad:   return t<.5f ? 2*t*t : -1+(4-2*t)*t;
      case InCubic:     return t*t*t;
      case OutCubic:    { float u=t-1; return u*u*u+1; }
      case InOutCubic:  return t<.5f ? 4*t*t*t : (t-1)*(2*t-2)*(2*t-2)+1;
      case InSine:      return 1-cosf(t*M_PI/2);
      case OutSine:     return sinf(t*M_PI/2);
      case InOutSine:   return -(cosf(M_PI*t)-1)/2;
      case InExpo:      return t==0?0:powf(2,10*t-10);
      case OutExpo:     return t==1?1:1-powf(2,-10*t);
      case InOutExpo:   return t==0?0:t==1?1:t<.5f?powf(2,20*t-10)/2:(2-powf(2,-20*t+10))/2;
      case InCirc:      return 1-sqrtf(1-t*t);
      case OutCirc:     { float u=t-1; return sqrtf(1-u*u); }
      case InOutCirc:   return t<.5f?(1-sqrtf(1-4*t*t))/2:(sqrtf(1-((-2*t+2)*(-2*t+2)))+1)/2;
      case InElastic:   { if(t==0||t==1)return t; float c=2*M_PI/3; return -powf(2,10*t-10)*sinf((t*10-10.75f)*c); }
      case OutElastic:  { if(t==0||t==1)return t; float c=2*M_PI/3; return powf(2,-10*t)*sinf((t*10-.75f)*c)+1; }
      case InOutElastic:{ float c=2*M_PI/4.5f; if(t==0||t==1)return t; return t<.5f?-(powf(2,20*t-10)*sinf((20*t-11.125f)*c))/2:(powf(2,-20*t+10)*sinf((20*t-11.125f)*c))/2+1; }
      case InBounce:    return 1-_bounce(1-t);
      case OutBounce:   return _bounce(t);
      case InOutBounce: return t<.5f?(1-_bounce(1-2*t))/2:(_bounce(2*t-1)+1)/2;
      case InBack:      { float c=1.70158f; return c*t*t*t-c*t*t; }
      case OutBack:     { float c=1.70158f,u=t-1; return 1+c*u*u*u+c*u*u; }
      case InOutBack:   { float c=1.70158f*1.525f; return t<.5f?(4*t*t*((c+1)*2*t-c))/2:(((-2*t+2)*(-2*t+2))*((c+1)*(-2*t+2)+c)+2)/2; }
      default:          return t;
    }
  }

  Type type() const { return _type; }
  void setType(Type t) { _type=t; }

private:
  Type _type;

  static float _bounce(float t) {
    const float n=7.5625f, d=2.75f;
    if(t<1/d)         return n*t*t;
    if(t<2/d)         return n*(t-=1.5f/d)*t+.75f;
    if(t<2.5f/d)      return n*(t-=2.25f/d)*t+.9375f;
    return n*(t-=2.625f/d)*t+.984375f;
  }
};

// ── QAbstractAnimation ────────────────────────────────────────────────────────
class QAbstractAnimation : public QObject {
public:
  enum Direction { Forward, Backward };
  enum State     { Stopped, Paused, Running };
  enum DeletionPolicy { KeepWhenStopped, DeleteWhenStopped };

  Signal<State,State> stateChanged{"stateChanged"};
  Signal<void>        finished    {"finished"};
  Signal<int>         currentLoopChanged{"currentLoopChanged"};

  explicit QAbstractAnimation(QObject* parent=nullptr) : QObject(parent) {}
  virtual ~QAbstractAnimation() { stop(); }

  virtual int  duration()     const = 0;
  virtual void updateCurrentTime(int ms) = 0;

  int  totalDuration() const {
    return _loopCount<0 ? -1 : duration()*_loopCount;
  }
  int  currentTime()   const { return _currentTime; }
  int  currentLoop()   const { return _currentLoop; }
  int  loopCount()     const { return _loopCount; }
  void setLoopCount(int n)   { _loopCount=n; }

  Direction direction() const { return _dir; }
  void setDirection(Direction d) { _dir=d; }

  State state() const { return _state; }

  void start(DeletionPolicy p=KeepWhenStopped) {
    if(_state==Running) return;
    _policy=p; _currentTime=0; _currentLoop=0;
    _setState(Running);
    _startTimer();
  }
  void stop()  { _stopTimer(); _setState(Stopped); if(_policy==DeleteWhenStopped) deleteLater(); }
  void pause() { _stopTimer(); _setState(Paused); }
  void resume(){ _setState(Running); _startTimer(); }

protected:
  void _setState(State s) {
    State old=_state; _state=s;
    if(old!=s) stateChanged.emit(s,old);
    if(s==Stopped) finished.emit();
  }

  void _tick() {
    _currentTime+=_tickMs;
    int d=duration();
    if(_currentTime>=d) {
      if(_loopCount<0 || _currentLoop+1<_loopCount) {
        _currentLoop++;
        _currentTime=0;
        currentLoopChanged.emit(_currentLoop);
      } else {
        _currentTime=d;
        updateCurrentTime(_currentTime);
        stop(); return;
      }
    }
    float progress=(float)_currentTime/(float)(d>0?d:1);
    if(_dir==Backward) progress=1-progress;
    updateCurrentTime((int)(progress*d));
  }

  void _startTimer() {
    _timer=xTimerCreate("anim",pdMS_TO_TICKS(_tickMs),pdTRUE,this,_timerCb);
    if(_timer) xTimerStart(_timer,0);
  }
  void _stopTimer() {
    if(_timer){ xTimerStop(_timer,0); xTimerDelete(_timer,0); _timer=nullptr; }
  }

  static void _timerCb(TimerHandle_t h) {
    QAbstractAnimation* a=static_cast<QAbstractAnimation*>(pvTimerGetTimerID(h));
    a->_tick();
  }

  State    _state      = Stopped;
  Direction _dir       = Forward;
  int      _currentTime= 0;
  int      _currentLoop= 0;
  int      _loopCount  = 1;
  const int _tickMs    = 16; // ~60fps
  TimerHandle_t _timer = nullptr;
  DeletionPolicy _policy=KeepWhenStopped;
};

// ── QVariantAnimation ─────────────────────────────────────────────────────────
class QVariantAnimation : public QAbstractAnimation {
public:
  Signal<QVariant> valueChanged{"valueChanged"};

  explicit QVariantAnimation(QObject* parent=nullptr) : QAbstractAnimation(parent) {}

  void setStartValue(const QVariant& v) { _start=v; }
  void setEndValue(const QVariant& v)   { _end=v; }
  void setKeyValueAt(float step, const QVariant& v) { _keys[step]=v; }
  void setDuration(int ms)              { _duration=ms; }
  void setEasingCurve(const QEasingCurve& e){ _easing=e; }

  QVariant  startValue()    const { return _start; }
  QVariant  endValue()      const { return _end; }
  QVariant  currentValue()  const { return _current; }
  int       duration()      const override { return _duration; }
  QEasingCurve easingCurve()const { return _easing; }

  void updateCurrentTime(int ms) override {
    float t=(float)ms/(float)(_duration>0?_duration:1);
    float et=_easing.valueForProgress(t);
    // Interpolate between start and end
    if(_start.type()==QVariant::Int||_start.type()==QVariant::LongLong) {
      int s=_start.toInt(), e=_end.toInt();
      _current=QVariant((int)(s+et*(e-s)));
    } else if(_start.type()==QVariant::Double||_start.type()==QVariant::Float) {
      double s=_start.toDouble(), e=_end.toDouble();
      _current=QVariant(s+et*(e-s));
    } else {
      _current=et<0.5f?_start:_end;
    }
    valueChanged.emit(_current);
  }

protected:
  QVariant     _start, _end, _current;
  int          _duration=250;
  QEasingCurve _easing;
  std::map<float,QVariant> _keys;
};

// ── QPropertyAnimation ────────────────────────────────────────────────────────
class QPropertyAnimation : public QVariantAnimation {
public:
  QPropertyAnimation(QObject* target, const QString& propName, QObject* parent=nullptr)
    : QVariantAnimation(parent), _target(target), _propName(propName) {
    // When value changes, set it on the target
    valueChanged.connect([this](std::vector<QVariant> args){
      if(_target && !args.empty()) _target->setProperty(_propName, args[0]);
    });
  }

  QObject* targetObject()  const { return _target; }
  QString  propertyName()  const { return _propName; }
  void setTargetObject(QObject* t){ _target=t; }
  void setPropertyName(const QString& n){ _propName=n; }

  // If start/end not set, read current property value from target
  void start(DeletionPolicy p=KeepWhenStopped) {
    if(_target && !_start.isValid())
      _start=_target->property(_propName);
    QVariantAnimation::start(p);
  }

private:
  QObject* _target   = nullptr;
  QString  _propName;
};

// ── QSequentialAnimationGroup ─────────────────────────────────────────────────
class QSequentialAnimationGroup : public QAbstractAnimation {
public:
  explicit QSequentialAnimationGroup(QObject* parent=nullptr)
    : QAbstractAnimation(parent) {}
  ~QSequentialAnimationGroup() { clear(); }

  void addAnimation(QAbstractAnimation* a)   { _anims.push_back(a); }
  void addPause(int ms) {
    auto* pa=new QVariantAnimation(this);
    pa->setDuration(ms); pa->setStartValue(QVariant(0)); pa->setEndValue(QVariant(0));
    _anims.push_back(pa);
  }
  void removeAnimation(QAbstractAnimation* a){
    _anims.erase(std::remove(_anims.begin(),_anims.end(),a),_anims.end());
  }
  void clear() { for(auto* a:_anims) delete a; _anims.clear(); }

  int  animationCount() const { return (int)_anims.size(); }
  QAbstractAnimation* animationAt(int i) const {
    return i<(int)_anims.size()?_anims[i]:nullptr;
  }

  int duration() const override {
    int total=0;
    for(auto* a:_anims) total+=a->duration();
    return total;
  }

  void updateCurrentTime(int ms) override {
    // Find which animation we're in
    int elapsed=0;
    for(auto* a:_anims){
      int d=a->duration();
      if(ms<=elapsed+d){
        a->updateCurrentTime(ms-elapsed);
        return;
      }
      elapsed+=d;
    }
  }

  // Sequential: run each animation one by one
  void start(DeletionPolicy p=KeepWhenStopped) {
    _policy=p;
    _runNext(0);
  }

private:
  std::vector<QAbstractAnimation*> _anims;
  DeletionPolicy _policy=KeepWhenStopped;

  void _runNext(int idx) {
    if(idx>=(int)_anims.size()){ finished.emit(); return; }
    auto* a=_anims[idx];
    a->finished.connect([this,idx](std::vector<QVariant>){ _runNext(idx+1); });
    a->start();
  }
};

// ── QParallelAnimationGroup ───────────────────────────────────────────────────
class QParallelAnimationGroup : public QAbstractAnimation {
public:
  explicit QParallelAnimationGroup(QObject* parent=nullptr)
    : QAbstractAnimation(parent) {}
  ~QParallelAnimationGroup() { clear(); }

  void addAnimation(QAbstractAnimation* a) { _anims.push_back(a); }
  void removeAnimation(QAbstractAnimation* a){
    _anims.erase(std::remove(_anims.begin(),_anims.end(),a),_anims.end());
  }
  void clear() { for(auto* a:_anims) delete a; _anims.clear(); }

  int  animationCount() const { return (int)_anims.size(); }
  QAbstractAnimation* animationAt(int i) const {
    return i<(int)_anims.size()?_anims[i]:nullptr;
  }

  int duration() const override {
    int mx=0;
    for(auto* a:_anims) if(a->duration()>mx) mx=a->duration();
    return mx;
  }

  void updateCurrentTime(int ms) override {
    for(auto* a:_anims) a->updateCurrentTime(ms);
  }

  void start(DeletionPolicy p=KeepWhenStopped) {
    if(_anims.empty()){ finished.emit(); return; }
    _done=0;
    for(auto* a:_anims){
      a->finished.connect([this,p](std::vector<QVariant>){
        if(++_done>=(int)_anims.size()){ finished.emit(); if(p==DeleteWhenStopped)deleteLater(); }
      });
      a->start();
    }
  }

  void stop()  { for(auto* a:_anims) a->stop(); }
  void pause() { for(auto* a:_anims) a->pause(); }
  void resume(){ for(auto* a:_anims) a->resume(); }

private:
  std::vector<QAbstractAnimation*> _anims;
  int _done=0;
};

// ── QPauseAnimation ───────────────────────────────────────────────────────────
class QPauseAnimation : public QAbstractAnimation {
public:
  explicit QPauseAnimation(int ms, QObject* parent=nullptr)
    : QAbstractAnimation(parent), _ms(ms) {}
  int  duration() const override { return _ms; }
  void updateCurrentTime(int) override {}
private:
  int _ms;
};

} // namespace NoorQt

using NoorQt::QEasingCurve;
using NoorQt::QAbstractAnimation;
using NoorQt::QVariantAnimation;
using NoorQt::QPropertyAnimation;
using NoorQt::QSequentialAnimationGroup;
using NoorQt::QParallelAnimationGroup;
using NoorQt::QPauseAnimation;
