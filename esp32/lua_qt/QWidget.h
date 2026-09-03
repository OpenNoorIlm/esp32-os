// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorQt — QWidget.h                                                      ║
// ║  Full Qt6 QWidget API                                                    ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once
#include "QObject.h"
#include "QGeometry.h"
#include "QPainter.h"

namespace NoorQt {

class QLayout;
class QStyle;
class QAction;
class QToolTip;

// ── Event type stubs (ESP32 has no rtti; all are QEvent aliases) ──────────────
using QResizeEvent        = QEvent;
using QMoveEvent          = QEvent;
using QShowEvent          = QEvent;
using QHideEvent          = QEvent;
using QCloseEvent         = QEvent;
using QFocusEvent         = QEvent;
using QEnterEvent         = QEvent;
using QContextMenuEvent   = QEvent;
using QWheelEvent         = QEvent;
using QKeyEvent           = QEvent;
using QTouchEvent         = QEvent;
using QInputMethodEvent   = QEvent;
using QActionEvent        = QEvent;
using QDragEnterEvent     = QEvent;
using QDragMoveEvent      = QEvent;
using QDragLeaveEvent     = QEvent;
using QDropEvent          = QEvent;
using QTabletEvent        = QEvent;

// ── QMouseEvent — simple touch/mouse event ────────────────────────────────────
class QMouseEvent : public QEvent {
public:
  explicit QMouseEvent(QPoint pos, QEvent::Type t=QEvent::MouseButtonPress)
    : QEvent(t), _pos(pos) {}
  QPoint pos()        const { return _pos; }
  QPoint globalPos()  const { return _pos; }
  int    x()          const { return _pos.x(); }
  int    y()          const { return _pos.y(); }
  int    button()     const { return 1; }
  int    buttons()    const { return 1; }
private:
  QPoint _pos;
};

// ── QSizePolicy ───────────────────────────────────────────────────────────────
class QSizePolicy {
public:
  enum Policy { Fixed=0, Minimum=1, Maximum=4, Preferred=5,
                Expanding=7, MinimumExpanding=3, Ignored=13 };
  enum ControlType { DefaultType=1, ButtonBox=2, CheckBox=4, ComboBox=8,
                     Frame=16, GroupBox=32, Label=64, Line=128,
                     LineEdit=256, PushButton=512, RadioButton=1024,
                     Slider=2048, SpinBox=4096, TabWidget=8192, ToolButton=16384 };

  QSizePolicy() : _h(Preferred),_v(Preferred) {}
  QSizePolicy(Policy h,Policy v) : _h(h),_v(v) {}

  Policy horizontalPolicy() const { return _h; }
  Policy verticalPolicy()   const { return _v; }
  void setHorizontalPolicy(Policy p){ _h=p; }
  void setVerticalPolicy(Policy p)  { _v=p; }

  bool hasHeightForWidth()      const { return _hwfh; }
  void setHeightForWidth(bool b)      { _hwfh=b; }
  bool hasWidthForHeight()      const { return false; }

  int horizontalStretch()       const { return _hs; }
  int verticalStretch()         const { return _vs; }
  void setHorizontalStretch(int s)    { _hs=s; }
  void setVerticalStretch(int s)      { _vs=s; }

  void setRetainSizeWhenHidden(bool b){ _retain=b; }
  bool retainSizeWhenHidden()   const { return _retain; }

  bool expandingDirections() const { return (_h==Expanding||_v==Expanding); }

  bool operator==(const QSizePolicy& o) const { return _h==o._h&&_v==o._v; }

private:
  Policy _h,_v;
  int _hs=0,_vs=0;
  bool _hwfh=false,_retain=false;
};

// ── QPalette ──────────────────────────────────────────────────────────────────
class QPalette {
public:
  enum ColorGroup { Active=0, Disabled=1, Inactive=2, NColorGroups=3, Current=Active, All=-1, Normal=Active };
  enum ColorRole  {
    WindowText=0, Button=1, Light=2, Midlight=3, Dark=4, Mid=5,
    Text=6, BrightText=7, ButtonText=8, Base=9, Window=10, Shadow=11,
    Highlight=12, HighlightedText=13, Link=14, LinkVisited=15,
    AlternateBase=16, NoRole=17, ToolTipBase=18, ToolTipText=19,
    PlaceholderText=20, NColorRoles=21
  };

  QPalette() { _initDefaults(); }

