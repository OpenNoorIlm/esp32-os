// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorQt — QNetwork.h                                                     ║
// ║  HTTP client, TCP/UDP sockets — mirrors Qt6 QtNetwork API exactly.       ║
// ║  Backed by ESP32 WiFiClient / HTTPClient / AsyncUDP.                     ║
// ║  MIT License — NoorRobot / OpenNoorIlm                                  ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once
#include "QObject.h"
#include "QGeometry.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <AsyncUDP.h>

namespace NoorQt {

// ── QHostAddress ──────────────────────────────────────────────────────────────
class QHostAddress {
public:
  enum SpecialAddress { Null, Broadcast, LocalHost, Any };

  QHostAddress() {}
  explicit QHostAddress(const String& ip) : _addr(ip) {}
  explicit QHostAddress(SpecialAddress s) {
    switch(s){
      case Broadcast: _addr="255.255.255.255"; break;
      case LocalHost: _addr="127.0.0.1"; break;
      case Any:       _addr="0.0.0.0"; break;
      default:        _addr=""; break;
    }
  }

  String toString() const { return _addr; }
  bool isNull()     const { return _addr.isEmpty(); }
  bool isLoopback() const { return _addr=="127.0.0.1"; }

  bool operator==(const QHostAddress& o) const { return _addr==o._addr; }

private:
  String _addr;
};

// ── QNetworkRequest ───────────────────────────────────────────────────────────
class QNetworkRequest {
public:
  enum KnownHeader { ContentTypeHeader, ContentLengthHeader,
                     AuthorizationHeader, UserAgentHeader };
  enum Attribute   { HttpStatusCodeAttribute, RedirectionTargetAttribute };

  QNetworkRequest() {}
  explicit QNetworkRequest(const String& url) : _url(url) {}

  String url() const { return _url; }
  void setUrl(const String& url) { _url = url; }

  void setHeader(KnownHeader h, const String& val) {
    switch(h){
      case ContentTypeHeader:   _headers["Content-Type"]   = val; break;
      case ContentLengthHeader: _headers["Content-Length"] = val; break;
      case AuthorizationHeader: _headers["Authorization"]  = val; break;
      case UserAgentHeader:     _headers["User-Agent"]     = val; break;
    }
  }
  void setRawHeader(const String& name, const String& val) { _headers[name]=val; }
  String rawHeader(const String& name) const {
    auto it = _headers.find(name);
    return it!=_headers.end() ? it->second : String();
  }

  const std::map<String,String>& headers() const { return _headers; }

private:
  String _url;
  std::map<String,String> _headers;
};

// ── QNetworkReply ─────────────────────────────────────────────────────────────
class QNetworkReply : public QObject {
public:
  enum NetworkError {
    NoError=0, ConnectionRefusedError=1, RemoteHostClosedError=2,
    HostNotFoundError=3, TimeoutError=4, UnknownNetworkError=99
  };

  // Signals (mirrors Qt6)
  Signal<void>        finished   {"finished"};
  Signal<int64_t>     downloadProgress{"downloadProgress"};
  Signal<NetworkError,String> errorOccurred{"errorOccurred"};

  QNetworkReply(QObject* parent=nullptr) : QObject(parent) {}

  void setData(const String& d)  { _data=d; }
  void setStatusCode(int c)      { _statusCode=c; }
  void setError(NetworkError e, const String& msg) { _error=e; _errorStr=msg; }
  void setHeader(const String& k, const String& v) { _respHeaders[k]=v; }

  String      readAll()        const { return _data; }
  int         attribute(QNetworkRequest::Attribute a) const {
    if(a==QNetworkRequest::HttpStatusCodeAttribute) return _statusCode;
    return 0;
  }
  NetworkError error()         const { return _error; }
  String       errorString()   const { return _errorStr; }
  bool         isFinished()    const { return _finished; }
  int          statusCode()    const { return _statusCode; }
  String       header(const String& k) const {
    auto it=_respHeaders.find(k); return it!=_respHeaders.end()?it->second:String();
  }
  void markFinished() { _finished=true; finished.emit(); }
  void abort()        { _error=UnknownNetworkError; _errorStr="Aborted"; _finished=true; }

private:
  String   _data;
  int      _statusCode = 0;
  NetworkError _error  = NoError;
  String   _errorStr;
  bool     _finished   = false;
  std::map<String,String> _respHeaders;
};

// ── QNetworkAccessManager ─────────────────────────────────────────────────────
// Mirrors Qt6 QNetworkAccessManager. Uses ESP32 HTTPClient internally.
// All requests are synchronous (ESP32 has no true async HTTP) but the
// reply object + finished signal API is identical to Qt6.
class QNetworkAccessManager : public QObject {
public:
  Signal<QNetworkReply*> finished{"finished"};

