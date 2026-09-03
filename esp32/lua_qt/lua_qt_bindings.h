// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorQt — lua_qt_bindings.h                                              ║
// ║  Lua 5.4 bindings for the full NoorQt framework.                         ║
// ║  Exposes: Qt.QFile, Qt.QDir, Qt.QSettings, Qt.QNetwork,                 ║
// ║           Qt.QSqlDatabase, Qt.QThread, Qt.QTimer,                       ║
// ║           Qt.QSoundEffect, Qt.QMediaPlayer,                              ║
// ║           Qt.QPropertyAnimation, Qt.QEasingCurve                        ║
// ║                                                                          ║
// ║  Usage in lua_engine.cpp:                                                ║
// ║    #include "lua_qt/lua_qt_bindings.h"                                  ║
// ║    // Inside LuaEngine::init():                                          ║
// ║    LuaQt::registerAll(L);                                                ║
// ║                                                                          ║
// ║  Usage in Lua:                                                           ║
// ║    local f = Qt.QFile("/myfile.txt")                                     ║
// ║    f:open("w")                                                           ║
// ║    f:write("hello")                                                      ║
// ║    f:close()                                                             ║
// ║                                                                          ║
// ║  MIT License — NoorRobot / OpenNoorIlm                                  ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once

// NoorQt modules
#include "NoorQt.h"

#include "../lua_config.h"
extern "C" {
#include "../lua_src/lua-master/lua.h"
#include "../lua_src/lua-master/lauxlib.h"
}