  QColor color(ColorGroup g,ColorRole r) const {
    return _colors[g][r];
  }
  QColor color(ColorRole r) const { return _colors[Active][r]; }
  QBrush brush(ColorGroup g,ColorRole r) const { return QBrush(_colors[g][r]); }
  QBrush brush(ColorRole r) const { return QBrush(_colors[Active][r]); }

  void setColor(ColorGroup g,ColorRole r,const QColor& c){ _colors[g][r]=c; }
  void setColor(ColorRole r,const QColor& c){ for(int g=0;g<NColorGroups;g++)_colors[g][r]=c; }
  void setBrush(ColorGroup g,ColorRole r,const QBrush& b){ _colors[g][r]=b.color(); }
  void setBrush(ColorRole r,const QBrush& b){ setColor(r,b.color()); }

  // Convenience
  QColor windowText()       const { return color(WindowText); }
  QColor button()           const { return color(Button); }
  QColor light()            const { return color(Light); }
  QColor dark()             const { return color(Dark); }
  QColor mid()              const { return color(Mid); }
  QColor text()             const { return color(Text); }
  QColor base()             const { return color(Base); }
  QColor window()           const { return color(Window); }
  QColor shadow()           const { return color(Shadow); }
  QColor highlight()        const { return color(Highlight); }
  QColor highlightedText()  const { return color(HighlightedText); }
  QColor link()             const { return color(Link); }
  QColor toolTipBase()      const { return color(ToolTipBase); }
  QColor toolTipText()      const { return color(ToolTipText); }
  QColor placeholderText()  const { return color(PlaceholderText); }

  bool isCopyOf(const QPalette& o) const { return *this==o; }
  bool operator==(const QPalette& o) const {
    for(int g=0;g<NColorGroups;g++) for(int r=0;r<NColorRoles;r++)
      if(_colors[g][r]!=o._colors[g][r])return false;
    return true;
  }

private:
  QColor _colors[NColorGroups][NColorRoles];
  void _initDefaults(){
    // Dark theme defaults
    setColor(Window,    QColor(30,30,30));
    setColor(WindowText,QColor(255,255,255));
    setColor(Base,      QColor(45,45,45));
    setColor(Text,      QColor(255,255,255));
    setColor(Button,    QColor(60,60,60));
    setColor(ButtonText,QColor(255,255,255));
    setColor(Highlight, QColor(0,120,215));
    setColor(HighlightedText,QColor(255,255,255));
    setColor(Shadow,    QColor(0,0,0));
    setColor(Light,     QColor(80,80,80));
    setColor(Dark,      QColor(20,20,20));
    setColor(Mid,       QColor(50,50,50));
    setColor(Link,      QColor(0,200,255));
    setColor(ToolTipBase,QColor(50,50,50));
    setColor(ToolTipText,QColor(220,220,220));
    setColor(PlaceholderText,QColor(120,120,120));
    setColor(AlternateBase,QColor(40,40,40));
    // Disabled group — dimmed
    for(int r=0;r<NColorRoles;r++){
      QColor c=_colors[Active][r];
      _colors[Disabled][r]=QColor(c.red()/2,c.green()/2,c.blue()/2);
    }
  }
};

// ── QWidget ───────────────────────────────────────────────────────────────────
class QWidget : public QObject, public QPaintDevice {
public:
  // ── Constructor ───────────────────────────────────────────────────────────
  explicit QWidget(QWidget* parent=nullptr, Qt::WindowFlags f=Qt::Widget)
    : QObject(parent), _parent(parent), _flags(f) {
    if(parent) { _x=0;_y=0;_w=parent->width();_h=parent->height(); }
    else { _x=0;_y=0;_w=240;_h=320; }
    _sizePolicy=QSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
  }

  virtual ~QWidget() {}

  const char* metaClassName() const override { return "QWidget"; }

  // ── Geometry ──────────────────────────────────────────────────────────────
  int x()      const { return _x; }
  int y()      const { return _y; }
  int width()  const override { return _w; }
  int height() const override { return _h; }
  QPoint pos()     const { return {_x,_y}; }
  QSize  size()    const { return {_w,_h}; }
  QRect  rect()    const { return {0,0,_w,_h}; }
  QRect  geometry()const { return {_x,_y,_w,_h}; }
  QRect  frameGeometry()const{ return geometry(); }
  QPoint mapToGlobal(QPoint p)   const { return {_x+p.x(),_y+p.y()}; }
  QPoint mapFromGlobal(QPoint p) const { return {p.x()-_x,p.y()-_y}; }
  QPoint mapToParent(QPoint p)   const { return {_x+p.x(),_y+p.y()}; }
  QPoint mapFromParent(QPoint p) const { return {p.x()-_x,p.y()-_y}; }
  QPoint mapTo(QWidget* w,QPoint p)   const { return p; } // stub
  QPoint mapFrom(QWidget* w,QPoint p) const { return p; } // stub