  explicit QNetworkAccessManager(QObject* parent=nullptr) : QObject(parent) {}

  // ── GET ───────────────────────────────────────────────────────────────────
  QNetworkReply* get(const QNetworkRequest& req) {
    auto* reply = new QNetworkReply(this);
    _doRequest("GET", req, "", reply);
    return reply;
  }

  // ── POST ──────────────────────────────────────────────────────────────────
  QNetworkReply* post(const QNetworkRequest& req, const String& body) {
    auto* reply = new QNetworkReply(this);
    _doRequest("POST", req, body, reply);
    return reply;
  }

  // ── PUT ───────────────────────────────────────────────────────────────────
  QNetworkReply* put(const QNetworkRequest& req, const String& body) {
    auto* reply = new QNetworkReply(this);
    _doRequest("PUT", req, body, reply);
    return reply;
  }

  // ── DELETE ────────────────────────────────────────────────────────────────
  QNetworkReply* deleteResource(const QNetworkRequest& req) {
    auto* reply = new QNetworkReply(this);
    _doRequest("DELETE", req, "", reply);
    return reply;
  }

  // ── HEAD ──────────────────────────────────────────────────────────────────
  QNetworkReply* head(const QNetworkRequest& req) {
    auto* reply = new QNetworkReply(this);
    _doRequest("HEAD", req, "", reply);
    return reply;
  }

private:
  void _doRequest(const String& method, const QNetworkRequest& req,
                  const String& body, QNetworkReply* reply) {
    if(WiFi.status() != WL_CONNECTED) {
      reply->setError(QNetworkReply::UnknownNetworkError, "WiFi not connected");
      reply->markFinished();
      finished.emit(reply);
      return;
    }

    HTTPClient http;
    http.begin(req.url());
    http.setTimeout(10000);

    // Apply headers
    for(auto& kv : req.headers())
      http.addHeader(kv.first, kv.second);

    int code = -1;
    if(method=="GET")    code = http.GET();
    else if(method=="POST")   code = http.POST(body);
    else if(method=="PUT")    code = http.PUT(body);
    else if(method=="DELETE") code = http.sendRequest("DELETE");
    else if(method=="HEAD")   code = http.sendRequest("HEAD");

    reply->setStatusCode(code);
    if(code>0) {
      if(method != "HEAD")
        reply->setData(http.getString());
      reply->setHeader("Content-Type", http.header("Content-Type"));
    } else {
      reply->setError(QNetworkReply::HostNotFoundError,
                      "HTTP error: " + String(code));
    }
    http.end();
    reply->markFinished();
    finished.emit(reply);
  }
};

// ── QTcpSocket ────────────────────────────────────────────────────────────────
class QTcpSocket : public QObject {
public:
  enum SocketState  { UnconnectedState, ConnectingState, ConnectedState,
                      ClosingState, BoundState };
  enum SocketError  { ConnectionRefusedError, RemoteHostClosedError,
                      HostNotFoundError, SocketTimeoutError, UnknownSocketError };
  enum OpenMode     { ReadOnly=1, WriteOnly=2, ReadWrite=3 };

  Signal<void>        connected       {"connected"};
  Signal<void>        disconnected    {"disconnected"};
  Signal<void>        readyRead       {"readyRead"};
  Signal<SocketError> errorOccurred   {"errorOccurred"};
  Signal<qint64>      bytesWritten    {"bytesWritten"};

  explicit QTcpSocket(QObject* parent=nullptr) : QObject(parent) {}
  ~QTcpSocket() { if(_client.connected()) _client.stop(); }

  // ── Connection ────────────────────────────────────────────────────────────
  void connectToHost(const QString& host, quint16 port) {
    _state = ConnectingState;
    if(_client.connect(host.c_str(), port)) {
      _state = ConnectedState;
      connected.emit();
    } else {
      _state = UnconnectedState;
      errorOccurred.emit(HostNotFoundError);
    }
  }
  void connectToHost(const QHostAddress& addr, quint16 port) {
    connectToHost(addr.toString(), port);
  }

  void disconnectFromHost() {
    _client.stop();
    _state = UnconnectedState;
    disconnected.emit();
  }

  void abort() { _client.stop(); _state=UnconnectedState; }

  bool waitForConnected(int ms=3000) {
    unsigned long t=millis();
    while(!_client.connected() && millis()-t<(unsigned long)ms) delay(10);
    return _client.connected();
  }
  bool waitForReadyRead(int ms=3000) {
    unsigned long t=millis();
    while(!_client.available() && _client.connected() && millis()-t<(unsigned long)ms) delay(10);
    return _client.available()>0;
  }
  bool waitForBytesWritten(int ms=3000) { (void)ms; return true; }
  bool waitForDisconnected(int ms=3000) {
    unsigned long t=millis();
    while(_client.connected() && millis()-t<(unsigned long)ms) delay(10);
    return !_client.connected();
  }

