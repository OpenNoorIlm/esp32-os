// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorQt — QMultimedia.h                                                  ║
// ║  Audio output, media player — mirrors Qt6 QtMultimedia API exactly.      ║
// ║  Backed by ESP32 DAC (GPIO25/26) + I2S + PAM8403 amp.                   ║
// ║  MIT License — NoorRobot / OpenNoorIlm                                  ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once
#include "QObject.h"
#include "QFile.h"
#include <driver/dac.h>
#include <driver/i2s.h>
#include <SPIFFS.h>

namespace NoorQt {

// ── QAudioFormat ─────────────────────────────────────────────────────────────
class QAudioFormat {
public:
  enum SampleFormat { Unknown, UInt8, Int16, Int32, Float };
  enum ChannelConfig { ChannelConfigUnknown, ChannelConfigMono, ChannelConfigStereo };

  QAudioFormat() {}

  void setSampleRate(int r)         { _sampleRate=r; }
  void setChannelCount(int c)       { _channels=c; }
  void setSampleFormat(SampleFormat f){ _fmt=f; }
  void setByteOrder(int)            {}  // always little-endian on ESP32
  void setCodec(const QString&)     {}  // raw PCM only

  int          sampleRate()   const { return _sampleRate; }
  int          channelCount() const { return _channels; }
  SampleFormat sampleFormat() const { return _fmt; }
  int          bytesPerSample()const{
    switch(_fmt){ case UInt8:return 1; case Int16:return 2;
                  case Int32:case Float:return 4; default:return 2; }
  }
  bool isValid() const { return _sampleRate>0 && _channels>0; }

private:
  int          _sampleRate = 44100;
  int          _channels   = 1;
  SampleFormat _fmt        = Int16;
};

// ── QAudioSink (Qt6 name) / QAudioOutput (Qt5 compat) ────────────────────────
class QAudioSink : public QObject {
public:
  enum State { ActiveState, SuspendedState, StoppedState, IdleState };
  Signal<State> stateChanged{"stateChanged"};

  QAudioSink(const QAudioFormat& fmt, QObject* parent=nullptr)
    : QObject(parent), _fmt(fmt) { _setupI2S(); }
  ~QAudioSink() { stop(); i2s_driver_uninstall(I2S_NUM_0); }

  void start(QIODevice* dev) {
    _dev=dev; _state=ActiveState;
    stateChanged.emit(_state);
    _stream();
  }

  void stop()   {
    i2s_stop(I2S_NUM_0);
    _state=StoppedState; stateChanged.emit(_state);
  }
  void suspend(){
    i2s_stop(I2S_NUM_0);
    _state=SuspendedState; stateChanged.emit(_state);
  }
  void resume() {
    i2s_start(I2S_NUM_0);
    _state=ActiveState; stateChanged.emit(_state);
  }

  State   state()          const { return _state; }
  qint64  processedUSecs() const { return _bytesOut*1000000LL/_fmt.sampleRate()/_fmt.bytesPerSample()/_fmt.channelCount(); }
  qint64  bytesFree()      const { return 4096; }
  int     periodSize()     const { return 512; }

  void    setVolume(float v) { _volume=v<0?0:v>1?1:v; }
  float   volume()    const  { return _volume; }

  QString error()     const  { return _errStr; }

private:
  QAudioFormat _fmt;
  QIODevice*   _dev=nullptr;
  State        _state=StoppedState;
  float        _volume=1.0f;
  qint64       _bytesOut=0;
  QString      _errStr;

  void _setupI2S() {
    i2s_config_t cfg = {
      .mode                 = (i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_TX|I2S_MODE_DAC_BUILT_IN),
      .sample_rate          = (uint32_t)_fmt.sampleRate(),
      .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format       = _fmt.channelCount()==2 ? I2S_CHANNEL_FMT_RIGHT_LEFT
                                                      : I2S_CHANNEL_FMT_ONLY_RIGHT,
      .communication_format = I2S_COMM_FORMAT_STAND_MSB,
      .intr_alloc_flags     = 0,
      .dma_buf_count        = 8,
      .dma_buf_len          = 64,
      .use_apll             = false,
      .tx_desc_auto_clear   = true,
    };
    i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
    i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
  }

  void _stream() {
    if(!_dev || !_dev->isOpen()) return;
    uint8_t buf[512];
    while(!_dev->atEnd() && _state==ActiveState) {
      qint64 n=_dev->read((char*)buf,512);
      if(n<=0) break;
      // Apply volume
      if(_volume<1.0f) {
        for(int i=0;i<n;i+=2) {
          int16_t s=*(int16_t*)(buf+i);
          s=(int16_t)(s*_volume);
          *(int16_t*)(buf+i)=s;
        }
      }
      size_t written=0;
      i2s_write(I2S_NUM_0,buf,(size_t)n,&written,portMAX_DELAY);
      _bytesOut+=written;
      yield(); // don't starve watchdog
    }
    if(_dev->atEnd()) { _state=IdleState; stateChanged.emit(_state); }
  }
};

// Qt5-compat alias
using QAudioOutput = QAudioSink;

// ── QSoundEffect ──────────────────────────────────────────────────────────────
// Plays short WAV files from SPIFFS. For robot.say() / sound effects.
class QSoundEffect : public QObject {
public:
  enum Loop { Infinite=-1 };
  Signal<void> playingChanged{"playingChanged"};
  Signal<void> statusChanged {"statusChanged"};

  explicit QSoundEffect(QObject* parent=nullptr) : QObject(parent) {}
  ~QSoundEffect() { stop(); }