  void move(int x,int y)               { _x=x;_y=y; emit_signal("moved",{}); }
  void move(QPoint p)                  { move(p.x(),p.y()); }
  void resize(int w,int h)             { _w=w;_h=h; resizeEvent(nullptr); emit_signal("resized",{}); }
  void resize(QSize s)                 { resize(s.width(),s.height()); }
  void setGeometry(int x,int y,int w,int h){ move(x,y);resize(w,h); }
  void setGeometry(QRect r)            { setGeometry(r.x(),r.y(),r.width(),r.height()); }
  void setFixedSize(int w,int h)       { _minW=_maxW=w;_minH=_maxH=h;resize(w,h); }
  void setFixedSize(QSize s)           { setFixedSize(s.width(),s.height()); }
  void setFixedWidth(int w)            { _minW=_maxW=w;_w=w; }
  void setFixedHeight(int h)           { _minH=_maxH=h;_h=h; }
  void setMinimumSize(int w,int h)     { _minW=w;_minH=h; }
  void setMinimumSize(QSize s)         { setMinimumSize(s.width(),s.height()); }
  void setMaximumSize(int w,int h)     { _maxW=w;_maxH=h; }
  void setMaximumSize(QSize s)         { setMaximumSize(s.width(),s.height()); }
  void setMinimumWidth(int w)          { _minW=w; }
  void setMinimumHeight(int h)         { _minH=h; }
  void setMaximumWidth(int w)          { _maxW=w; }
  void setMaximumHeight(int h)         { _maxH=h; }
  QSize minimumSize()  const { return {_minW,_minH}; }
  QSize maximumSize()  const { return {_maxW,_maxH}; }
  QSize minimumSizeHint() const { return sizeHint(); }
  virtual QSize sizeHint()      const { return {_w,_h}; }
  virtual QSize minimumSizeHint2()const{ return {20,20}; }
  bool  hasHeightForWidth()     const { return false; }
  virtual int heightForWidth(int w)const{ return _h; }
  QSizePolicy sizePolicy() const { return _sizePolicy; }
  void setSizePolicy(QSizePolicy p)    { _sizePolicy=p; }
  void setSizePolicy(QSizePolicy::Policy h,QSizePolicy::Policy v){ _sizePolicy=QSizePolicy(h,v); }
  void updateGeometry()                { emit_signal("geometryChanged",{}); }
  void adjustSize()                    { resize(sizeHint()); }

  // ── Visibility ────────────────────────────────────────────────────────────
  bool isVisible()    const { return _visible; }
  bool isHidden()     const { return !_visible; }
  bool isEnabled()    const { return _enabled; }
  bool isWindow()     const { return !_parent; }
  bool isModal()      const { return _modal; }
  bool isFullScreen() const { return _fullscreen; }
  bool isMaximized()  const { return false; }
  bool isMinimized()  const { return false; }
  bool hasFocus()     const { return _focused; }
  bool underMouse()   const { return false; }
  bool isActiveWindow()const{ return _visible; }

  virtual void show()     { _visible=true; showEvent(nullptr); update(); emit_signal("shown",{}); }
  virtual void hide()     { _visible=false; hideEvent(nullptr); _eraseArea(); emit_signal("hidden",{}); }
  virtual void close()    { hide(); closeEvent(nullptr); emit_signal("closed",{}); emit_signal("destroyed",{}); }
  void setVisible(bool v) { v?show():hide(); }
  void setHidden(bool h)  { setVisible(!h); }
  void setEnabled(bool e) { _enabled=e; update(); emit_signal("enabledChanged",{QVariant(e)}); }
  void setDisabled(bool d){ setEnabled(!d); }
  void raise()            { emit_signal("raised",{}); }
  void lower()            { emit_signal("lowered",{}); }
  void setModal(bool m)   { _modal=m; }
  void showFullScreen()   { _fullscreen=true; show(); }
  void showNormal()       { _fullscreen=false; show(); }