namespace LuaQt {

// ════════════════════════════════════════════════════════════════════════════
// Helpers
// ════════════════════════════════════════════════════════════════════════════

// Push nil + error message, return 2
static int lua_err(lua_State* L, const char* msg) {
  lua_pushnil(L);
  lua_pushstring(L, msg);
  return 2;
}

// Get a userdata of a given type from a Lua stack index
template<typename T>
static T* checkUD(lua_State* L, int idx, const char* mt) {
  return *static_cast<T**>(luaL_checkudata(L, idx, mt));
}

// Push a pointer as userdata with metatable
template<typename T>
static T** newUD(lua_State* L, const char* mt) {
  T** pp = static_cast<T**>(lua_newuserdata(L, sizeof(T*)));
  luaL_setmetatable(L, mt);
  return pp;
}

// ════════════════════════════════════════════════════════════════════════════
// Qt.QFile
// ════════════════════════════════════════════════════════════════════════════
#define MT_QFILE "Qt.QFile"

static int qfile_new(lua_State* L) {
  const char* path = luaL_optstring(L, 1, "");
  QFile** pp = newUD<QFile>(L, MT_QFILE);
  *pp = new QFile(path);
  return 1;
}
static int qfile_open(lua_State* L) {
  QFile* f = checkUD<QFile>(L, 1, MT_QFILE);
  const char* mode = luaL_optstring(L, 2, "r");
  QIODevice::OpenMode m = QIODevice::ReadOnly;
  String ms(mode);
  if (ms=="w") m=QIODevice::WriteOnly;
  else if(ms=="r+") m=QIODevice::ReadWrite;
  else if(ms=="a") m=(QIODevice::OpenMode)(QIODevice::WriteOnly|QIODevice::Append);
  lua_pushboolean(L, f->open(m));
  return 1;
}
static int qfile_close(lua_State* L) {
  checkUD<QFile>(L, 1, MT_QFILE)->close();
  return 0;
}
static int qfile_read(lua_State* L) {
  QFile* f = checkUD<QFile>(L, 1, MT_QFILE);
  int max = (int)luaL_optinteger(L, 2, 4096);
  std::vector<char> buf(max+1, 0);
  qint64 n = f->read(buf.data(), max);
  if(n<0) lua_pushnil(L);
  else lua_pushlstring(L, buf.data(), (size_t)n);
  return 1;
}
static int qfile_readAll(lua_State* L) {
  QFile* f = checkUD<QFile>(L, 1, MT_QFILE);
  String s = f->readAll();
  lua_pushlstring(L, s.c_str(), s.length());
  return 1;
}
static int qfile_readLine(lua_State* L) {
  QFile* f = checkUD<QFile>(L, 1, MT_QFILE);
  String s = f->readLine();
  lua_pushlstring(L, s.c_str(), s.length());
  return 1;
}
static int qfile_write(lua_State* L) {
  QFile* f = checkUD<QFile>(L, 1, MT_QFILE);
  size_t len; const char* data = luaL_checklstring(L, 2, &len);
  lua_pushinteger(L, (lua_Integer)f->write(data, (qint64)len));
  return 1;
}
static int qfile_atEnd(lua_State* L) {
  lua_pushboolean(L, checkUD<QFile>(L,1,MT_QFILE)->atEnd());
  return 1;
}
static int qfile_exists(lua_State* L) {
  // Can be called as method or static: Qt.QFile.exists("/path")
  if(lua_isstring(L,1)) { lua_pushboolean(L, QFile::exists(luaL_checkstring(L,1))); return 1; }
  lua_pushboolean(L, checkUD<QFile>(L,1,MT_QFILE)->exists());
  return 1;
}
static int qfile_remove(lua_State* L) {
  if(lua_isstring(L,1)) { lua_pushboolean(L, QFile::remove(luaL_checkstring(L,1))); return 1; }
  lua_pushboolean(L, checkUD<QFile>(L,1,MT_QFILE)->remove());
  return 1;
}
static int qfile_rename(lua_State* L) {
  QFile* f = checkUD<QFile>(L, 1, MT_QFILE);
  lua_pushboolean(L, f->rename(luaL_checkstring(L,2)));
  return 1;
}
static int qfile_size(lua_State* L) {
  lua_pushinteger(L, (lua_Integer)checkUD<QFile>(L,1,MT_QFILE)->size());
  return 1;
}
static int qfile_seek(lua_State* L) {
  lua_pushboolean(L, checkUD<QFile>(L,1,MT_QFILE)->seek((qint64)luaL_checkinteger(L,2)));
  return 1;
}
static int qfile_pos(lua_State* L) {
  lua_pushinteger(L, (lua_Integer)checkUD<QFile>(L,1,MT_QFILE)->pos());
  return 1;
}
static int qfile_gc(lua_State* L) {
  delete checkUD<QFile>(L, 1, MT_QFILE);
  return 0;
}
static int qfile_tostring(lua_State* L) {
  QFile* f = checkUD<QFile>(L, 1, MT_QFILE);
  lua_pushfstring(L, "QFile(%s)", f->fileName().c_str());
  return 1;
}

static const luaL_Reg qfile_methods[] = {
  {"open",     qfile_open},
  {"close",    qfile_close},
  {"read",     qfile_read},
  {"readAll",  qfile_readAll},
  {"readLine", qfile_readLine},
  {"write",    qfile_write},
  {"atEnd",    qfile_atEnd},
  {"exists",   qfile_exists},
  {"remove",   qfile_remove},
  {"rename",   qfile_rename},
  {"size",     qfile_size},
  {"seek",     qfile_seek},
  {"pos",      qfile_pos},
  {"__gc",     qfile_gc},
  {"__tostring",qfile_tostring},
  {nullptr,    nullptr}
};

// ════════════════════════════════════════════════════════════════════════════
// Qt.QDir
// ════════════════════════════════════════════════════════════════════════════
#define MT_QDIR "Qt.QDir"

static int qdir_new(lua_State* L) {
  const char* path = luaL_optstring(L, 1, "/");
  QDir** pp = newUD<QDir>(L, MT_QDIR);
  *pp = new QDir(path);
  return 1;
}
static int qdir_entryList(lua_State* L) {
  QDir* d = checkUD<QDir>(L, 1, MT_QDIR);
  auto list = d->entryList();
  lua_newtable(L);
  for(int i=0;i<(int)list.size();i++){
    lua_pushstring(L, list[i].c_str());
    lua_rawseti(L, -2, i+1);
  }
  return 1;
}
static int qdir_exists(lua_State* L) {
  QDir* d = checkUD<QDir>(L, 1, MT_QDIR);
  if(lua_isstring(L,2)) lua_pushboolean(L, d->exists(luaL_checkstring(L,2)));
  else lua_pushboolean(L, d->exists());
  return 1;
}
static int qdir_mkdir(lua_State* L) {
  lua_pushboolean(L, checkUD<QDir>(L,1,MT_QDIR)->mkdir(luaL_checkstring(L,2)));
  return 1;
}
static int qdir_path(lua_State* L) {
  lua_pushstring(L, checkUD<QDir>(L,1,MT_QDIR)->path().c_str());
  return 1;
}
static int qdir_filePath(lua_State* L) {
  lua_pushstring(L, checkUD<QDir>(L,1,MT_QDIR)->filePath(luaL_checkstring(L,2)).c_str());
  return 1;
}
static int qdir_gc(lua_State* L) {
  delete checkUD<QDir>(L, 1, MT_QDIR); return 0;
}
static const luaL_Reg qdir_methods[] = {
  {"entryList", qdir_entryList},
  {"exists",    qdir_exists},
  {"mkdir",     qdir_mkdir},
  {"path",      qdir_path},
  {"filePath",  qdir_filePath},
  {"__gc",      qdir_gc},
  {nullptr,     nullptr}
};

// ════════════════════════════════════════════════════════════════════════════
// Qt.QSettings
// ════════════════════════════════════════════════════════════════════════════
#define MT_QSETTINGS "Qt.QSettings"

static int qsettings_new(lua_State* L) {
  const char* org = luaL_checkstring(L, 1);
  const char* app = luaL_optstring(L, 2, "app");
  QSettings** pp = newUD<QSettings>(L, MT_QSETTINGS);
  *pp = new QSettings(org, app);
  return 1;
}
static int qsettings_setValue(lua_State* L) {
  QSettings* s = checkUD<QSettings>(L, 1, MT_QSETTINGS);
  const char* key = luaL_checkstring(L, 2);
  if(lua_isboolean(L,3))      s->setValue(key, QVariant((bool)lua_toboolean(L,3)));
  else if(lua_isinteger(L,3)) s->setValue(key, QVariant((int)lua_tointeger(L,3)));
  else if(lua_isnumber(L,3))  s->setValue(key, QVariant((double)lua_tonumber(L,3)));
  else                         s->setValue(key, QVariant(luaL_checkstring(L,3)));
  return 0;
}
static int qsettings_value(lua_State* L) {
  QSettings* s = checkUD<QSettings>(L, 1, MT_QSETTINGS);
  const char* key = luaL_checkstring(L, 2);
  QVariant def;
  if(lua_isstring(L,3)) def=QVariant(lua_tostring(L,3));
  QVariant v = s->value(key, def);
  if(!v.isValid()) { lua_pushnil(L); return 1; }
  if(v.type()==QVariant::Bool)   { lua_pushboolean(L,v.toBool()); return 1; }
  if(v.type()==QVariant::Int)    { lua_pushinteger(L,v.toInt()); return 1; }
  if(v.type()==QVariant::Double) { lua_pushnumber(L,v.toDouble()); return 1; }
  lua_pushstring(L, v.toString().c_str());
  return 1;
}
static int qsettings_contains(lua_State* L) {
  lua_pushboolean(L, checkUD<QSettings>(L,1,MT_QSETTINGS)->contains(luaL_checkstring(L,2)));
  return 1;
}
static int qsettings_remove(lua_State* L) {
  checkUD<QSettings>(L,1,MT_QSETTINGS)->remove(luaL_checkstring(L,2));
  return 0;
}
static int qsettings_sync(lua_State* L) {
  checkUD<QSettings>(L,1,MT_QSETTINGS)->sync();
  return 0;
}
static int qsettings_allKeys(lua_State* L) {
  auto keys = checkUD<QSettings>(L,1,MT_QSETTINGS)->allKeys();
  lua_newtable(L);
  for(int i=0;i<(int)keys.size();i++){
    lua_pushstring(L,keys[i].c_str()); lua_rawseti(L,-2,i+1);
  }
  return 1;
}
static int qsettings_beginGroup(lua_State* L) {
  checkUD<QSettings>(L,1,MT_QSETTINGS)->beginGroup(luaL_checkstring(L,2)); return 0;
}
static int qsettings_endGroup(lua_State* L) {
  checkUD<QSettings>(L,1,MT_QSETTINGS)->endGroup(); return 0;
}
static int qsettings_gc(lua_State* L) {
  delete checkUD<QSettings>(L, 1, MT_QSETTINGS); return 0;
}
static const luaL_Reg qsettings_methods[] = {
  {"setValue",   qsettings_setValue},
  {"value",      qsettings_value},
  {"contains",   qsettings_contains},
  {"remove",     qsettings_remove},
  {"sync",       qsettings_sync},
  {"allKeys",    qsettings_allKeys},
  {"beginGroup", qsettings_beginGroup},
  {"endGroup",   qsettings_endGroup},
  {"__gc",       qsettings_gc},
  {nullptr,      nullptr}
};

// ════════════════════════════════════════════════════════════════════════════
// Qt.QTimer
// ════════════════════════════════════════════════════════════════════════════
#define MT_QTIMER "Qt.QTimer"

static int qtimer_new(lua_State* L) {
  QTimer** pp = newUD<QTimer>(L, MT_QTIMER);
  *pp = new QTimer();
  return 1;
}
static int qtimer_start(lua_State* L) {
  QTimer* t = checkUD<QTimer>(L, 1, MT_QTIMER);
  int ms = (int)luaL_optinteger(L, 2, 1000);
  t->start(ms);
  return 0;
}
static int qtimer_stop(lua_State* L) {
  checkUD<QTimer>(L,1,MT_QTIMER)->stop(); return 0;
}
static int qtimer_setSingleShot(lua_State* L) {
  checkUD<QTimer>(L,1,MT_QTIMER)->setSingleShot(lua_toboolean(L,2)); return 0;
}
static int qtimer_isActive(lua_State* L) {
  lua_pushboolean(L, checkUD<QTimer>(L,1,MT_QTIMER)->isActive()); return 1;
}
static int qtimer_interval(lua_State* L) {
  lua_pushinteger(L, checkUD<QTimer>(L,1,MT_QTIMER)->interval()); return 1;
}
static int qtimer_setInterval(lua_State* L) {
  checkUD<QTimer>(L,1,MT_QTIMER)->setInterval((int)luaL_checkinteger(L,2)); return 0;
}
static int qtimer_onTimeout(lua_State* L) {
  // Qt.QTimer:onTimeout(function() ... end)
  // Store the Lua function in the registry, wire to timeout signal
  QTimer* t = checkUD<QTimer>(L, 1, MT_QTIMER);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  lua_State* Lref = L;
  int ref = luaL_ref(L, LUA_REGISTRYINDEX);
  t->timeout.connect([Lref, ref](){
    lua_rawgeti(Lref, LUA_REGISTRYINDEX, ref);
    lua_pcall(Lref, 0, 0, 0);
  });
  return 0;
}
static int qtimer_gc(lua_State* L) {
  delete checkUD<QTimer>(L, 1, MT_QTIMER); return 0;
}
static const luaL_Reg qtimer_methods[] = {
  {"start",        qtimer_start},
  {"stop",         qtimer_stop},
  {"setSingleShot",qtimer_setSingleShot},
  {"isActive",     qtimer_isActive},
  {"interval",     qtimer_interval},
  {"setInterval",  qtimer_setInterval},
  {"onTimeout",    qtimer_onTimeout},
  {"__gc",         qtimer_gc},
  {nullptr,        nullptr}
};

// ════════════════════════════════════════════════════════════════════════════
// Qt.QNetwork (HTTP)
// ════════════════════════════════════════════════════════════════════════════
#define MT_QNAM "Qt.QNetworkAccessManager"

static int qnam_new(lua_State* L) {
  QNetworkAccessManager** pp = newUD<QNetworkAccessManager>(L, MT_QNAM);
  *pp = new QNetworkAccessManager();
  return 1;
}
// Synchronous get() — returns {status, body} or nil, err
static int qnam_get(lua_State* L) {
  QNetworkAccessManager* nam = checkUD<QNetworkAccessManager>(L, 1, MT_QNAM);
  const char* url = luaL_checkstring(L, 2);
  QNetworkRequest req(url);
  // Optional headers table
  if(lua_istable(L,3)){
    lua_pushnil(L);
    while(lua_next(L,3)){
      req.setRawHeader(lua_tostring(L,-2), lua_tostring(L,-1));
      lua_pop(L,1);
    }
  }
  QNetworkReply* reply = nam->get(req);
  if(reply->error()!=QNetworkReply::NoError){
    lua_pushnil(L); lua_pushstring(L,reply->errorString().c_str());
    delete reply; return 2;
  }
  lua_pushinteger(L, reply->statusCode());
  lua_pushstring(L, reply->readAll().c_str());
  delete reply; return 2;
}
static int qnam_post(lua_State* L) {
  QNetworkAccessManager* nam = checkUD<QNetworkAccessManager>(L, 1, MT_QNAM);
  const char* url  = luaL_checkstring(L, 2);
  const char* body = luaL_optstring(L, 3, "");
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  QNetworkReply* reply = nam->post(req, body);
  if(reply->error()!=QNetworkReply::NoError){
    lua_pushnil(L); lua_pushstring(L,reply->errorString().c_str());
    delete reply; return 2;
  }
  lua_pushinteger(L, reply->statusCode());
  lua_pushstring(L, reply->readAll().c_str());
  delete reply; return 2;
}
static int qnam_gc(lua_State* L) {
  delete checkUD<QNetworkAccessManager>(L, 1, MT_QNAM); return 0;
}
static const luaL_Reg qnam_methods[] = {
  {"get",  qnam_get},
  {"post", qnam_post},
  {"__gc", qnam_gc},
  {nullptr,nullptr}
};

// ════════════════════════════════════════════════════════════════════════════
// Qt.QSqlDatabase  +  Qt.QSqlQuery
// ════════════════════════════════════════════════════════════════════════════
#define MT_QSQLDB "Qt.QSqlDatabase"
#define MT_QSQLQ  "Qt.QSqlQuery"

static int qsqldb_open(lua_State* L) {
  const char* path   = luaL_checkstring(L, 1);
  const char* driver = luaL_optstring(L, 2, "QSQLITE");
  QSqlDatabase db = QSqlDatabase::addDatabase(driver, path);
  db.setDatabaseName(path);
  if(!db.open()){
    lua_pushnil(L); lua_pushstring(L, db.lastError().text().c_str());
    return 2;
  }
  // Store db connection name so we can retrieve later
  lua_pushstring(L, path);
  return 1; // returns the connection name as a handle
}

// exec(connName, sql) → table of rows, or nil+err
static int qsqldb_exec(lua_State* L) {
  const char* conn = luaL_checkstring(L, 1);
  const char* sql  = luaL_checkstring(L, 2);
  QSqlDatabase db = QSqlDatabase::database(conn);
  if(!db.isOpen()) return lua_err(L, "Database not open");
  QSqlQuery q = db.exec(sql);
  if(q.lastError().isValid()){
    lua_pushnil(L); lua_pushstring(L, q.lastError().text().c_str());
    return 2;
  }
  // Build result as array of tables
  lua_newtable(L);
  int row=1;
  while(q.next()){
    lua_newtable(L);
    QSqlRecord rec = q.record();
    for(int i=0;i<rec.count();i++){
      lua_pushstring(L, rec.value(i).toString().c_str());
      lua_setfield(L, -2, rec.fieldName(i).c_str());
    }
    lua_rawseti(L, -2, row++);
  }
  return 1;
}

static int qsqldb_close(lua_State* L) {
  const char* conn = luaL_checkstring(L, 1);
  QSqlDatabase::database(conn).close();
  QSqlDatabase::removeDatabase(conn);
  return 0;
}

// ════════════════════════════════════════════════════════════════════════════
// Qt.QThread  (run a Lua coroutine in a FreeRTOS task)
// ════════════════════════════════════════════════════════════════════════════
#define MT_QTHREAD "Qt.QThread"

struct LuaThread {
  lua_State* co;      // coroutine
  lua_State* parent;  // parent state (for registry refs)
  int        fnRef;   // registry ref of the Lua function
  bool       running;
  TaskHandle_t task;
};

static int qthread_new(lua_State* L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  LuaThread** pp = newUD<LuaThread>(L, MT_QTHREAD);
  LuaThread* lt = new LuaThread();
  lt->parent  = L;
  lt->running = false;
  lt->task    = nullptr;
  // Store function ref
  lua_pushvalue(L, 1);
  lt->fnRef = luaL_ref(L, LUA_REGISTRYINDEX);
  // Create coroutine
  lt->co = lua_newthread(L);
  lua_rawgeti(lt->co, LUA_REGISTRYINDEX, lt->fnRef);
  *pp = lt;
  return 1;
}

static int qthread_start(lua_State* L) {
  LuaThread* lt = checkUD<LuaThread>(L, 1, MT_QTHREAD);
  if(lt->running) return 0;
  lt->running = true;
  xTaskCreate([](void* arg){
    LuaThread* lt2 = static_cast<LuaThread*>(arg);
    int nres=0;
    lua_resume(lt2->co, nullptr, 0, &nres);
    lt2->running=false;
    vTaskDelete(nullptr);
  }, "lua_thread", 8192, lt, 3, &lt->task);
  return 0;
}

static int qthread_isRunning(lua_State* L) {
  lua_pushboolean(L, checkUD<LuaThread>(L,1,MT_QTHREAD)->running);
  return 1;
}

static int qthread_gc(lua_State* L) {
  LuaThread* lt = checkUD<LuaThread>(L, 1, MT_QTHREAD);
  luaL_unref(lt->parent, LUA_REGISTRYINDEX, lt->fnRef);
  delete lt;
  return 0;
}

static const luaL_Reg qthread_methods[] = {
  {"start",     qthread_start},
  {"isRunning", qthread_isRunning},
  {"__gc",      qthread_gc},
  {nullptr,     nullptr}
};

// ════════════════════════════════════════════════════════════════════════════
// Qt.QSoundEffect / Qt.QMediaPlayer
// ════════════════════════════════════════════════════════════════════════════
#define MT_QSFX "Qt.QSoundEffect"

static int qsfx_new(lua_State* L) {
  const char* src = luaL_optstring(L, 1, "");
  QSoundEffect** pp = newUD<QSoundEffect>(L, MT_QSFX);
  *pp = new QSoundEffect();
  if(src[0]) (*pp)->setSource(src);
  return 1;
}
static int qsfx_play(lua_State* L) {
  checkUD<QSoundEffect>(L,1,MT_QSFX)->play(); return 0;
}
static int qsfx_stop(lua_State* L) {
  checkUD<QSoundEffect>(L,1,MT_QSFX)->stop(); return 0;
}
static int qsfx_setSource(lua_State* L) {
  checkUD<QSoundEffect>(L,1,MT_QSFX)->setSource(luaL_checkstring(L,2)); return 0;
}
static int qsfx_setVolume(lua_State* L) {
  checkUD<QSoundEffect>(L,1,MT_QSFX)->setVolume((float)luaL_checknumber(L,2)); return 0;
}
static int qsfx_setLoopCount(lua_State* L) {
  checkUD<QSoundEffect>(L,1,MT_QSFX)->setLoopCount((int)luaL_checkinteger(L,2)); return 0;
}
static int qsfx_gc(lua_State* L) {
  delete checkUD<QSoundEffect>(L, 1, MT_QSFX); return 0;
}
static const luaL_Reg qsfx_methods[] = {
  {"play",         qsfx_play},
  {"stop",         qsfx_stop},
  {"setSource",    qsfx_setSource},
  {"setVolume",    qsfx_setVolume},
  {"setLoopCount", qsfx_setLoopCount},
  {"__gc",         qsfx_gc},
  {nullptr,        nullptr}
};

// ════════════════════════════════════════════════════════════════════════════
// Qt.QPropertyAnimation
// ════════════════════════════════════════════════════════════════════════════
#define MT_QANIM "Qt.QPropertyAnimation"

// Lightweight standalone version — animates a Lua number via callback
struct LuaAnim {
  int      durationMs;
  float    startVal, endVal;
  int      easingType;
  int      cbRef;       // registry ref of Lua callback function(value)
  lua_State* L;
  bool     running;
  TimerHandle_t timer;
  unsigned long startMs;
};

static void luaAnimTick(TimerHandle_t h) {
  LuaAnim* a = static_cast<LuaAnim*>(pvTimerGetTimerID(h));
  unsigned long elapsed = millis() - a->startMs;
  float t = (float)elapsed / (float)(a->durationMs>0?a->durationMs:1);
  if(t>1.0f) t=1.0f;
  QEasingCurve ec((QEasingCurve::Type)a->easingType);
  float et = ec.valueForProgress(t);
  float val = a->startVal + et*(a->endVal - a->startVal);
  // Call Lua callback
  lua_rawgeti(a->L, LUA_REGISTRYINDEX, a->cbRef);
  lua_pushnumber(a->L, val);
  lua_pcall(a->L, 1, 0, 0);
  if(t>=1.0f){
    xTimerStop(h,0); xTimerDelete(h,0); a->timer=nullptr; a->running=false;
  }
}

static int qanim_new(lua_State* L) {
  float sv   = (float)luaL_checknumber(L, 1);
  float ev   = (float)luaL_checknumber(L, 2);
  int   dur  = (int)luaL_checkinteger(L, 3);
  luaL_checktype(L, 4, LUA_TFUNCTION);
  int   ease = (int)luaL_optinteger(L, 5, (int)QEasingCurve::Linear);

  LuaAnim** pp = newUD<LuaAnim>(L, MT_QANIM);
  LuaAnim* a = new LuaAnim();
  a->L=L; a->startVal=sv; a->endVal=ev; a->durationMs=dur;
  a->easingType=ease; a->running=false; a->timer=nullptr;
  lua_pushvalue(L, 4);
  a->cbRef = luaL_ref(L, LUA_REGISTRYINDEX);
  *pp = a;
  return 1;
}
static int qanim_start(lua_State* L) {
  LuaAnim* a = checkUD<LuaAnim>(L, 1, MT_QANIM);
  if(a->running) return 0;
  a->startMs=millis(); a->running=true;
  a->timer=xTimerCreate("anim",pdMS_TO_TICKS(16),pdTRUE,a,luaAnimTick);
  if(a->timer) xTimerStart(a->timer,0);
  return 0;
}
static int qanim_stop(lua_State* L) {
  LuaAnim* a = checkUD<LuaAnim>(L, 1, MT_QANIM);
  if(a->timer){ xTimerStop(a->timer,0); xTimerDelete(a->timer,0); a->timer=nullptr; }
  a->running=false;
  return 0;
}
static int qanim_isRunning(lua_State* L) {
  lua_pushboolean(L, checkUD<LuaAnim>(L,1,MT_QANIM)->running); return 1;
}
static int qanim_gc(lua_State* L) {
  LuaAnim* a = checkUD<LuaAnim>(L, 1, MT_QANIM);
  if(a->timer){ xTimerStop(a->timer,0); xTimerDelete(a->timer,0); }
  luaL_unref(a->L, LUA_REGISTRYINDEX, a->cbRef);
  delete a; return 0;
}
static const luaL_Reg qanim_methods[] = {
  {"start",     qanim_start},
  {"stop",      qanim_stop},
  {"isRunning", qanim_isRunning},
  {"__gc",      qanim_gc},
  {nullptr,     nullptr}
};

// ════════════════════════════════════════════════════════════════════════════
// Qt.sleep / Qt.msleep / Qt.yield
// ════════════════════════════════════════════════════════════════════════════
static int qt_sleep(lua_State* L) {
  delay((int)luaL_checkinteger(L,1)*1000); return 0;
}
static int qt_msleep(lua_State* L) {
  delay((int)luaL_checkinteger(L,1)); return 0;
}
static int qt_yield(lua_State*) {
  taskYIELD(); return 0;
}
static int qt_millis(lua_State* L) {
  lua_pushinteger(L, (lua_Integer)millis()); return 1;
}
static int qt_micros(lua_State* L) {
  lua_pushinteger(L, (lua_Integer)micros()); return 1;
}

// ════════════════════════════════════════════════════════════════════════════
// Metatable builder helper
// ════════════════════════════════════════════════════════════════════════════
static void makeMeta(lua_State* L, const char* mt, const luaL_Reg* methods) {
  luaL_newmetatable(L, mt);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, methods, 0);
  lua_pop(L, 1);
}