  // ── I/O ──────────────────────────────────────────────────────────────────
  qint64 write(const QString& data) {
    if(!_client.connected()) return -1;
    size_t n = _client.print(data);
    bytesWritten.emit((qint64)n);
    return (qint64)n;
  }
  qint64 write(const char* data, qint64 len) {
    if(!_client.connected()) return -1;
    size_t n = _client.write((const uint8_t*)data, len);
    bytesWritten.emit((qint64)n);
    return (qint64)n;
  }

  QString read(qint64 maxlen=4096) {
    String out;
    while(_client.available() && (qint64)out.length()<maxlen)
      out += (char)_client.read();
    return out;
  }
  QString readAll() { return read(65536); }
  QString readLine(qint64 maxlen=1024) {
    return _client.readStringUntil('\n');
  }

  bool    atEnd()         const { return !const_cast<WiFiClient&>(_client).available(); }
  qint64  bytesAvailable()const { return const_cast<WiFiClient&>(_client).available(); }

  // ── State ─────────────────────────────────────────────────────────────────
  SocketState state()     const { return _state; }
  bool isConnected()      const { return _state==ConnectedState; }
  bool isOpen()           const { return _state==ConnectedState; }

  QHostAddress peerAddress() const { return QHostAddress(_client.remoteIP().toString()); }
  quint16      peerPort()    const { return _client.remotePort(); }
  QHostAddress localAddress()const { return QHostAddress(WiFi.localIP().toString()); }
  quint16      localPort()   const { return 0; }

  QString peerName() const { return _peerName; }

  SocketError error() const { return _lastError; }

  // ── Poll (call from loop()) ───────────────────────────────────────────────
  void poll() {
    if(_state==ConnectedState) {
      if(!_client.connected()) { _state=UnconnectedState; disconnected.emit(); }
      else if(_client.available()) readyRead.emit();
    }
  }

private:
  WiFiClient  _client;
  SocketState _state     = UnconnectedState;
  SocketError _lastError = UnknownSocketError;
  QString     _peerName;
};

// ── QUdpSocket ────────────────────────────────────────────────────────────────
class QUdpSocket : public QObject {
public:
  Signal<void> readyRead{"readyRead"};

  explicit QUdpSocket(QObject* parent=nullptr) : QObject(parent) {}
  ~QUdpSocket() { _udp.close(); }

  bool bind(quint16 port) {
    _port = port;
    return _udp.listen(port);
  }
  bool bind(const QHostAddress&, quint16 port) { return bind(port); }

  qint64 writeDatagram(const QString& data, const QHostAddress& host, quint16 port) {
    AsyncUDPMessage msg;
    msg.write((const uint8_t*)data.c_str(), data.length());
    return _udp.sendTo(msg, IPAddress(), port) ? data.length() : -1;
  }

  bool hasPendingDatagrams()  const { return _pending.length()>0; }
  qint64 pendingDatagramSize()const { return _pending.length(); }

  QString receiveDatagram() {
    QString d = _pending;
    _pending = "";
    return d;
  }

  void poll() {
    // AsyncUDP fires callback — call readyRead from there
  }

  void onPacket(const QString& data) {
    _pending = data;
    readyRead.emit();
  }

private:
  AsyncUDP _udp;
  quint16  _port = 0;
  QString  _pending;
};

// ── QDnsLookup ────────────────────────────────────────────────────────────────
class QDnsLookup : public QObject {
public:
  enum Type { A, AAAA, CNAME, MX, NS, SRV, TXT };
  Signal<void> finished{"finished"};

  QDnsLookup(Type t, const QString& name, QObject* parent=nullptr)
    : QObject(parent), _type(t), _name(name) {}

  void lookup() {
    // ESP32: only A record lookup via WiFi.hostByName
    IPAddress ip;
    if(WiFi.hostByName(_name.c_str(), ip)==1) {
      _result = ip.toString();
      _error  = false;
    } else {
      _error  = true;
    }
    finished.emit();
  }

  bool    isFinished()     const { return true; }
  bool    error()          const { return _error; }
  QString errorString()    const { return _error?"Host not found":""; }
  QString hostAddressRecords() const { return _result; }

private:
  Type    _type;
  QString _name;
  QString _result;
  bool    _error = false;
};

} // namespace NoorQt

// ── Global using declarations (match Qt6 style) ───────────────────────────────
using NoorQt::QHostAddress;
using NoorQt::QNetworkRequest;
using NoorQt::QNetworkReply;
using NoorQt::QNetworkAccessManager;
using NoorQt::QTcpSocket;
using NoorQt::QUdpSocket;
using NoorQt::QDnsLookup;