  // ── Style / Palette ───────────────────────────────────────────────────────
  void setStyleSheet(const QString& ss){ _styleSheet=ss; _parseStyleSheet(); update(); }
  QString styleSheet()      const { return _styleSheet; }
  void setPalette(const QPalette& p)   { _palette=p; update(); }
  const QPalette& palette() const      { return _palette; }
  void setFont(const QFont& f)         { _font=f; update(); }
  const QFont& font()       const      { return _font; }
  void setAutoFillBackground(bool b)   { _autoFill=b; }
  bool autoFillBackground()  const     { return _autoFill; }
  void setBackgroundRole(QPalette::ColorRole r){ _bgRole=r; }
  void setForegroundRole(QPalette::ColorRole r){ _fgRole=r; }
  QPalette::ColorRole backgroundRole() const { return _bgRole; }
  QPalette::ColorRole foregroundRole() const { return _fgRole; }

  // ── Layout ────────────────────────────────────────────────────────────────
  void    setLayout(QLayout* l)  { _layout=l; }
  QLayout* layout()        const { return _layout; }
  void    setContentsMargins(int l,int t,int r,int b){ _margins=QMargins(l,t,r,b); }
  void    setContentsMargins(QMargins m){ _margins=m; }
  QMargins contentsMargins()     const { return _margins; }
  QRect   contentsRect()         const { return rect().marginsRemoved(_margins); }

  // ── Focus ─────────────────────────────────────────────────────────────────
  Qt::FocusPolicy focusPolicy()   const { return _focusPolicy; }
  void setFocusPolicy(Qt::FocusPolicy p){ _focusPolicy=p; }
  void setFocus()                       { _focused=true; focusInEvent(nullptr); update(); emit_signal("focusIn",{}); }
  void clearFocus()                     { _focused=false; focusOutEvent(nullptr); update(); emit_signal("focusOut",{}); }
  void setFocus(Qt::FocusReason)        { setFocus(); }
  bool acceptDrops()               const{ return false; }
  void setAcceptDrops(bool)             {}

  // ── Cursor ────────────────────────────────────────────────────────────────
  void setCursor(Qt::CursorShape)  {}
  void unsetCursor()               {}

  // ── Window title / icon ───────────────────────────────────────────────────
  void    setWindowTitle(const QString& t){ _windowTitle=t; emit_signal("windowTitleChanged",{QVariant(t)}); }
  QString windowTitle()   const { return _windowTitle; }
  void    setWindowFlags(Qt::WindowFlags f){ _flags=f; }
  Qt::WindowFlags windowFlags() const { return _flags; }
  void    setWindowModality(Qt::WindowModality m){ (void)m; }
  void    setWindowOpacity(float o){ _opacity=o; }
  float   windowOpacity()   const { return _opacity; }

  // ── Tooltip ───────────────────────────────────────────────────────────────
  void    setToolTip(const QString& t)   { _tooltip=t; }
  QString toolTip()           const { return _tooltip; }
  void    setToolTipDuration(int ms)     { _tooltipDuration=ms; }
  void    setStatusTip(const QString& t) { _statustip=t; }
  void    setWhatsThis(const QString& t) { _whatsthis=t; }

  // ── Tab order ─────────────────────────────────────────────────────────────
  static void setTabOrder(QWidget* first,QWidget* second){ (void)first;(void)second; }

  // ── Update / Repaint ──────────────────────────────────────────────────────
  void update()             { if(_visible) repaint(); }
  void update(QRect r)      { if(_visible) repaint(r); }
  void update(int x,int y,int w,int h){ update({x,y,w,h}); }
  void repaint()            { if(!_visible)return; QPainter p(this); paintEvent(&p); }
  void repaint(QRect r)     { repaint(); }
  void repaint(int x,int y,int w,int h){ repaint({x,y,w,h}); }

  // ── Children ──────────────────────────────────────────────────────────────
  QWidget* parentWidget() const { return _parent; }
  QList<QWidget*> childWidgets() const {
    QList<QWidget*> r;
    for(auto* c:children()) r.push_back(static_cast<QWidget*>(c));
    return r;
  }
  bool isAncestorOf(const QWidget* w) const { return QObject::isAncestorOf(w); }