// ════════════════════════════════════════════════════════════════════════════
// registerAll(L) — call once from LuaEngine::init()
// ════════════════════════════════════════════════════════════════════════════
static void registerAll(lua_State* L) {
  // Build metatables
  makeMeta(L, MT_QFILE,     qfile_methods);
  makeMeta(L, MT_QDIR,      qdir_methods);
  makeMeta(L, MT_QSETTINGS, qsettings_methods);
  makeMeta(L, MT_QTIMER,    qtimer_methods);
  makeMeta(L, MT_QNAM,      qnam_methods);
  makeMeta(L, MT_QTHREAD,   qthread_methods);
  makeMeta(L, MT_QSFX,      qsfx_methods);
  makeMeta(L, MT_QANIM,     qanim_methods);

  // Qt global table
  lua_newtable(L);

  // Constructors
  lua_pushcfunction(L, qfile_new);       lua_setfield(L, -2, "QFile");
  lua_pushcfunction(L, qdir_new);        lua_setfield(L, -2, "QDir");
  lua_pushcfunction(L, qsettings_new);   lua_setfield(L, -2, "QSettings");
  lua_pushcfunction(L, qtimer_new);      lua_setfield(L, -2, "QTimer");
  lua_pushcfunction(L, qnam_new);        lua_setfield(L, -2, "QNetworkAccessManager");
  lua_pushcfunction(L, qthread_new);     lua_setfield(L, -2, "QThread");
  lua_pushcfunction(L, qsfx_new);        lua_setfield(L, -2, "QSoundEffect");
  lua_pushcfunction(L, qanim_new);       lua_setfield(L, -2, "QPropertyAnimation");

  // Static functions
  lua_pushcfunction(L, qsqldb_open);     lua_setfield(L, -2, "openDatabase");
  lua_pushcfunction(L, qsqldb_exec);     lua_setfield(L, -2, "sqlExec");
  lua_pushcfunction(L, qsqldb_close);    lua_setfield(L, -2, "closeDatabase");
  lua_pushcfunction(L, qfile_exists);    lua_setfield(L, -2, "fileExists");
  lua_pushcfunction(L, qfile_remove);    lua_setfield(L, -2, "removeFile");

  // Utilities
  lua_pushcfunction(L, qt_sleep);        lua_setfield(L, -2, "sleep");
  lua_pushcfunction(L, qt_msleep);       lua_setfield(L, -2, "msleep");
  lua_pushcfunction(L, qt_yield);        lua_setfield(L, -2, "yield");
  lua_pushcfunction(L, qt_millis);       lua_setfield(L, -2, "millis");
  lua_pushcfunction(L, qt_micros);       lua_setfield(L, -2, "micros");

  // Easing curve constants (Qt.Linear, Qt.InOutQuad, etc.)
  lua_newtable(L);
  const struct { const char* name; int val; } easings[] = {
    {"Linear",0},{"InQuad",1},{"OutQuad",2},{"InOutQuad",3},
    {"InCubic",4},{"OutCubic",5},{"InOutCubic",6},
    {"InSine",7},{"OutSine",8},{"InOutSine",9},
    {"InExpo",10},{"OutExpo",11},{"InOutExpo",12},
    {"InCirc",13},{"OutCirc",14},{"InOutCirc",15},
    {"InElastic",16},{"OutElastic",17},{"InOutElastic",18},
    {"InBounce",19},{"OutBounce",20},{"InOutBounce",21},
    {"InBack",22},{"OutBack",23},{"InOutBack",24},
    {nullptr,0}
  };
  for(int i=0;easings[i].name;i++){
    lua_pushinteger(L,easings[i].val);
    lua_setfield(L,-2,easings[i].name);
  }
  lua_setfield(L, -2, "Easing");

  lua_setglobal(L, "Qt");
}

} // namespace LuaQt