  void setSource(const QString& path) { _path=path; _loadWav(); }
  QString source() const { return _path; }

  void setVolume(float v)  { _volume=v<0?0:v>1?1:v; }
  float volume()   const   { return _volume; }

  void setLoopCount(int n) { _loops=n; }
  int  loopCount()  const  { return _loops; }

  bool isPlaying() const   { return _playing; }

  void play() {
    if(_path.isEmpty()) return;
    _playing=true; playingChanged.emit();
    _playRaw();
    _playing=false; playingChanged.emit();
  }

  void stop() { _playing=false; i2s_stop(I2S_NUM_0); }

private:
  QString _path;
  float   _volume=1.0f;
  int     _loops=1;
  bool    _playing=false;
  std::vector<uint8_t> _pcm;
  int     _sampleRate=8000;

  void _loadWav() {
    _pcm.clear();
    File f=SPIFFS.open(_path,"r");
    if(!f) return;
    // Skip 44-byte WAV header; read PCM data
    uint8_t hdr[44]; f.read(hdr,44);
    // Extract sample rate from WAV header (bytes 24-27)
    _sampleRate=(hdr[27]<<24)|(hdr[26]<<16)|(hdr[25]<<8)|hdr[24];
    while(f.available()) _pcm.push_back(f.read());
    f.close();
  }

  void _playRaw() {
    if(_pcm.empty()) return;
    i2s_config_t cfg={
      .mode=(i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_TX|I2S_MODE_DAC_BUILT_IN),
      .sample_rate=(uint32_t)_sampleRate,
      .bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format=I2S_CHANNEL_FMT_ONLY_RIGHT,
      .communication_format=I2S_COMM_FORMAT_STAND_MSB,
      .intr_alloc_flags=0,.dma_buf_count=8,.dma_buf_len=64,
      .use_apll=false,.tx_desc_auto_clear=true,
    };
    i2s_driver_install(I2S_NUM_0,&cfg,0,nullptr);
    i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
    int loops=(_loops==Infinite)?1000:_loops;
    for(int l=0;l<loops&&_playing;l++){
      size_t written=0;
      for(size_t i=0;i<_pcm.size()&&_playing;i+=512){
        size_t chunk=min((size_t)512,_pcm.size()-i);
        // Apply volume
        uint8_t buf[512]; memcpy(buf,_pcm.data()+i,chunk);
        if(_volume<1.0f){
          for(size_t j=0;j<chunk;j+=2){
            int16_t s=*(int16_t*)(buf+j);
            s=(int16_t)(s*_volume);
            *(int16_t*)(buf+j)=s;
          }
        }
        i2s_write(I2S_NUM_0,buf,chunk,&written,portMAX_DELAY);
        yield();
      }
    }
    i2s_driver_uninstall(I2S_NUM_0);
  }
};

// ── QMediaPlayer ──────────────────────────────────────────────────────────────
class QMediaPlayer : public QObject {
public:
  enum PlaybackState { StoppedState, PlayingState, PausedState };
  enum MediaStatus   { NoMedia, LoadedMedia, EndOfMedia, InvalidMedia, LoadingMedia };
  enum Error         { NoError, ResourceError, FormatError, NetworkError };

  Signal<PlaybackState>  playbackStateChanged{"playbackStateChanged"};
  Signal<MediaStatus>    mediaStatusChanged  {"mediaStatusChanged"};
  Signal<qint64>         positionChanged     {"positionChanged"};
  Signal<qint64>         durationChanged     {"durationChanged"};
  Signal<float>          volumeChanged       {"volumeChanged"};
  Signal<Error,QString>  errorOccurred       {"errorOccurred"};

  explicit QMediaPlayer(QObject* parent=nullptr) : QObject(parent) {}
  ~QMediaPlayer() { stop(); }

  void setSource(const QString& path) {
    _source=path;
    _status=LoadedMedia; mediaStatusChanged.emit(_status);
  }
  void setSource(const char* path)  { setSource(QString(path)); }
  QString source()      const       { return _source; }

  void setVolume(float v)           { _volume=v<0?0:v>1?1:v; _sfx.setVolume(_volume); volumeChanged.emit(_volume); }
  float volume()        const       { return _volume; }

  void setPosition(qint64 ms)       { _position=ms; }
  qint64 position()     const       { return _position; }
  qint64 duration()     const       { return _duration; }

  PlaybackState playbackState() const { return _pbState; }
  MediaStatus   mediaStatus()   const { return _status; }

  void play() {
    if(_source.isEmpty()) return;
    _pbState=PlayingState; playbackStateChanged.emit(_pbState);
    _sfx.setSource(_source);
    _sfx.setVolume(_volume);
    _sfx.play();
    _pbState=StoppedState; playbackStateChanged.emit(_pbState);
    _status=EndOfMedia;    mediaStatusChanged.emit(_status);
  }

  void pause() {
    _pbState=PausedState; playbackStateChanged.emit(_pbState);
    _sfx.stop();
  }

  void stop() {
    _sfx.stop();
    _pbState=StoppedState; playbackStateChanged.emit(_pbState);
    _position=0;
  }

private:
  QString       _source;
  float         _volume  = 1.0f;
  qint64        _position= 0;
  qint64        _duration= 0;
  PlaybackState _pbState = StoppedState;
  MediaStatus   _status  = NoMedia;
  QSoundEffect  _sfx;
};

} // namespace NoorQt

using NoorQt::QAudioFormat;
using NoorQt::QAudioSink;
using NoorQt::QAudioOutput;
using NoorQt::QSoundEffect;
using NoorQt::QMediaPlayer;