  // ── Hit testing ───────────────────────────────────────────────────────────
  bool contains(int x,int y) const { return x>=_x&&x<_x+_w&&y>=_y&&y<_y+_h; }
  bool contains(QPoint p)    const { return contains(p.x(),p.y()); }
  QWidget* childAt(int x,int y) const {
    for(auto*c:childWidgets()) if(c->isVisible()&&c->contains(c->mapFromParent({x,y})))return c;
    return nullptr;
  }

  // ── Actions ───────────────────────────────────────────────────────────────
  void addAction(QAction* a)    { _actions.push_back(a); }
  void removeAction(QAction* a) { _actions.erase(std::remove(_actions.begin(),_actions.end(),a),_actions.end()); }
  QList<QAction*>& actions()    { return _actions; }

  // ── Grab ─────────────────────────────────────────────────────────────────
  void grabMouse()   {}
  void releaseMouse(){}
  void grabKeyboard(){}
  void releaseKeyboard(){}
  bool hasMouseTracking()    const { return _mouseTracking; }
  void setMouseTracking(bool b)    { _mouseTracking=b; }

  // ── Scroll ────────────────────────────────────────────────────────────────
  void scroll(int dx,int dy){}

  // ── Event virtuals ────────────────────────────────────────────────────────
  virtual void paintEvent(QPainter* painter) {
    if(_autoFill) {
      painter->fillRect(rect(),_palette.color(_bgRole));
    }
    for(auto*c:childWidgets()) if(c->isVisible()) {
      QPainter cp(c); c->paintEvent(&cp);
    }
  }
  virtual void resizeEvent(QResizeEvent*)   { for(auto*c:childWidgets())c->resize(c->sizeHint()); }
  virtual void moveEvent(QMoveEvent*)       {}
  virtual void showEvent(QShowEvent*)       {}
  virtual void hideEvent(QHideEvent*)       {}
  virtual void closeEvent(QCloseEvent*)     { emit_signal("aboutToClose",{}); }
  virtual void focusInEvent(QFocusEvent*)   {}
  virtual void focusOutEvent(QFocusEvent*)  {}
  virtual void enterEvent(QEnterEvent*)     {}
  virtual void leaveEvent(QEvent*)          {}
  virtual void changeEvent(QEvent*)         {}
  virtual void contextMenuEvent(QContextMenuEvent*){}
  virtual void wheelEvent(QWheelEvent*)     {}
  virtual void keyPressEvent(QKeyEvent*)    {}
  virtual void keyReleaseEvent(QKeyEvent*)  {}
  virtual void mousePressEvent(QMouseEvent*e)   { emit_signal("pressed",{}); }
  virtual void mouseReleaseEvent(QMouseEvent*e) { emit_signal("released",{}); }
  virtual void mouseMoveEvent(QMouseEvent*)     {}
  virtual void mouseDoubleClickEvent(QMouseEvent*){}
  virtual void touchEvent(QTouchEvent*)     {}
  virtual void inputMethodEvent(QInputMethodEvent*){}
  virtual void actionEvent(QActionEvent*)   {}
  virtual void dragEnterEvent(QDragEnterEvent*){}
  virtual void dragMoveEvent(QDragMoveEvent*){}
  virtual void dragLeaveEvent(QDragLeaveEvent*){}
  virtual void dropEvent(QDropEvent*)       {}
  virtual void tabletEvent(QTabletEvent*)   {}

  // ── Touch dispatch (called from App event loop) ───────────────────────────
  virtual bool handleTouch(int tx,int ty) {
    if(!_visible||!_enabled) return false;
    if(!contains(tx,ty)) return false;
    // Try children first
    for(auto*c:childWidgets()) {
      if(c->handleTouch(tx-_x,ty-_y)) return true;
    }
    // Dispatch to self
    QMouseEvent me({tx,ty});
    mousePressEvent(&me);
    emit_signal("clicked",{});
    delay(60);
    mouseReleaseEvent(&me);
    return true;
  }

protected:
  void _eraseArea() { tft.fillRect(_x,_y,_w,_h,0x0000); }

  QWidget*   _parent     = nullptr;
  int _x=0,_y=0,_w=240,_h=28;
  int _minW=0,_minH=0,_maxW=9999,_maxH=9999;
  bool _visible  = false;
  bool _enabled  = true;
  bool _focused  = false;
  bool _modal    = false;
  bool _fullscreen=false;
  bool _autoFill = true;
  bool _mouseTracking=false;
  float _opacity = 1.0f;
  Qt::FocusPolicy _focusPolicy = Qt::ClickFocus;
  Qt::WindowFlags _flags;
  QSizePolicy _sizePolicy;
  QPalette    _palette;
  QFont       _font;
  QMargins    _margins;
  QLayout*    _layout = nullptr;
  QString     _styleSheet;
  QString     _windowTitle;
  QString     _tooltip;
  QString     _statustip;
  QString     _whatsthis;
  QPalette::ColorRole _bgRole = QPalette::Window;
  QPalette::ColorRole _fgRole = QPalette::WindowText;
  QList<QAction*> _actions;
  int _tooltipDuration = 3000;

  // Parsed stylesheet values
  struct ParsedSS {
    QColor bg,fg,border,selBg,selFg,placeholderFg;
    int borderWidth=1,borderRadius=4,padding=4,margin=2;
    bool hasBg=false,hasFg=false,hasBorder=false;
  } _ss;

  void _parseStyleSheet(){
    // Parse QSS-like stylesheet
    QString s=_styleSheet;
    // Extract property: value pairs
    auto getVal=[&](const char* prop)->QString{
      int i=s.indexOf(prop); if(i<0)return "";
      int c=s.indexOf(':',i); if(c<0)return "";
      int semi=s.indexOf(';',c); 
      QString v=semi<0?s.substring(c+1):s.substring(c+1,semi);
      v.trim(); return v;
    };
    QString bg=getVal("background-color");
    if(bg.isEmpty())bg=getVal("background");
    if(!bg.isEmpty()){ _ss.bg=QColor::fromName(bg);_ss.hasBg=true; }
    QString fg=getVal("color");
    if(!fg.isEmpty()){ _ss.fg=QColor::fromName(fg);_ss.hasFg=true; }
    QString bc=getVal("border-color");
    if(!bc.isEmpty()){ _ss.border=QColor::fromName(bc);_ss.hasBorder=true; }
    QString bw=getVal("border-width"); if(!bw.isEmpty())_ss.borderWidth=bw.toInt();
    QString br=getVal("border-radius"); if(!br.isEmpty())_ss.borderRadius=br.toInt();
    QString pd=getVal("padding"); if(!pd.isEmpty())_ss.padding=pd.toInt();
    QString mg=getVal("margin"); if(!mg.isEmpty())_ss.margin=mg.toInt();
  }
};

// ── Stub event classes (for virtual signatures) ───────────────────────────────
struct QResizeEvent  { QSize size, oldSize; };
struct QMoveEvent    { QPoint pos, oldPos; };
struct QShowEvent    {};
struct QHideEvent    {};
struct QCloseEvent   { bool accepted=true; void accept(){accepted=true;} void ignore(){accepted=false;} };
struct QFocusEvent   { Qt::FocusReason reason=Qt::OtherFocusReason; };
struct QEnterEvent   { QPoint pos; };
struct QContextMenuEvent { QPoint pos; };
struct QWheelEvent   { QPoint pos; int delta=0; };
struct QKeyEvent     { Qt::Key key; QString text; bool autoRepeat=false; };
struct QMouseEvent   {
  QPoint pos,globalPos;
  Qt::MouseButton button=Qt::LeftButton;
  Qt::MouseButtons buttons;
  QMouseEvent(){}
  QMouseEvent(QPoint p):pos(p),globalPos(p){}
};
struct QTouchEvent   { struct TouchPoint { QPointF pos; int id; }; QList<TouchPoint> points; };
struct QInputMethodEvent {};
struct QActionEvent  {};
struct QDragEnterEvent{};
struct QDragMoveEvent {};
struct QDragLeaveEvent{};
struct QDropEvent    {};
struct QTabletEvent  {};
struct QWheelEvent2  {};
enum Qt_MouseButton  { LeftButton=0x1, RightButton=0x2, MiddleButton=0x4, NoButton=0 };
enum Qt_MouseButtons { };
enum Qt_FocusReason  { MouseFocusReason, TabFocusReason, BacktabFocusReason, ActiveWindowFocusReason, PopupFocusReason, ShortcutFocusReason, MenuBarFocusReason, OtherFocusReason };
enum Qt_WindowModality{ NonModal, WindowModal, ApplicationModal };
enum Qt_AspectRatioMode{ IgnoreAspectRatio, KeepAspectRatio, KeepAspectRatioByExpanding };

} // namespace NoorQt

using NoorQt::QWidget;
using NoorQt::QPalette;
using NoorQt::QSizePolicy;
