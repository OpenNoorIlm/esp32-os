// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorQt — QWidgets.h                                                     ║
// ║  All concrete Qt6 widgets                                                ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once
#include "QWidget.h"
#include "QLayout.h"
#include "../vkeyboard.h"

#include <TFT_eSPI.h>
extern TFT_eSPI tft;

namespace NoorQt {

// ── QAbstractButton ───────────────────────────────────────────────────────────
class QAbstractButton : public QWidget {
public:
  explicit QAbstractButton(QWidget* parent=nullptr) : QWidget(parent) {}

  void setText(const QString& t)  { _text=t; update(); }
  QString text()           const  { return _text; }
  void setChecked(bool c)         { if(_checkable){_checked=c;emit_signal("toggled",{QVariant(c)});update();} }
  bool isChecked()         const  { return _checked; }
  void setCheckable(bool c)       { _checkable=c; }
  bool isCheckable()       const  { return _checkable; }
  void setAutoRepeat(bool a)      { _autoRepeat=a; }
  bool autoRepeat()        const  { return _autoRepeat; }
  void setAutoRepeatDelay(int ms) { _autoRepeatDelay=ms; }
  void setAutoRepeatInterval(int ms){ _autoRepeatInterval=ms; }
  void setAutoExclusive(bool e)   { _autoExclusive=e; }
  bool autoExclusive()     const  { return _autoExclusive; }
  void setDown(bool d)            { _down=d; update(); }
  bool isDown()            const  { return _down; }
  void toggle()                   { setChecked(!_checked); }
  void click()                    { emit_signal("clicked",{QVariant(_checked)}); }
  void animateClick(int ms=100)   { _down=true;update();delay(ms);_down=false;update();click(); }

  // Signals: clicked(bool), pressed, released, toggled(bool)

  bool handleTouch(int tx,int ty) override {
    if(!isVisible()||!isEnabled()) return false;
    if(!contains(tx,ty)) return false;
    if(_checkable) { _checked=!_checked; emit_signal("toggled",{QVariant(_checked)}); }
    _down=true; update(); delay(70); _down=false; update();
    emit_signal("pressed",{});
    emit_signal("clicked",{QVariant(_checked)});
    emit_signal("released",{});
    return true;
  }

protected:
  QString _text;
  bool _checked=false,_checkable=false,_down=false;
  bool _autoRepeat=false,_autoExclusive=false;
  int  _autoRepeatDelay=300,_autoRepeatInterval=100;
};

// ── QPushButton ───────────────────────────────────────────────────────────────
class QPushButton : public QAbstractButton {
public:
  explicit QPushButton(QWidget* parent=nullptr) : QAbstractButton(parent) { _h=32; }
  explicit QPushButton(const QString& text,QWidget* parent=nullptr) : QAbstractButton(parent) { _text=text;_h=32; }

  const char* metaClassName() const override { return "QPushButton"; }
  QSize sizeHint() const override { return {max(80,(int)_text.length()*8+16),32}; }

  void setFlat(bool f)        { _flat=f; update(); }
  bool isFlat()        const  { return _flat; }
  void setDefault(bool d)     { _default=d; update(); }
  bool isDefault()     const  { return _default; }
  void setAutoDefault(bool a) { _autoDefault=a; }
  bool autoDefault()   const  { return _autoDefault; }
  void setMenu(void* m)       {} // stub

  void paintEvent(QPainter* p) override {
    if(!isVisible()) return;
    QColor bg  = _down ? _ss.hasBg?_ss.bg.darker(130):QColor(0,90,180)
                       : _flat   ? QColor(0,0,0,0)
                       : _ss.hasBg? _ss.bg : (_default?QColor(0,120,215):QColor(60,60,60));
    QColor fg  = _ss.hasFg?_ss.fg:QColor(255,255,255);
    QColor bc  = _ss.hasBorder?_ss.border:(_default?QColor(0,100,200):QColor(80,80,80));
    int r      = _ss.borderRadius;

    if(!_flat){
      // Shadow
      p->setBrush(QBrush(QColor(0,0,0,100)));
      p->setPen(QPen::NoPen);
      p->drawRoundedRect(_x+2,_y+2,_w,_h,r,r);
      // Body
      p->setBrush(QBrush(bg));
      p->setPen(QPen(bc,_default?2:1));
      p->drawRoundedRect(_x,_y,_w,_h,r,r);
    }
    // Text
    p->setPen(QPen(!isEnabled()?QColor(100,100,100):fg));
    p->setFont(_font);
    p->drawText(QRect{_x,_y,_w,_h},Qt::AlignCenter,_text);
  }

private:
  bool _flat=false,_default=false,_autoDefault=false;
};

// ── QLabel ────────────────────────────────────────────────────────────────────
class QLabel : public QWidget {
public:
  explicit QLabel(QWidget* parent=nullptr) : QWidget(parent) { _autoFill=false; }
  explicit QLabel(const QString& text,QWidget* parent=nullptr) : QWidget(parent),_text(text) { _autoFill=false; }

  const char* metaClassName() const override { return "QLabel"; }
  QSize sizeHint() const override { return {max(20,(int)_text.length()*6*_font.tftSize()),12*_font.tftSize()}; }

  void setText(const QString& t)       { _text=t; update(); emit_signal("textChanged",{QVariant(t)}); }
  QString text()                 const { return _text; }
  void setNum(int n)                   { setText(String(n)); }
  void setNum(double d)                { setText(String(d)); }
  void setAlignment(Qt::Alignment a)   { _align=a; update(); }
  Qt::Alignment alignment()      const { return _align; }
  void setWordWrap(bool w)             { _wordWrap=w; update(); }
  bool wordWrap()                const { return _wordWrap; }
  void setIndent(int i)                { _indent=i; update(); }
  int  indent()                  const { return _indent; }
  void setMargin(int m)                { _margin=m; update(); }
  int  margin()                  const { return _margin; }
  void setTextFormat(Qt::TextFormat f) { _textFormat=f; update(); }
  Qt::TextFormat textFormat()    const { return _textFormat; }
  void setScaledContents(bool s)       { _scaled=s; update(); }
  void setBuddy(QWidget* w)            { _buddy=w; }
  QWidget* buddy()               const { return _buddy; }
  void setOpenExternalLinks(bool o)    { _openLinks=o; }
  void clear()                         { setText(""); }

  void paintEvent(QPainter* p) override {
    if(!isVisible()) return;
    if(_autoFill) p->fillRect(rect(),_palette.color(_bgRole));
    p->setPen(QPen(_ss.hasFg?_ss.fg:_palette.color(QPalette::WindowText)));
    p->setFont(_font);
    int flags=_align;
    if(_wordWrap) {
      // Simple word wrap
      int y=_y+_margin;
      QString word,line;
      int maxChars=(_w-_margin*2)/(6*_font.tftSize());
      for(int i=0;i<=(int)_text.length();i++){
        char c=i<(int)_text.length()?_text[i]:' ';
        if(c==' '||c=='\n'){
          if((int)(line.length()+word.length())>maxChars){
            p->drawText(_x+_margin,y,_w-_margin*2,12*_font.tftSize(),flags,line);
            y+=12*_font.tftSize(); line="";
          }
          line+=word+" "; word=""; if(c=='\n'){p->drawText(_x+_margin,y,_w-_margin*2,12*_font.tftSize(),flags,line);y+=12*_font.tftSize();line="";}
        } else word+=c;
      }
      if(!line.isEmpty()) p->drawText(_x+_margin,y,_w-_margin*2,12*_font.tftSize(),flags,line);
    } else {
      p->drawText(QRect{_x+_margin,_y+_margin,_w-_margin*2,_h-_margin*2},flags,_text);
    }
  }

private:
  QString _text;
  Qt::Alignment _align=Qt::AlignLeft|Qt::AlignVCenter;
  bool _wordWrap=false,_scaled=false,_openLinks=false;
  int _indent=0,_margin=2;
  Qt::TextFormat _textFormat=Qt::AutoText;
  QWidget* _buddy=nullptr;
};

// ── QLineEdit ─────────────────────────────────────────────────────────────────
class QLineEdit : public QWidget {
public:
  enum EchoMode { Normal, NoEcho, Password, PasswordEchoOnEdit };
  enum ActionPosition { LeadingPosition, TrailingPosition };

  explicit QLineEdit(QWidget* parent=nullptr) : QWidget(parent) { _h=28; }
  explicit QLineEdit(const QString& contents,QWidget* parent=nullptr) : QWidget(parent),_text(contents) { _h=28; }

  const char* metaClassName() const override { return "QLineEdit"; }
  QSize sizeHint() const override { return {_w,28}; }

  void setText(const QString& t)         { _text=t; update(); emit_signal("textChanged",{QVariant(t)}); }
  QString text()                   const { return _text; }
  QString displayText()            const { return _echoMode==Password?QString(_text.length(),'*'):_text; }
  void setPlaceholderText(const QString& t){ _placeholder=t; update(); }
  QString placeholderText()        const { return _placeholder; }
  void setEchoMode(EchoMode m)           { _echoMode=m; update(); }
  EchoMode echoMode()              const { return _echoMode; }
  void setMaxLength(int l)               { _maxLength=l; }
  int  maxLength()                 const { return _maxLength; }
  void setReadOnly(bool r)               { _readOnly=r; }
  bool isReadOnly()                const { return _readOnly; }
  void setAlignment(Qt::Alignment a)     { _align=a; update(); }
  Qt::Alignment alignment()        const { return _align; }
  void setClearButtonEnabled(bool b)     { _clearBtn=b; }
  bool isClearButtonEnabled()      const { return _clearBtn; }
  void setInputMask(const QString& m)    { _inputMask=m; }
  QString inputMask()              const { return _inputMask; }
  void setValidator(void*)               {} // stub
  bool hasAcceptableInput()        const { return true; }
  void setFrame(bool f)                  { _frame=f; update(); }
  bool hasFrame()                  const { return _frame; }

  int  cursorPosition()            const { return _cursor; }
  void setCursorPosition(int p)          { _cursor=constrain(p,0,(int)_text.length()); }
  void home(bool mark)                   { _cursor=0; }
  void end(bool mark)                    { _cursor=_text.length(); }
  void backspace()                       { if(_cursor>0){_text.remove(_cursor-1,1);_cursor--;update();emit_signal("textChanged",{QVariant(_text)});} }
  void del2()                            { if(_cursor<(int)_text.length()){_text.remove(_cursor,1);update();emit_signal("textChanged",{QVariant(_text)});} }
  void insert(const QString& t)          { _text=_text.substring(0,_cursor)+t+_text.substring(_cursor);_cursor+=t.length();if(_maxLength>0&&(int)_text.length()>_maxLength)_text=_text.substring(0,_maxLength);update();emit_signal("textChanged",{QVariant(_text)});emit_signal("textEdited",{QVariant(_text)}); }
  void clear()                           { _text="";_cursor=0;update();emit_signal("textChanged",{QVariant(_text)}); }
  void selectAll()                       { _selStart=0;_selEnd=_text.length(); }
  void deselect()                        { _selStart=_selEnd=_cursor; }
  bool hasSelectedText()           const { return _selStart!=_selEnd; }
  QString selectedText()           const { return _text.substring(min(_selStart,_selEnd),max(_selStart,_selEnd)); }
  void setSelection(int start,int len)   { _selStart=start;_selEnd=start+len; }
  int  selectionStart()            const { return min(_selStart,_selEnd); }
  int  selectionEnd()              const { return max(_selStart,_selEnd); }
  int  selectionLength()           const { return abs(_selEnd-_selStart); }
  void cut()                             { /* stub */ }
  void copy()                            { /* stub */ }
  void paste()                           { /* stub */ }
  void undo()                            { if(!_undoStack.empty()){_text=_undoStack.back();_undoStack.pop_back();update();} }
  void redo()                            { /* stub */ }
  bool isUndoAvailable()           const { return !_undoStack.empty(); }
  bool isRedoAvailable()           const { return false; }

  void setCompleter(void*)               {} // stub
  void addAction(void*,ActionPosition)   {} // stub

  // Signals: textChanged(str), textEdited(str), returnPressed, editingFinished, selectionChanged, cursorPositionChanged(int,int), inputRejected

  void paintEvent(QPainter* p) override {
    if(!isVisible()) return;
    QColor bg=_ss.hasBg?_ss.bg:QColor(45,45,45);
    QColor fg=_text.isEmpty()?QColor(100,100,100):(_ss.hasFg?_ss.fg:QColor(220,220,220));
    QColor bc=_focused?QColor(0,120,215):(_ss.hasBorder?_ss.border:QColor(70,70,70));
    int r=_ss.borderRadius;
    p->setBrush(QBrush(bg));
    p->setPen(QPen(bc,_focused?2:1));
    if(_frame) p->drawRoundedRect(_x,_y,_w,_h,r,r);
    else p->fillRect(QRect{_x,_y,_w,_h},bg);
    QString display=displayText().isEmpty()?_placeholder:displayText();
    p->setPen(QPen(fg));
    p->setFont(_font);
    int pad=_ss.padding;
    // Truncate from right if too long
    int maxC=(_w-pad*2)/(6*_font.tftSize());
    if((int)display.length()>maxC) display=display.substring(display.length()-maxC);
    p->drawText(_x+pad,_y,_w-pad*2,_h,Qt::AlignLeft|Qt::AlignVCenter,display);
    // Cursor
    if(_focused){
      int cx=_x+pad+min((int)displayText().length(),(int)display.length())*6*_font.tftSize();
      tft.fillRect(cx,_y+4,2,_h-8,QColor(0,120,215).toRgb565());
    }
  }

  bool handleTouch(int tx,int ty) override {
    if(!isVisible()||!isEnabled()||_readOnly) return false;
    if(!contains(tx,ty)) return false;
    setFocus();
    _undoStack.push_back(_text);
    QString newText=VKeyboard::open(_placeholder,_text);
    if(_maxLength>0&&(int)newText.length()>_maxLength) newText=newText.substring(0,_maxLength);
    if(newText!=_text){ _text=newText; emit_signal("textEdited",{QVariant(_text)}); emit_signal("textChanged",{QVariant(_text)}); }
    emit_signal("editingFinished",{});
    emit_signal("returnPressed",{});
    clearFocus(); update();
    return true;
  }

private:
  QString _text,_placeholder,_inputMask;
  EchoMode _echoMode=Normal;
  int _maxLength=32767,_cursor=0,_selStart=0,_selEnd=0;
  bool _readOnly=false,_clearBtn=false,_frame=true;
  Qt::Alignment _align=Qt::AlignLeft|Qt::AlignVCenter;
  QList<QString> _undoStack;
};

// ── QTextEdit ─────────────────────────────────────────────────────────────────
class QTextEdit : public QWidget {
public:
  explicit QTextEdit(QWidget* parent=nullptr) : QWidget(parent) {}
  explicit QTextEdit(const QString& text,QWidget* parent=nullptr) : QWidget(parent),_text(text) {}

  const char* metaClassName() const override { return "QTextEdit"; }

  void setText(const QString& t)       { _text=t; update(); emit_signal("textChanged",{}); }
  void setPlainText(const QString& t)  { setText(t); }
  void setHtml(const QString& h)       { _text=h; update(); } // stub — display as plain
  QString toPlainText()          const { return _text; }
  QString toHtml()               const { return "<p>"+_text+"</p>"; }
  void append(const QString& t)        { _text+="\n"+t; update(); emit_signal("textChanged",{}); }
  void insertPlainText(const QString& t){ _text+=t; update(); emit_signal("textChanged",{}); }
  void clear()                         { _text=""; update(); emit_signal("textChanged",{}); }
  bool isEmpty()                 const { return _text.isEmpty(); }
  void setReadOnly(bool r)             { _readOnly=r; }
  bool isReadOnly()              const { return _readOnly; }
  void setPlaceholderText(const QString& t){ _placeholder=t; update(); }
  QString placeholderText()      const { return _placeholder; }
  void setLineWrapMode(int m)          { _lineWrap=m; update(); }
  void setWordWrapMode(int m)          { _wordWrap=m; update(); }
  void setTabStopDistance(float d)     { _tabStop=d; }
  void setAcceptRichText(bool b)       { _richText=b; }
  void setOverwriteMode(bool b)        { _overwrite=b; }
  void setUndoRedoEnabled(bool b)      { _undoRedo=b; }
  bool isUndoRedoEnabled()       const { return _undoRedo; }
  void undo()                          {}
  void redo()                          {}
  void cut()                           {}
  void copy()                          {}
  void paste()                         {}
  void selectAll()                     {}
  void scrollToAnchor(const QString&)  {}
  void zoomIn(int r=1)                 { _zoom+=r; update(); }
  void zoomOut(int r=1)                { _zoom-=r; update(); }

  void paintEvent(QPainter* p) override {
    if(!isVisible()) return;
    QColor bg=_ss.hasBg?_ss.bg:QColor(35,35,35);
    p->fillRect(QRect{_x,_y,_w,_h},bg);
    p->setPen(QPen(_ss.hasBorder?_ss.border:QColor(70,70,70)));
    p->drawRect(QRect{_x,_y,_w,_h});
    QString display=_text.isEmpty()?_placeholder:_text;
    p->setPen(QPen(_text.isEmpty()?QColor(100,100,100):(_ss.hasFg?_ss.fg:QColor(220,220,220))));
    p->setFont(_font);
    // Render lines
    int y=_y+4-_scrollY,pad=4,fs=_font.tftSize();
    int maxC=(_w-pad*2)/(6*fs);
    QString line;
    for(int i=0;i<=(int)display.length();i++){
      char c=i<(int)display.length()?display[i]:'\n';
      if(c=='\n'||(int)line.length()>=maxC){
        if(y>=_y&&y<_y+_h) p->drawText(_x+pad,y,_w-pad*2,12*fs,Qt::AlignLeft,line);
        y+=12*fs; line=""; if(c!='\n')line+=c;
      } else line+=c;
    }
    // Scrollbar
    if(_contentH>_h){
      int sbH=_h*_h/_contentH;
      int sbY=_y+_scrollY*_h/_contentH;
      tft.fillRect(_x+_w-4,_y,4,_h,QColor(50,50,50).toRgb565());
      tft.fillRect(_x+_w-4,sbY,4,sbH,QColor(100,100,100).toRgb565());
    }
  }

  bool handleTouch(int tx,int ty) override {
    if(!isVisible()||!isEnabled()||_readOnly) return false;
    if(!contains(tx,ty)) return false;
    QString newText=VKeyboard::open(_placeholder,_text);
    if(newText!=_text){ _text=newText; emit_signal("textChanged",{}); }
    update(); return true;
  }

private:
  QString _text,_placeholder;
  bool _readOnly=false,_richText=true,_overwrite=false,_undoRedo=true;
  int _lineWrap=1,_wordWrap=0,_zoom=0;
  float _tabStop=80;
  int _scrollY=0,_contentH=0;
};

// ── QPlainTextEdit ────────────────────────────────────────────────────────────
class QPlainTextEdit : public QTextEdit {
public:
  explicit QPlainTextEdit(QWidget* parent=nullptr) : QTextEdit(parent) {}
  explicit QPlainTextEdit(const QString& t,QWidget* parent=nullptr) : QTextEdit(t,parent) {}
  const char* metaClassName() const override { return "QPlainTextEdit"; }
  void setMaximumBlockCount(int n){ _maxBlocks=n; }
  int  maximumBlockCount() const  { return _maxBlocks; }
  void setTabChangesFocus(bool b) { _tabFocus=b; }
  bool tabChangesFocus()   const  { return _tabFocus; }
  void appendPlainText(const QString& t){ append(t); }
  void insertPlainText(const QString& t){ QTextEdit::insertPlainText(t); }
  void setDocumentTitle(const QString& t){ _docTitle=t; }
  QString documentTitle() const   { return _docTitle; }
private:
  int _maxBlocks=0;
  bool _tabFocus=false;
  QString _docTitle;
};

// ── QCheckBox ─────────────────────────────────────────────────────────────────
class QCheckBox : public QAbstractButton {
public:
  enum CheckState { Unchecked=0, PartiallyChecked=1, Checked=2 };

  explicit QCheckBox(QWidget* parent=nullptr) : QAbstractButton(parent) { _checkable=true;_h=24; }
  explicit QCheckBox(const QString& text,QWidget* parent=nullptr) : QAbstractButton(parent){ _text=text;_checkable=true;_h=24; }

  const char* metaClassName() const override { return "QCheckBox"; }
  QSize sizeHint() const override { return {max(60,(int)_text.length()*7+24),24}; }

  void setTristate(bool t)          { _tristate=t; }
  bool isTristate()          const  { return _tristate; }
  void setCheckState(CheckState s)  { _state=s; _checked=(s==Checked); update(); emit_signal("stateChanged",{QVariant((int)s)}); }
  CheckState checkState()    const  { return _state; }
  // Signals: stateChanged(int)

  void paintEvent(QPainter* p) override {
    if(!isVisible()) return;
    QColor bg=_down?QColor(60,60,60):QColor(35,35,35);
    QColor bc=_focused?QColor(0,120,215):QColor(80,80,80);
    QColor ac=QColor(0,120,215);
    p->setBrush(QBrush(bg)); p->setPen(QPen(bc));
    p->drawRoundedRect(_x,_y+(_h-16)/2,16,16,3,3);
    if(_state==Checked){
      p->setBrush(QBrush(ac)); p->setPen(QPen::NoPen);
      p->drawRoundedRect(_x+2,_y+(_h-16)/2+2,12,12,2,2);
      p->setPen(QPen(QColor(255,255,255),2));
      p->drawLine(_x+4,_y+(_h-16)/2+8,_x+7,_y+(_h-16)/2+11);
      p->drawLine(_x+7,_y+(_h-16)/2+11,_x+12,_y+(_h-16)/2+5);
    } else if(_state==PartiallyChecked){
      p->setBrush(QBrush(ac.darker(150))); p->setPen(QPen::NoPen);
      p->drawRoundedRect(_x+4,_y+(_h-16)/2+4,8,8,2,2);
    }
    p->setPen(QPen(_ss.hasFg?_ss.fg:QColor(220,220,220)));
    p->setFont(_font);
    p->drawText(_x+22,_y,_w-22,_h,Qt::AlignLeft|Qt::AlignVCenter,_text);
  }

private:
  CheckState _state=Unchecked;
  bool _tristate=false;
};

// ── QRadioButton ──────────────────────────────────────────────────────────────
class QRadioButton : public QAbstractButton {
public:
  explicit QRadioButton(QWidget* parent=nullptr) : QAbstractButton(parent){ _checkable=true;_autoExclusive=true;_h=24; }
  explicit QRadioButton(const QString& text,QWidget* parent=nullptr) : QAbstractButton(parent){ _text=text;_checkable=true;_autoExclusive=true;_h=24; }

  const char* metaClassName() const override { return "QRadioButton"; }
  QSize sizeHint() const override { return {max(60,(int)_text.length()*7+24),24}; }

  void paintEvent(QPainter* p) override {
    if(!isVisible()) return;
    QColor bg=QColor(35,35,35);
    QColor bc=_focused?QColor(0,120,215):QColor(80,80,80);
    p->setBrush(QBrush(bg)); p->setPen(QPen(bc));
    p->drawEllipse(QPoint(_x+8,_y+_h/2),8,8);
    if(_checked){
      p->setBrush(QBrush(QColor(0,120,215))); p->setPen(QPen::NoPen);
      p->drawEllipse(QPoint(_x+8,_y+_h/2),4,4);
    }
    p->setPen(QPen(_ss.hasFg?_ss.fg:QColor(220,220,220)));
    p->setFont(_font);
    p->drawText(_x+22,_y,_w-22,_h,Qt::AlignLeft|Qt::AlignVCenter,_text);
  }
};

// ── QAbstractSlider ───────────────────────────────────────────────────────────
class QAbstractSlider : public QWidget {
public:
  explicit QAbstractSlider(QWidget* parent=nullptr) : QWidget(parent) {}

  void setValue(int v)            { int old=_value; _value=constrain(v,_min,_max); if(_value!=old){update();emit_signal("valueChanged",{QVariant(_value)});} }
  void setMinimum(int m)          { _min=m; if(_value<_min)setValue(_min); }
  void setMaximum(int m)          { _max=m; if(_value>_max)setValue(_max); }
  void setRange(int mn,int mx)    { _min=mn;_max=mx;setValue(constrain(_value,_min,_max)); }
  void setSingleStep(int s)       { _singleStep=s; }
  void setPageStep(int s)         { _pageStep=s; }
  void setTracking(bool t)        { _tracking=t; }
  void setOrientation(Qt::Orientation o){ _orientation=o; update(); }
  void setInvertedAppearance(bool i){ _inverted=i; update(); }
  void setInvertedControls(bool i){ _invertedControls=i; }

  int  value()           const    { return _value; }
  int  minimum()         const    { return _min; }
  int  maximum()         const    { return _max; }
  int  singleStep()      const    { return _singleStep; }
  int  pageStep()        const    { return _pageStep; }
  bool hasTracking()     const    { return _tracking; }
  Qt::Orientation orientation()const{ return _orientation; }
  bool invertedAppearance()const  { return _inverted; }

  void triggerAction(int action)  { /* stub */ }

  // Signals: valueChanged(int), sliderPressed, sliderMoved(int), sliderReleased, rangeChanged(int,int), actionTriggered(int)

protected:
  int _value=0,_min=0,_max=100,_singleStep=1,_pageStep=10;
  bool _tracking=true,_inverted=false,_invertedControls=false;
  Qt::Orientation _orientation=Qt::Horizontal;
};

// ── QSlider ───────────────────────────────────────────────────────────────────
class QSlider : public QAbstractSlider {
public:
  enum TickPosition { NoTicks=0, TicksAbove=1, TicksLeft=1, TicksBelow=2, TicksRight=2, TicksBothSides=3 };

  explicit QSlider(QWidget* parent=nullptr) : QAbstractSlider(parent){ _h=28; }
  explicit QSlider(Qt::Orientation o,QWidget* parent=nullptr) : QAbstractSlider(parent){ _orientation=o;_h=o==Qt::Horizontal?28:120; }

  const char* metaClassName() const override { return "QSlider"; }
  QSize sizeHint() const override { return _orientation==Qt::Horizontal?QSize{_w,28}:QSize{28,_h}; }

  void setTickPosition(TickPosition t){ _ticks=t; update(); }
  void setTickInterval(int i)         { _tickInterval=i; update(); }
  TickPosition tickPosition()  const  { return _ticks; }
  int  tickInterval()          const  { return _tickInterval; }

  void paintEvent(QPainter* p) override {
    if(!isVisible()) return;
    bool h=_orientation==Qt::Horizontal;
    QColor trackBg=QColor(60,60,60),trackFg=QColor(0,120,215),knobC=QColor(255,255,255);
    float t=(float)(_value-_min)/max(1,_max-_min);
    if(h){
      int ty=_y+_h/2; int filled=(int)(t*_w);
      p->setBrush(QBrush(trackBg)); p->setPen(QPen::NoPen);
      p->drawRoundedRect(_x,ty-3,_w,6,3,3);
      if(filled>0){ p->setBrush(QBrush(trackFg)); p->drawRoundedRect(_x,ty-3,filled,6,3,3); }
      int kx=_x+filled;
      tft.fillCircle(kx+1,ty+1,9,QColor(0,0,0,80).toRgb565());
      p->setBrush(QBrush(knobC)); p->setPen(QPen(trackFg,2)); p->drawEllipse(QPoint(kx,ty),8,8);
    } else {
      int tx=_x+_w/2; int filled=(int)((1-t)*_h);
      p->setBrush(QBrush(trackBg)); p->setPen(QPen::NoPen);
      p->drawRoundedRect(tx-3,_y,6,_h,3,3);
      if(filled<_h){ p->setBrush(QBrush(trackFg)); p->drawRoundedRect(tx-3,_y+filled,6,_h-filled,3,3); }
      int ky=_y+filled;
      p->setBrush(QBrush(knobC)); p->setPen(QPen(trackFg,2)); p->drawEllipse(QPoint(tx,ky),8,8);
    }
    if(_ticks!=NoTicks&&_tickInterval>0){
      p->setPen(QPen(QColor(100,100,100)));
      for(int v=_min;v<=_max;v+=_tickInterval){
        float tv=(float)(v-_min)/max(1,_max-_min);
        if(h){ int x=_x+(int)(tv*_w); p->drawLine(x,_y+_h/2+12,x,_y+_h/2+16); }
        else { int y=_y+(int)((1-tv)*_h); p->drawLine(_x+_w/2+12,y,_x+_w/2+16,y); }
      }
    }
  }

  bool handleTouch(int tx,int ty) override {
    if(!isVisible()||!isEnabled()) return false;
    if(!contains(tx,ty)) return false;
    bool h=_orientation==Qt::Horizontal;
    float t=h?(float)(tx-_x)/_w:(float)(ty-_y)/_h;
    if(!h)t=1-t;
    t=constrain(t,0.0f,1.0f);
    setValue(_min+(int)(t*(_max-_min)));
    emit_signal("sliderMoved",{QVariant(_value)});
    return true;
  }

private:
  TickPosition _ticks=NoTicks;
  int _tickInterval=0;
};

// ── QSpinBox ──────────────────────────────────────────────────────────────────
class QSpinBox : public QWidget {
public:
  explicit QSpinBox(QWidget* parent=nullptr) : QWidget(parent){ _h=28; }

  const char* metaClassName() const override { return "QSpinBox"; }
  QSize sizeHint() const override { return {80,28}; }

  void setValue(int v)              { int old=_value;_value=constrain(v,_min,_max);if(_value!=old){update();emit_signal("valueChanged",{QVariant(_value)});} }
  void setMinimum(int m)            { _min=m;if(_value<_min)setValue(_min); }
  void setMaximum(int m)            { _max=m;if(_value>_max)setValue(_max); }
  void setRange(int mn,int mx)      { _min=mn;_max=mx;setValue(constrain(_value,_min,_max)); }
  void setSingleStep(int s)         { _step=s; }
  void setPrefix(const QString& p)  { _prefix=p;update(); }
  void setSuffix(const QString& s)  { _suffix=s;update(); }
  void setSpecialValueText(const QString& t){ _specialText=t;update(); }
  void setReadOnly(bool r)          { _readOnly=r; }
  void setWrapping(bool w)          { _wrap=w; }
  void setAccelerated(bool a)       { _accel=a; }
  void setDisplayIntegerBase(int b) { _base=b; }
  void setKeyboardTracking(bool k)  { _kbTrack=k; }
  void stepUp()                     { setValue(_value+_step); }
  void stepDown()                   { setValue(_value-_step); }
  void selectAll()                  {}
  void clear()                      { setValue(_min); }

  int     value()    const { return _value; }
  int     minimum()  const { return _min; }
  int     maximum()  const { return _max; }
  int     singleStep()const{ return _step; }
  QString prefix()   const { return _prefix; }
  QString suffix()   const { return _suffix; }
  QString text()     const { return _prefix+(_value==_min&&!_specialText.isEmpty()?_specialText:String(_value))+_suffix; }
  bool    isReadOnly()const{ return _readOnly; }
  bool    wrapping() const { return _wrap; }

  void paintEvent(QPainter* p) override {
    if(!isVisible()) return;
    QColor bg=_ss.hasBg?_ss.bg:QColor(45,45,45);
    QColor fg=_ss.hasFg?_ss.fg:QColor(220,220,220);
    QColor bc=_focused?QColor(0,120,215):(_ss.hasBorder?_ss.border:QColor(70,70,70));
    p->setBrush(QBrush(bg)); p->setPen(QPen(bc)); p->drawRoundedRect(_x,_y,_w,_h,_ss.borderRadius,_ss.borderRadius);
    // Minus btn
    p->setBrush(QBrush(QColor(60,60,60))); p->setPen(QPen::NoPen);
    p->drawRoundedRect(_x+2,_y+2,22,_h-4,4,4);
    p->setPen(QPen(QColor(200,50,50),2)); p->setFont(QFont("default",12,QFont::Bold));
    p->drawText(_x+2,_y+2,22,_h-4,Qt::AlignCenter,"-");
    // Plus btn
    p->setBrush(QBrush(QColor(60,60,60))); p->setPen(QPen::NoPen);
    p->drawRoundedRect(_x+_w-24,_y+2,22,_h-4,4,4);
    p->setPen(QPen(QColor(50,180,50),2));
    p->drawText(_x+_w-24,_y+2,22,_h-4,Qt::AlignCenter,"+");
    // Value
    p->setPen(QPen(fg)); p->setFont(_font);
    p->drawText(_x+26,_y,_w-52,_h,Qt::AlignCenter,text());
  }

  bool handleTouch(int tx,int ty) override {
    if(!isVisible()||!isEnabled()||_readOnly) return false;
    if(!contains(tx,ty)) return false;
    if(tx<_x+24){ stepDown(); return true; }
    if(tx>_x+_w-24){ stepUp(); return true; }
    // Tap center = open keyboard input
    QString input=VKeyboard::open("Enter value:",String(_value));
    setValue(input.toInt());
    return true;
  }

  // Signals: valueChanged(int), valueChanged(str), textChanged(str)
private:
  int _value=0,_min=0,_max=99,_step=1,_base=10;
  QString _prefix,_suffix,_specialText;
  bool _readOnly=false,_wrap=false,_accel=false,_kbTrack=true;
};

// ── QDoubleSpinBox ────────────────────────────────────────────────────────────
class QDoubleSpinBox : public QWidget {
public:
  explicit QDoubleSpinBox(QWidget* parent=nullptr) : QWidget(parent){ _h=28; }
  const char* metaClassName() const override { return "QDoubleSpinBox"; }
  void setValue(double v)   { _value=constrain((float)v,(float)_min,(float)_max);update();emit_signal("valueChanged",{QVariant((double)_value)}); }
  void setMinimum(double m) { _min=m; }
  void setMaximum(double m) { _max=m; }
  void setRange(double mn,double mx){ _min=mn;_max=mx; }
  void setSingleStep(double s){ _step=s; }
  void setDecimals(int d)   { _decimals=d; update(); }
  void setPrefix(const QString& p){ _prefix=p; update(); }
  void setSuffix(const QString& s){ _suffix=s; update(); }
  double value()     const  { return _value; }
  double minimum()   const  { return _min; }
  double maximum()   const  { return _max; }
  double singleStep()const  { return _step; }
  int    decimals()  const  { return _decimals; }
  QString text()     const  { return _prefix+String(_value,_decimals)+_suffix; }
  void stepUp()             { setValue(_value+_step); }
  void stepDown()           { setValue(_value-_step); }
  void paintEvent(QPainter* p) override {
    if(!isVisible())return;
    p->fillRect(QRect{_x,_y,_w,_h},_ss.hasBg?_ss.bg:QColor(45,45,45));
    p->setPen(QPen(_ss.hasFg?_ss.fg:QColor(220,220,220))); p->setFont(_font);
    p->drawText(_x,_y,_w,_h,Qt::AlignCenter,text());
  }
  bool handleTouch(int tx,int ty) override {
    if(!isVisible()||!isEnabled())return false;
    if(!contains(tx,ty))return false;
    if(tx<_x+_w/3)stepDown();
    else if(tx>_x*2/_w)stepUp();
    return true;
  }
private:
  double _value=0,_min=0,_max=99,_step=1;
  int _decimals=2;
  QString _prefix,_suffix;
};

// ── QProgressBar ──────────────────────────────────────────────────────────────
class QProgressBar : public QWidget {
public:
  explicit QProgressBar(QWidget* parent=nullptr) : QWidget(parent){ _h=22; }
  const char* metaClassName() const override { return "QProgressBar"; }
  QSize sizeHint() const override { return {_w,22}; }

  void setValue(int v)               { int old=_value;_value=constrain(v,_min,_max);if(_value!=old){update();emit_signal("valueChanged",{QVariant(_value)});} }
  void setMinimum(int m)             { _min=m;update(); }
  void setMaximum(int m)             { _max=m;update(); }
  void setRange(int mn,int mx)       { _min=mn;_max=mx;update(); }
  void setOrientation(Qt::Orientation o){ _orientation=o;update(); }
  void setInvertedAppearance(bool i) { _inverted=i;update(); }
  void setTextVisible(bool t)        { _textVisible=t;update(); }
  void setFormat(const QString& f)   { _format=f;update(); }
  void setAlignment(Qt::Alignment a) { _align=a;update(); }
  void reset()                       { setValue(_min); }

  int  value()            const { return _value; }
  int  minimum()          const { return _min; }
  int  maximum()          const { return _max; }
  bool isTextVisible()    const { return _textVisible; }
  QString format()        const { return _format; }
  QString text()          const {
    if(_max==_min) return ""; // indeterminate
    QString f=_format; int pct=(int)((float)(_value-_min)/(_max-_min)*100);
    f.replace("%p",String(pct)); f.replace("%v",String(_value)); f.replace("%m",String(_max));
    return f;
  }
  Qt::Orientation orientation()const{ return _orientation; }

  void paintEvent(QPainter* p) override {
    if(!isVisible())return;
    bool h=_orientation==Qt::Horizontal;
    QColor bg=_ss.hasBg?_ss.bg:QColor(50,50,50);
    QColor fg=_ss.hasFg?_ss.fg:QColor(0,120,215);
    QColor textC=QColor(220,220,220);
    int r=_ss.borderRadius;
    p->setBrush(QBrush(bg)); p->setPen(QPen(_ss.hasBorder?_ss.border:QColor(70,70,70)));
    p->drawRoundedRect(_x,_y,_w,_h,r,r);
    if(_max>_min){
      float t=(float)(_value-_min)/(_max-_min);
      if(_inverted)t=1-t;
      int filled=h?(int)(t*_w):(int)(t*_h);
      if(filled>0){
        p->setBrush(QBrush(fg)); p->setPen(QPen::NoPen);
        if(h) p->drawRoundedRect(_x,_y,filled,_h,r,r);
        else  p->drawRoundedRect(_x,_y+_h-filled,_w,filled,r,r);
      }
    }
    if(_textVisible&&!text().isEmpty()){
      p->setPen(QPen(textC)); p->setFont(_font);
      p->drawText(_x,_y,_w,_h,Qt::AlignCenter,text());
    }
  }

  // Signals: valueChanged(int)
private:
  int _value=0,_min=0,_max=100;
  bool _inverted=false,_textVisible=true;
  Qt::Orientation _orientation=Qt::Horizontal;
  Qt::Alignment _align=Qt::AlignCenter;
  QString _format="%p%";
};

// ── QComboBox ─────────────────────────────────────────────────────────────────
class QComboBox : public QWidget {
public:
  explicit QComboBox(QWidget* parent=nullptr) : QWidget(parent){ _h=28; }
  const char* metaClassName() const override { return "QComboBox"; }
  QSize sizeHint() const override { return {_w,28}; }

  void addItem(const QString& text,QVariant data=QVariant()){ _items.push_back({text,data});update(); }
  void addItems(const QStringList& texts){ for(auto&t:texts)addItem(t);update(); }
  void insertItem(int idx,const QString& text,QVariant data=QVariant()){ int clamp=idx<0?0:idx>(int)_items.size()?(int)_items.size():idx; _items.insert(clamp,{text,data});update(); }
  void insertItems(int idx,const QStringList& texts){ for(int i=0;i<(int)texts.size();i++)insertItem(idx+i,texts[i]);update(); }
  void removeItem(int idx){ if(idx>=0&&idx<(int)_items.size()){_items.erase(_items.begin()+idx);if(_current>=idx&&_current>0)_current--;update();} }
  void clear()            { _items.clear();_current=-1;update(); }
  int  count()     const  { return _items.size(); }
  bool isEmpty()   const  { return _items.empty(); }

  void setCurrentIndex(int idx)         { if(idx>=0&&idx<(int)_items.size()){int old=_current;_current=idx;if(old!=idx){update();emit_signal("currentIndexChanged",{QVariant(idx)});emit_signal("currentTextChanged",{QVariant(currentText())});}} }
  void setCurrentText(const QString& t) { for(int i=0;i<(int)_items.size();i++)if(_items[i].text==t){setCurrentIndex(i);return;} }
  int     currentIndex()         const  { return _current; }
  QString currentText()          const  { return _current>=0&&_current<(int)_items.size()?_items[_current].text:""; }
  QVariant currentData(int role=0)const { return _current>=0&&_current<(int)_items.size()?_items[_current].data:QVariant(); }
  QString itemText(int idx)      const  { return idx>=0&&idx<(int)_items.size()?_items[idx].text:""; }
  QVariant itemData(int idx)     const  { return idx>=0&&idx<(int)_items.size()?_items[idx].data:QVariant(); }
  void setItemText(int idx,const QString& t){ if(idx>=0&&idx<(int)_items.size()){_items[idx].text=t;update();} }
  void setItemData(int idx,QVariant d)       { if(idx>=0&&idx<(int)_items.size())_items[idx].data=d; }
  int  findText(const QString& t,int flags=Qt::MatchExactly)const{ for(int i=0;i<(int)_items.size();i++)if(_items[i].text==t)return i;return -1; }
  int  findData(QVariant d)const{ for(int i=0;i<(int)_items.size();i++)if(_items[i].data==d)return i;return -1; }

  void setEditable(bool e)             { _editable=e; }
  bool isEditable()             const  { return _editable; }
  void setMaxVisibleItems(int n)       { _maxVisible=n; }
  void setMaxCount(int n)              { _maxCount=n; }
  void setMinimumContentsLength(int l) { _minContLen=l; }
  void setPlaceholderText(const QString& t){ _placeholder=t;update(); }
  void setSizeAdjustPolicy(int p)      { _sizeAdjust=p; }
  void setInsertPolicy(int p)          { _insertPolicy=p; }
  void setDuplicatesEnabled(bool e)    { _dups=e; }
  bool duplicatesEnabled()      const  { return _dups; }
  void setModelColumn(int c)           { _modelCol=c; }
  int  modelColumn()            const  { return _modelCol; }
  void showPopup()                     { _expanded=true;update(); }
  void hidePopup()                     { _expanded=false;update(); }

  void paintEvent(QPainter* p) override {
    if(!isVisible())return;
    QColor bg=_ss.hasBg?_ss.bg:QColor(45,45,45);
    QColor fg=_ss.hasFg?_ss.fg:QColor(220,220,220);
    QColor bc=_focused?QColor(0,120,215):(_ss.hasBorder?_ss.border:QColor(70,70,70));
    int r=_ss.borderRadius;
    p->setBrush(QBrush(bg)); p->setPen(QPen(bc));
    p->drawRoundedRect(_x,_y,_w,_h,r,r);
    QString txt=currentText().isEmpty()?_placeholder:currentText();
    p->setPen(QPen(currentText().isEmpty()?QColor(100,100,100):fg));
    p->setFont(_font);
    p->drawText(_x+_ss.padding,_y,_w-20,_h,Qt::AlignLeft|Qt::AlignVCenter,txt);
    // Arrow
    int ax=_x+_w-14,ay=_y+_h/2;
    tft.fillTriangle(ax,ay-3,ax+8,ay-3,ax+4,ay+4,(_ss.hasFg?_ss.fg:QColor(180,180,180)).toRgb565());
    if(_expanded) _drawDropdown(p);
  }

  void _drawDropdown(QPainter* p){
    int rows=min(_maxVisible,(int)_items.size());
    int dy=_y+_h;
    QColor bg=QColor(45,45,45),sel=QColor(0,90,160),fg2=QColor(220,220,220);
    p->setBrush(QBrush(bg)); p->setPen(QPen(QColor(70,70,70)));
    p->drawRoundedRect(_x,dy,_w,rows*22+4,4,4);
    for(int i=0;i<rows;i++){
      bool isSel=(i==_current);
      if(isSel){ p->setBrush(QBrush(sel));p->setPen(QPen::NoPen);p->drawRect(_x+2,dy+2+i*22,_w-4,22); }
      p->setPen(QPen(isSel?QColor(255,255,255):fg2)); p->setFont(_font);
      p->drawText(_x+_ss.padding,dy+2+i*22,_w-_ss.padding*2,22,Qt::AlignLeft|Qt::AlignVCenter,_items[i].text);
    }
  }

  bool handleTouch(int tx,int ty) override {
    if(!isVisible()||!isEnabled())return false;
    if(_expanded){
      int rows=min(_maxVisible,(int)_items.size());
      int dy=_y+_h;
      if(tx>=_x&&tx<=_x+_w&&ty>=dy&&ty<dy+rows*22+4){
        int idx=(ty-dy-2)/22;
        if(idx>=0&&idx<(int)_items.size())setCurrentIndex(idx);
      }
      _expanded=false;update();return true;
    }
    if(!contains(tx,ty))return false;
    _expanded=true;update();return true;
  }

  // Signals: currentIndexChanged(int), currentTextChanged(str), activated(int), activated(str), highlighted(int), editTextChanged(str)
private:
  struct Item { QString text; QVariant data; };
  QList<Item> _items;
  int _current=-1,_maxVisible=8,_maxCount=2147483647,_minContLen=0,_modelCol=0;
  bool _editable=false,_expanded=false,_dups=true;
  int _sizeAdjust=0,_insertPolicy=0;
  QString _placeholder;
};

// ── QListWidget ───────────────────────────────────────────────────────────────
class QListWidgetItem {
public:
  QListWidgetItem(const QString& text="") : _text(text) {}
  QString text()              const { return _text; }
  void setText(const QString& t)    { _text=t; }
  void setData(int role,QVariant v) { _data[role]=v; }
  QVariant data(int role)     const { auto it=_data.find(role);return it!=_data.end()?it->second:QVariant(); }
  void setSelected(bool s)          { _selected=s; }
  bool isSelected()           const { return _selected; }
  void setHidden(bool h)            { _hidden=h; }
  bool isHidden()             const { return _hidden; }
  void setCheckState(Qt::CheckState s){ _checkState=(int)s; }
  Qt::CheckState checkState() const { return (Qt::CheckState)_checkState; }
  void setFlags(Qt::ItemFlags f)    { _flags=f; }
  Qt::ItemFlags flags()       const { return _flags; }
  void setToolTip(const QString& t) { _tooltip=t; }
  QString toolTip()           const { return _tooltip; }
  void setForeground(QColor c)      { _fg=c; }
  void setBackground(QColor c)      { _bg=c; }
  QColor foreground()         const { return _fg; }
  QColor background()         const { return _bg; }
  bool operator<(const QListWidgetItem& o)const{ return _text<o._text; }
private:
  QString _text,_tooltip;
  std::map<int,QVariant> _data;
  bool _selected=false,_hidden=false;
  int _checkState=0;
  Qt::ItemFlags _flags=Qt::ItemIsEnabled|Qt::ItemIsSelectable;
  QColor _fg{220,220,220},_bg{0,0,0,0};
};

class QListWidget : public QWidget {
public:
  enum SelectionMode { NoSelection, SingleSelection, MultiSelection, ExtendedSelection, ContiguousSelection };
  enum ViewMode { ListMode, IconMode };
  enum Flow { TopToBottom, LeftToRight };
  enum ResizeMode { Fixed, Adjust };

  explicit QListWidget(QWidget* parent=nullptr) : QWidget(parent) {}
  const char* metaClassName() const override { return "QListWidget"; }

  QListWidgetItem* addItem(const QString& text){ auto*i=new QListWidgetItem(text);addItem(i);return i; }
  void addItem(QListWidgetItem* item)          { _items.push_back(item);update(); }
  void addItems(const QStringList& texts)      { for(auto&t:texts)addItem(t);update(); }
  void insertItem(int row,QListWidgetItem* i)  { int clamp=row<0?0:row>(int)_items.size()?(int)_items.size():row; _items.insert(clamp,i);update(); }
  void insertItem(int row,const QString& text) { insertItem(row,new QListWidgetItem(text)); }
  void insertItems(int row,const QStringList& ts){ for(int i=0;i<(int)ts.size();i++)insertItem(row+i,ts[i]);update(); }
  QListWidgetItem* takeItem(int row)           { if(row<0||row>=(int)_items.size())return nullptr;auto*i=_items[row];_items.erase(_items.begin()+row);update();return i; }
  void removeItemWidget(QListWidgetItem* i)    { _items.erase(std::remove(_items.begin(),_items.end(),i),_items.end());update(); }
  void clear()                                 { for(auto*i:_items)delete i;_items.clear();update(); }

  int  count()                const { return _items.size(); }
  int  row(QListWidgetItem* i)const { for(int j=0;j<(int)_items.size();j++)if(_items[j]==i)return j;return -1; }
  QListWidgetItem* item(int r)const { return r>=0&&r<(int)_items.size()?_items[r]:nullptr; }
  QListWidgetItem* currentItem()const{ return _current>=0&&_current<(int)_items.size()?_items[_current]:nullptr; }
  int  currentRow()           const { return _current; }
  void setCurrentRow(int r)         { _current=constrain(r,-1,(int)_items.size()-1);update();if(_current>=0)emit_signal("currentRowChanged",{QVariant(_current)}); }
  void setCurrentItem(QListWidgetItem* i){ setCurrentRow(row(i)); }

  QList<QListWidgetItem*> selectedItems()const{
    QList<QListWidgetItem*> r;
    for(auto*i:_items)if(i->isSelected())r.push_back(i);
    return r;
  }
  QList<QListWidgetItem*> findItems(const QString& text,int flags=Qt::MatchExactly)const{
    QList<QListWidgetItem*> r;
    for(auto*i:_items)if(flags==Qt::MatchContains?i->text().indexOf(text)>=0:i->text()==text)r.push_back(i);
    return r;
  }

  void setSelectionMode(SelectionMode m){ _selMode=m; }
  SelectionMode selectionMode()  const  { return _selMode; }
  void setViewMode(ViewMode m)          { _viewMode=m;update(); }
  void setFlow(Flow f)                  { _flow=f;update(); }
  void setSortingEnabled(bool s)        { _sortEnabled=s; }
  void sortItems(Qt::SortOrder o=Qt::AscendingOrder){
    if(o==Qt::AscendingOrder)
      std::sort(_items.begin(),_items.end(),[](QListWidgetItem*a,QListWidgetItem*b){return *a<*b;});
    else
      std::sort(_items.begin(),_items.end(),[](QListWidgetItem*a,QListWidgetItem*b){return *b<*a;});
    update();
  }
  void setIconSize(QSize s)             { _iconSize=s; }
  void setGridSize(QSize s)             { _gridSize=s; }
  void setSpacing(int s)                { _spacing=s;update(); }
  void setWordWrap(bool w)              { _wordWrap=w;update(); }
  void scrollToItem(QListWidgetItem* i,int hint=0){ int r=row(i);if(r>=0)_scrollTop=max(0,r-5);update(); }
  void scrollToTop()                    { _scrollTop=0;update(); }
  void scrollToBottom()                 { _scrollTop=max(0,(int)_items.size()-visibleRows());update(); }
  void editItem(QListWidgetItem* i)     {}
  void openPersistentEditor(QListWidgetItem*){}
  void closePersistentEditor(QListWidgetItem*){}
  bool isPersistentEditorOpen(QListWidgetItem*)const{return false;}
  void setItemWidget(QListWidgetItem*,QWidget*){}
  QWidget* itemWidget(QListWidgetItem*)const{return nullptr;}
  bool isSortingEnabled()        const  { return _sortEnabled; }
  QListWidgetItem* itemAt(QPoint p)const{ int r=_scrollTop+(p.y()-_y)/_rowH;return item(r); }
  QListWidgetItem* itemAt(int x,int y)const{ return itemAt({x,y}); }
  QRect visualItemRect(QListWidgetItem* i)const{ int r=row(i);return{_x,_y+(r-_scrollTop)*_rowH,_w,_rowH}; }
  int  indexFromItem(QListWidgetItem* i)const{ return row(i); }

  void paintEvent(QPainter* p) override {
    if(!isVisible())return;
    QColor bg=_ss.hasBg?_ss.bg:QColor(35,35,35);
    QColor fg=_ss.hasFg?_ss.fg:QColor(220,220,220);
    QColor selBg=QColor(0,90,160),altBg=QColor(40,40,40);
    p->setBrush(QBrush(bg)); p->setPen(QPen(_ss.hasBorder?_ss.border:QColor(60,60,60)));
    p->drawRect(QRect{_x,_y,_w,_h});
    int vr=visibleRows();
    for(int i=0;i<vr&&(_scrollTop+i)<(int)_items.size();i++){
      auto* item2=_items[_scrollTop+i];
      if(item2->isHidden())continue;
      int iy=_y+i*_rowH;
      bool sel=item2->isSelected()||((_scrollTop+i)==_current);
      QColor rowBg=sel?selBg:(i%2==0?bg:altBg);
      p->setBrush(QBrush(rowBg)); p->setPen(QPen::NoPen);
      p->drawRect(QRect{_x+1,iy,_w-2,_rowH});
      p->setPen(QPen(sel?QColor(255,255,255):fg));
      p->setFont(_font);
      QString txt=item2->text();
      if((int)txt.length()*6*_font.tftSize()>_w-_ss.padding*2) txt=txt.substring(0,(_w-_ss.padding*2)/(6*_font.tftSize())-1)+"~";
      p->drawText(_x+_ss.padding,iy,_w-_ss.padding*2,_rowH,Qt::AlignLeft|Qt::AlignVCenter,txt);
    }
    // Scrollbar
    if((int)_items.size()>vr){
      int sbH=_h*vr/max(1,(int)_items.size());
      int sbY=_y+_scrollTop*_h/max(1,(int)_items.size());
      tft.fillRect(_x+_w-4,_y,4,_h,QColor(50,50,50).toRgb565());
      tft.fillRect(_x+_w-4,sbY,4,sbH,QColor(100,100,100).toRgb565());
    }
  }

  bool handleTouch(int tx,int ty) override {
    if(!isVisible()||!isEnabled())return false;
    if(!contains(tx,ty))return false;
    int idx=_scrollTop+(ty-_y)/_rowH;
    if(idx>=0&&idx<(int)_items.size()){
      if(_selMode==SingleSelection){for(auto*i:_items)i->setSelected(false);}
      _items[idx]->setSelected(!_items[idx]->isSelected()||_selMode==SingleSelection?true:_items[idx]->isSelected());
      _current=idx; update();
      emit_signal("itemClicked",{QVariant(_items[idx]->text())});
      emit_signal("currentRowChanged",{QVariant(idx)});
      emit_signal("currentItemChanged",{});
    }
    return true;
  }

  // Signals: itemClicked(item), itemDoubleClicked(item), itemPressed(item), itemActivated(item), itemEntered(item), itemChanged(item), currentItemChanged(curr,prev), currentTextChanged(str), currentRowChanged(int)
private:
  QList<QListWidgetItem*> _items;
  int _current=-1,_scrollTop=0,_rowH=20,_spacing=0;
  SelectionMode _selMode=SingleSelection;
  ViewMode _viewMode=ListMode;
  Flow _flow=TopToBottom;
  bool _sortEnabled=false,_wordWrap=false;
  QSize _iconSize{16,16},_gridSize{0,0};
  int visibleRows()const{return _h/_rowH;}
};

// ── QTabWidget ────────────────────────────────────────────────────────────────
class QTabWidget : public QWidget {
public:
  enum TabPosition { North, South, West, East };
  enum TabShape    { Rounded, Triangular };

  explicit QTabWidget(QWidget* parent=nullptr) : QWidget(parent){ _tabBarH=24; }
  const char* metaClassName() const override { return "QTabWidget"; }

  int  addTab(QWidget* page,const QString& label)        { return insertTab(-1,page,label); }
  int  insertTab(int idx,QWidget* page,const QString& label){
    Tab t; t.page=page; t.label=label;
    if(idx<0||idx>=(int)_tabs.size()) _tabs.push_back(t);
    else { int clamp=idx<0?0:idx>(int)_tabs.size()?(int)_tabs.size():idx; _tabs.insert(clamp,t); }
    if(_tabs.size()==1)setCurrentIndex(0);
    update(); return _tabs.size()-1;
  }
  void removeTab(int idx){ if(idx>=0&&idx<(int)_tabs.size()){_tabs.erase(_tabs.begin()+idx);if(_current>=idx&&_current>0)_current--;update();} }
  void setTabText(int idx,const QString& t){ if(idx>=0&&idx<(int)_tabs.size()){_tabs[idx].label=t;update();} }
  void setTabToolTip(int idx,const QString& t){ if(idx>=0&&idx<(int)_tabs.size())_tabs[idx].tooltip=t; }
  void setTabEnabled(int idx,bool e){ if(idx>=0&&idx<(int)_tabs.size()){_tabs[idx].enabled=e;update();} }
  void setTabVisible(int idx,bool v){ if(idx>=0&&idx<(int)_tabs.size()){_tabs[idx].visible=v;update();} }

  void setCurrentIndex(int idx){
    if(idx<0||idx>=(int)_tabs.size())return;
    int old=_current;_current=idx;
    if(old!=idx){
      if(old>=0&&old<(int)_tabs.size()&&_tabs[old].page)_tabs[old].page->hide();
      if(_tabs[idx].page)_tabs[idx].page->show();
      update();
      emit_signal("currentChanged",{QVariant(idx)});
    }
  }
  void setCurrentWidget(QWidget* w){ for(int i=0;i<(int)_tabs.size();i++)if(_tabs[i].page==w){setCurrentIndex(i);return;} }

  int     currentIndex()  const { return _current; }
  QWidget* currentWidget()const { return _current>=0&&_current<(int)_tabs.size()?_tabs[_current].page:nullptr; }
  int     count()         const { return _tabs.size(); }
  int     indexOf(QWidget* w)const{ for(int i=0;i<(int)_tabs.size();i++)if(_tabs[i].page==w)return i;return -1; }
  QWidget* widget(int idx)const { return idx>=0&&idx<(int)_tabs.size()?_tabs[idx].page:nullptr; }
  QString tabText(int idx)const { return idx>=0&&idx<(int)_tabs.size()?_tabs[idx].label:""; }
  bool    isTabEnabled(int idx)const{ return idx>=0&&idx<(int)_tabs.size()?_tabs[idx].enabled:false; }
  bool    isTabVisible(int idx)const{ return idx>=0&&idx<(int)_tabs.size()?_tabs[idx].visible:false; }

  void setTabPosition(TabPosition p){ _tabPos=p;update(); }
  void setTabShape(TabShape s)      { _tabShape=s;update(); }
  void setMovable(bool m)           { _movable=m; }
  void setTabsClosable(bool c)      { _closable=c;update(); }
  void setDocumentMode(bool d)      { _docMode=d;update(); }
  void setElideMode(int m)          { _elideMode=m; }
  void setIconSize(QSize s)         { _iconSize=s; }
  void setUsesScrollButtons(bool u) { _scrollBtns=u; }
  bool tabsClosable()        const  { return _closable; }
  bool isMovable()           const  { return _movable; }
  TabPosition tabPosition()  const  { return _tabPos; }
  TabShape    tabShape()     const  { return _tabShape; }
  QWidget* cornerWidget(Qt::Corner c=Qt::TopRightCorner)const{ return nullptr; }
  void setCornerWidget(QWidget*,Qt::Corner=Qt::TopRightCorner){}

  void clear(){ _tabs.clear();_current=-1;update(); }

  void paintEvent(QPainter* p) override {
    if(!isVisible())return;
    int n=_tabs.size(); if(n==0)return;
    int tabW=_w/max(1,n);
    // Tab bar
    for(int i=0;i<n;i++){
      bool sel=(i==_current);
      bool en=_tabs[i].enabled;
      QColor tbg=sel?QColor(0,120,215):QColor(50,50,50);
      QColor tfg=sel?QColor(255,255,255):(en?QColor(180,180,180):QColor(80,80,80));
      p->setBrush(QBrush(tbg));
      p->setPen(QPen(QColor(70,70,70)));
      p->drawRoundedRect(_x+i*tabW,_y,tabW,_tabBarH,4,4);
      p->setPen(QPen(tfg)); p->setFont(_font);
      QString lbl=_tabs[i].label;
      if(_closable)lbl+=" x";
      int lw=lbl.length()*6*_font.tftSize();
      if(lw>tabW-4)lbl=lbl.substring(0,(tabW-8)/(6*_font.tftSize()));
      p->drawText(_x+i*tabW,_y,tabW,_tabBarH,Qt::AlignCenter,lbl);
    }
    // Content area
    tft.fillRect(_x,_y+_tabBarH,_w,_h-_tabBarH,QColor(30,30,30).toRgb565());
    tft.drawRect(_x,_y+_tabBarH,_w,_h-_tabBarH,QColor(60,60,60).toRgb565());
    if(_current>=0&&_current<(int)_tabs.size()&&_tabs[_current].page){
      auto* page=_tabs[_current].page;
      page->setGeometry(_x+2,_y+_tabBarH+2,_w-4,_h-_tabBarH-4);
      QPainter cp(page); page->paintEvent(&cp);
    }
  }

  bool handleTouch(int tx,int ty) override {
    if(!isVisible()||!isEnabled())return false;
    if(tx>=_x&&tx<=_x+_w&&ty>=_y&&ty<_y+_tabBarH){
      int tabW=_w/max(1,(int)_tabs.size());
      int idx=(tx-_x)/tabW;
      if(idx>=0&&idx<(int)_tabs.size()&&_tabs[idx].enabled){
        if(_closable&&tx>_x+idx*tabW+tabW-14){emit_signal("tabCloseRequested",{QVariant(idx)});return true;}
        setCurrentIndex(idx);
      }
      return true;
    }
    if(_current>=0&&_current<(int)_tabs.size()&&_tabs[_current].page)
      return _tabs[_current].page->handleTouch(tx,ty);
    return false;
  }

  // Signals: currentChanged(int), tabCloseRequested(int), tabBarClicked(int), tabBarDoubleClicked(int)
private:
  struct Tab { QWidget* page=nullptr; QString label,tooltip; bool enabled=true,visible=true; };
  QList<Tab> _tabs;
  int _current=-1,_tabBarH=24;
  TabPosition _tabPos=North;
  TabShape    _tabShape=Rounded;
  bool _movable=false,_closable=false,_docMode=false,_scrollBtns=true;
  int _elideMode=0;
  QSize _iconSize{16,16};
};

// ── QScrollArea ───────────────────────────────────────────────────────────────
class QScrollArea : public QWidget {
public:
  explicit QScrollArea(QWidget* parent=nullptr) : QWidget(parent) {}
  const char* metaClassName() const override { return "QScrollArea"; }

  void setWidget(QWidget* w)               { _content=w; update(); }
  QWidget* widget()                  const { return _content; }
  void setWidgetResizable(bool r)          { _resizable=r; if(r&&_content)_content->resize(_w,_content->height()); update(); }
  bool widgetResizable()             const { return _resizable; }
  void setAlignment(Qt::Alignment a)       { _align=a; update(); }
  Qt::Alignment alignment()          const { return _align; }
  void setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy p){ _hPolicy=p; }
  void setVerticalScrollBarPolicy(Qt::ScrollBarPolicy p)  { _vPolicy=p; }
  Qt::ScrollBarPolicy horizontalScrollBarPolicy()const{ return _hPolicy; }
  Qt::ScrollBarPolicy verticalScrollBarPolicy()  const{ return _vPolicy; }
  void ensureVisible(int x,int y,int xm=50,int ym=50){ _scrollX=max(0,x-_w+xm);_scrollY=max(0,y-_h+ym);update(); }
  void ensureWidgetVisible(QWidget* w,int xm=50,int ym=50){ if(w)ensureVisible(w->x(),w->y(),xm,ym); }
  void setScrollPosition(int x,int y){ _scrollX=x;_scrollY=y;update(); }
  QPoint scrollPosition()     const  { return{_scrollX,_scrollY}; }
  int  horizontalScrollBarValue()const{ return _scrollX; }
  int  verticalScrollBarValue()  const{ return _scrollY; }
  void scrollBy(int dx,int dy)       { _scrollX+=dx;_scrollY+=dy;update(); }

  void paintEvent(QPainter* p) override {
    if(!isVisible())return;
    p->fillRect(QRect{_x,_y,_w,_h},_palette.color(QPalette::Base));
    p->setPen(QPen(QColor(60,60,60)));
    p->drawRect(QRect{_x,_y,_w,_h});
    if(!_content)return;
    // Clip and render content with scroll offset
    int cw=_resizable?_w:_content->width();
    int ch=_content->height();
    _content->setGeometry(_x-_scrollX,_y-_scrollY,cw,ch);
    if(_content->isVisible()){ QPainter cp(_content); _content->paintEvent(&cp); }
    // Scrollbars
    if(ch>_h&&_vPolicy!=Qt::ScrollBarAlwaysOff){
      int sbH=_h*_h/ch; int sbY=_y+_scrollY*_h/ch;
      tft.fillRect(_x+_w-5,_y,5,_h,QColor(40,40,40).toRgb565());
      tft.fillRect(_x+_w-5,sbY,5,sbH,QColor(100,100,100).toRgb565());
    }
    if(cw>_w&&_hPolicy!=Qt::ScrollBarAlwaysOff){
      int sbW=_w*_w/cw; int sbX=_x+_scrollX*_w/cw;
      tft.fillRect(_x,_y+_h-5,_w,5,QColor(40,40,40).toRgb565());
      tft.fillRect(sbX,_y+_h-5,sbW,5,QColor(100,100,100).toRgb565());
    }
  }

  bool handleTouch(int tx,int ty) override {
    if(!isVisible()||!isEnabled())return false;
    if(!contains(tx,ty))return false;
    if(_content) return _content->handleTouch(tx+_scrollX,ty+_scrollY);
    return false;
  }

private:
  QWidget* _content=nullptr;
  bool _resizable=false;
  int _scrollX=0,_scrollY=0;
  Qt::Alignment _align=Qt::AlignLeft|Qt::AlignTop;
  Qt::ScrollBarPolicy _hPolicy=Qt::ScrollBarAsNeeded,_vPolicy=Qt::ScrollBarAsNeeded;
};

// ── QGroupBox ─────────────────────────────────────────────────────────────────
class QGroupBox : public QWidget {
public:
  explicit QGroupBox(QWidget* parent=nullptr) : QWidget(parent){ _titleH=22; }
  explicit QGroupBox(const QString& title,QWidget* parent=nullptr) : QWidget(parent),_title(title){ _titleH=22; }
  const char* metaClassName() const override { return "QGroupBox"; }

  void setTitle(const QString& t)      { _title=t;update(); }
  QString title()               const  { return _title; }
  void setCheckable(bool c)            { _checkable=c;update(); }
  bool isCheckable()            const  { return _checkable; }
  void setChecked(bool c)              { if(_checkable){_checked=c;update();emit_signal("toggled",{QVariant(c)});} }
  bool isChecked()              const  { return _checked; }
  void setAlignment(Qt::Alignment a)   { _align=a;update(); }
  Qt::Alignment alignment()     const  { return _align; }
  void setFlat(bool f)                 { _flat=f;update(); }
  bool isFlat()                 const  { return _flat; }

  void paintEvent(QPainter* p) override {
    if(!isVisible())return;
    QColor bg=_ss.hasBg?_ss.bg:QColor(35,35,35);
    QColor bc=_ss.hasBorder?_ss.border:QColor(70,70,70);
    QColor tc=QColor(0,120,215);
    if(!_flat){ p->setBrush(QBrush(bg));p->setPen(QPen(bc));p->drawRoundedRect(_x,_y+_titleH/2,_w,_h-_titleH/2,4,4); }
    // Title
    int tw=_title.length()*6*_font.tftSize()+8;
    tft.fillRect(_x+8,_y,tw,_titleH,_flat?QColor(0,0,0).toRgb565():bg.toRgb565());
    p->setPen(QPen(tc)); p->setFont(_font);
    p->drawText(_x+12,_y,tw,_titleH,Qt::AlignLeft|Qt::AlignVCenter,_title);
    if(_checkable){
      p->setBrush(_checked?QBrush(tc):QBrush(bg));
      p->setPen(QPen(bc));
      p->drawRoundedRect(_x+tw+4,_y+(_titleH-12)/2,12,12,2,2);
    }
    // Children
    for(auto*c:childWidgets())if(c->isVisible()){ QPainter cp(c); c->paintEvent(&cp); }
  }

  bool handleTouch(int tx,int ty) override {
    if(!isVisible()||!isEnabled())return false;
    if(_checkable&&tx>=_x&&tx<_x+30&&ty>=_y&&ty<_y+_titleH){ setChecked(!_checked);return true; }
    for(auto*c:childWidgets())if(c->handleTouch(tx,ty))return true;
    return false;
  }

  // Signals: clicked(bool), toggled(bool)
private:
  QString _title;
  bool _checkable=false,_checked=true,_flat=false;
  Qt::Alignment _align=Qt::AlignLeft;
  int _titleH;
};

// ── QToolButton ───────────────────────────────────────────────────────────────
class QToolButton : public QAbstractButton {
public:
  enum ToolButtonPopupMode { DelayedPopup, MenuButtonPopup, InstantPopup };
  enum ArrowType { NoArrow, UpArrow, DownArrow, LeftArrow, RightArrow };

  explicit QToolButton(QWidget* parent=nullptr) : QAbstractButton(parent){ _w=32;_h=32; }
  const char* metaClassName() const override { return "QToolButton"; }
  QSize sizeHint() const override { return {32,32}; }

  void setArrowType(ArrowType a)       { _arrow=a;update(); }
  void setAutoRaise(bool r)            { _autoRaise=r;update(); }
  bool autoRaise()              const  { return _autoRaise; }
  void setPopupMode(ToolButtonPopupMode m){ _popupMode=m; }
  void setToolButtonStyle(int s)       { _style2=s;update(); }
  int  toolButtonStyle()        const  { return _style2; }

  void paintEvent(QPainter* p) override {
    if(!isVisible())return;
    QColor bg=_down?QColor(60,60,60):(_autoRaise?QColor(0,0,0,0):QColor(50,50,50));
    QColor fg=_ss.hasFg?_ss.fg:QColor(220,220,220);
    if(!_autoRaise||_down){ p->setBrush(QBrush(bg));p->setPen(QPen(QColor(70,70,70)));p->drawRoundedRect(_x,_y,_w,_h,4,4); }
    // Arrow
    if(_arrow!=NoArrow){
      int cx=_x+_w/2,cy=_y+_h/2;
      p->setPen(QPen::NoPen); p->setBrush(QBrush(fg));
      if(_arrow==UpArrow)    p->drawPolygon(QPolygon{{cx-5,cy+3},{cx+5,cy+3},{cx,cy-4}});
      else if(_arrow==DownArrow) p->drawPolygon(QPolygon{{cx-5,cy-3},{cx+5,cy-3},{cx,cy+4}});
      else if(_arrow==LeftArrow) p->drawPolygon(QPolygon{{cx+3,cy-5},{cx+3,cy+5},{cx-4,cy}});
      else if(_arrow==RightArrow)p->drawPolygon(QPolygon{{cx-3,cy-5},{cx-3,cy+5},{cx+4,cy}});
    }
    if(!_text.isEmpty()){ p->setPen(QPen(fg));p->setFont(_font);p->drawText(_x,_y,_w,_h,Qt::AlignCenter,_text); }
  }
private:
  ArrowType _arrow=NoArrow;
  ToolButtonPopupMode _popupMode=DelayedPopup;
  bool _autoRaise=false;
  int _style2=0;
};

// ── QDial ─────────────────────────────────────────────────────────────────────
class QDial : public QAbstractSlider {
public:
  explicit QDial(QWidget* parent=nullptr) : QAbstractSlider(parent){ _w=60;_h=60; }
  const char* metaClassName() const override { return "QDial"; }
  QSize sizeHint() const override { return {60,60}; }

  void setNotchesVisible(bool v)       { _notches=v;update(); }
  void setNotchTarget(double t)        { _notchTarget=t;update(); }
  void setWrapping(bool w)             { _wrap=w; }
  bool notchesVisible()         const  { return _notches; }
  bool wrapping()               const  { return _wrap; }

  void paintEvent(QPainter* p) override {
    if(!isVisible())return;
    int cx=_x+_w/2,cy=_y+_h/2,r=min(_w,_h)/2-4;
    p->setBrush(QBrush(QColor(50,50,50))); p->setPen(QPen(QColor(80,80,80)));
    p->drawEllipse(QPoint(cx,cy),r,r);
    float t=(float)(_value-_min)/max(1,_max-_min);
    float angle=(-M_PI*0.75f)+t*M_PI*1.5f;
    int nx=cx+(int)(cosf(angle)*(r-6)),ny=cy+(int)(sinf(angle)*(r-6));
    p->setPen(QPen(QColor(0,120,215),3));
    p->drawLine(cx,cy,nx,ny);
    if(_notches){
      p->setPen(QPen(QColor(100,100,100)));
      int steps=(_max-_min)/max(1,(int)_notchTarget);
      for(int i=0;i<=steps;i++){
        float a=(-M_PI*0.75f)+(float)i/steps*M_PI*1.5f;
        p->drawLine(cx+(int)(cosf(a)*(r-4)),cy+(int)(sinf(a)*(r-4)),cx+(int)(cosf(a)*r),cy+(int)(sinf(a)*r));
      }
    }
    p->setPen(QPen(QColor(180,180,180))); p->setFont(_font);
    p->drawText(_x,_y,_w,_h,Qt::AlignCenter,String(_value));
  }

  bool handleTouch(int tx,int ty) override {
    if(!isVisible()||!isEnabled())return false;
    if(!contains(tx,ty))return false;
    int cx=_x+_w/2,cy=_y+_h/2;
    float angle=atan2f(ty-cy,tx-cx);
    float t=(angle+M_PI*0.75f)/(M_PI*1.5f);
    t=constrain(t,0.0f,1.0f);
    setValue(_min+(int)(t*(_max-_min)));
    return true;
  }
private:
  bool _notches=false,_wrap=false;
  double _notchTarget=3.75;
};

// ── QLCDNumber ────────────────────────────────────────────────────────────────
class QLCDNumber : public QWidget {
public:
  enum Mode { Hex, Dec, Oct, Bin };
  enum SegmentStyle { Outline, Filled, Flat };

  explicit QLCDNumber(QWidget* parent=nullptr) : QWidget(parent){ _h=40; }
  explicit QLCDNumber(int numDigits,QWidget* parent=nullptr) : QWidget(parent),_digits(numDigits){ _h=40; }
  const char* metaClassName() const override { return "QLCDNumber"; }
  QSize sizeHint() const override { return {_digits*20,40}; }

  void display(int n)             { _intVal=n;_strVal=String(n);update();emit_signal("valueChanged",{QVariant(n)}); }
  void display(double d)          { _dblVal=d;_strVal=String(d,_decimals);update();emit_signal("valueChanged",{QVariant(d)}); }
  void display(const QString& s)  { _strVal=s;update();emit_signal("valueChanged",{QVariant(s)}); }
  void setMode(Mode m)            { _mode=m;update(); }
  void setSegmentStyle(SegmentStyle s){ _style2=s;update(); }
  void setNumDigits(int n)        { _digits=n;update(); }
  void setSmallDecimalPoint(bool s){ _smallDecPt=s;update(); }
  void setDigitCount(int n)       { _digits=n;update(); }
  void setBinDecOctHexMode(bool,bool,bool,bool){}
  void setDecMode()               { _mode=Dec;update(); }
  void setHexMode()               { _mode=Hex;update(); }
  void setOctMode()               { _mode=Oct;update(); }
  void setBinMode()               { _mode=Bin;update(); }

  double value()          const   { return _dblVal; }
  int    intValue()       const   { return _intVal; }
  Mode   mode()           const   { return _mode; }
  int    numDigits()      const   { return _digits; }
  int    digitCount()     const   { return _digits; }
  SegmentStyle segmentStyle()const{ return _style2; }
  bool   checkOverflow(double d)const{ return false; }
  bool   checkOverflow(int n)   const{ return false; }
  bool   smallDecimalPoint()    const{ return _smallDecPt; }

  void paintEvent(QPainter* p) override {
    if(!isVisible())return;
    p->fillRect(QRect{_x,_y,_w,_h},QColor(0,30,0));
    p->setPen(QPen(QColor(70,70,70)));
    p->drawRect(QRect{_x,_y,_w,_h});
    p->setPen(QPen(QColor(0,220,0))); p->setFont(QFont("default",16,QFont::Bold));
    QString display=_strVal;
    switch(_mode){
      case Hex: display=String(_intVal,16);display.toUpperCase();break;
      case Oct: display=String(_intVal,8);break;
      case Bin: display=String(_intVal,2);break;
      default:break;
    }
    p->drawText(_x,_y,_w,_h,Qt::AlignRight|Qt::AlignVCenter,display);
  }
private:
  int _intVal=0,_digits=8,_decimals=2;
  double _dblVal=0;
  QString _strVal="0";
  Mode _mode=Dec;
  SegmentStyle _style2=Outline;
  bool _smallDecPt=false;
};

// ── QFrame ────────────────────────────────────────────────────────────────────
class QFrame : public QWidget {
public:
  enum Shape  { NoFrame, Box, Panel, WinPanel, HLine, VLine, StyledPanel };
  enum Shadow { Plain=16, Raised=32, Sunken=48 };

  explicit QFrame(QWidget* parent=nullptr) : QWidget(parent) {}
  const char* metaClassName() const override { return "QFrame"; }

  void setFrameShape(Shape s)        { _shape=s;update(); }
  void setFrameShadow(Shadow s)      { _shadow2=s;update(); }
  void setFrameStyle(int style)      { _shape=(Shape)(style&0x0F);_shadow2=(Shadow)(style&0xF0);update(); }
  void setLineWidth(int w)           { _lineW=w;update(); }
  void setMidLineWidth(int w)        { _midLineW=w;update(); }
  int  frameWidth()           const  { return _lineW; }
  int  lineWidth()            const  { return _lineW; }
  int  midLineWidth()         const  { return _midLineW; }
  Shape  frameShape()         const  { return _shape; }
  Shadow frameShadow()        const  { return _shadow2; }
  int  frameStyle()           const  { return (int)_shape|(int)_shadow2; }
  QRect frameRect()           const  { return {_x,_y,_w,_h}; }
  void setFrameRect(QRect r)         { setGeometry(r); }

  void paintEvent(QPainter* p) override {
    if(!isVisible())return;
    if(_autoFill)p->fillRect(QRect{_x,_y,_w,_h},_palette.color(QPalette::Window));
    QColor light=QColor(100,100,100),dark=QColor(20,20,20),mid=QColor(60,60,60);
    switch(_shape){
      case HLine: p->setPen(QPen(_shadow2==Sunken?dark:light,_lineW)); p->drawLine(_x,_y+_h/2,_x+_w,_y+_h/2); break;
      case VLine: p->setPen(QPen(_shadow2==Sunken?dark:light,_lineW)); p->drawLine(_x+_w/2,_y,_x+_w/2,_y+_h); break;
      case Box: case Panel: case StyledPanel:
        p->setPen(QPen(_shadow2==Sunken?dark:light,_lineW)); p->drawRect(QRect{_x,_y,_w,_h});
        if(_shadow2!=Plain){ p->setPen(QPen(_shadow2==Sunken?light:dark,_lineW)); p->drawLine(_x+_w,_y,_x+_w,_y+_h); p->drawLine(_x,_y+_h,_x+_w,_y+_h); }
        break;
      default: break;
    }
    for(auto*c:childWidgets())if(c->isVisible()){ QPainter cp(c);c->paintEvent(&cp); }
  }
private:
  Shape _shape=NoFrame; Shadow _shadow2=Plain;
  int _lineW=1,_midLineW=0;
};

// ── QSplitter ─────────────────────────────────────────────────────────────────
class QSplitter : public QWidget {
public:
  explicit QSplitter(QWidget* parent=nullptr) : QWidget(parent) {}
  explicit QSplitter(Qt::Orientation o,QWidget* parent=nullptr) : QWidget(parent),_orientation(o) {}
  const char* metaClassName() const override { return "QSplitter"; }

  void addWidget(QWidget* w)           { _panels.push_back(w);_sizes.push_back(0);_reflow(); }
  void insertWidget(int idx,QWidget* w){ int clamp=idx<0?0:idx>(int)_panels.size()?(int)_panels.size():idx; _panels.insert(clamp,w);_sizes.insert(clamp,0);_reflow(); }
  QWidget* widget(int idx)       const { return idx>=0&&idx<(int)_panels.size()?_panels[idx]:nullptr; }
  int  count()                   const { return _panels.size(); }
  int  indexOf(QWidget* w)       const { for(int i=0;i<(int)_panels.size();i++)if(_panels[i]==w)return i;return -1; }
  void setOrientation(Qt::Orientation o){ _orientation=o;_reflow(); }
  Qt::Orientation orientation()  const { return _orientation; }
  void setHandleWidth(int w)           { _handleW=w;_reflow(); }
  int  handleWidth()             const { return _handleW; }
  void setSizes(QList<int> sizes)      { _sizes=sizes;_reflow(); }
  QList<int> sizes()             const { return _sizes; }
  // Use uint8_t instead of bool to avoid std::vector<bool> proxy-reference issue
  void setCollapsible(int idx,bool c)  { if(idx>=0&&idx<(int)_collapsible.size())_collapsible[idx]=(uint8_t)c; }
  void setChildrenCollapsible(bool c)  { for(int i=0;i<(int)_collapsible.size();i++)_collapsible[i]=(uint8_t)c; }
  void moveSplitter(int pos,int idx)   { if(idx>=0&&idx<(int)_sizes.size())_sizes[idx]=pos;_reflow(); }
  void refresh()                       { _reflow(); }
  QList<int> indexOf()           const { return {}; }

  void paintEvent(QPainter* p) override {
    if(!isVisible())return;
    for(auto*w:_panels)if(w->isVisible()){ QPainter cp(w);w->paintEvent(&cp); }
    // Draw handles
    p->setBrush(QBrush(QColor(60,60,60)));p->setPen(QPen::NoPen);
    if(_orientation==Qt::Horizontal){
      int x=_x;
      for(int i=0;i<(int)_panels.size()-1;i++){
        x+=_sizes[i];
        p->drawRect(QRect{x,_y,_handleW,_h});
        x+=_handleW;
      }
    } else {
      int y=_y;
      for(int i=0;i<(int)_panels.size()-1;i++){
        y+=_sizes[i];
        p->drawRect(QRect{_x,y,_w,_handleW});
        y+=_handleW;
      }
    }
  }

  bool handleTouch(int tx,int ty) override {
    if(!isVisible()||!isEnabled())return false;
    for(auto*w:_panels)if(w->handleTouch(tx,ty))return true;
    return contains(tx,ty);
  }

  // Signals: splitterMoved(int,int)
private:
  QList<QWidget*> _panels;
  QList<int> _sizes;
  QList<uint8_t> _collapsible;  // uint8_t avoids std::vector<bool> specialization
  Qt::Orientation _orientation=Qt::Horizontal;
  int _handleW=4;

  void _reflow(){
    if(_panels.empty())return;
    bool h=_orientation==Qt::Horizontal;
    int total=h?_w:_h, n=_panels.size();
    int each=(total-(n-1)*_handleW)/max(1,n);
    while((int)_sizes.size()<n)_sizes.push_back(each);
    while((int)_collapsible.size()<n)_collapsible.push_back((uint8_t)1);
    int pos=h?_x:_y;
    for(int i=0;i<n;i++){
      if(_sizes[i]<=0)_sizes[i]=each;
      if(h) _panels[i]->setGeometry(pos,_y,_sizes[i],_h);
      else  _panels[i]->setGeometry(_x,pos,_w,_sizes[i]);
      pos+=_sizes[i]+_handleW;
    }
  }
};

// ── QStackedWidget ────────────────────────────────────────────────────────────
class QStackedWidget : public QWidget {
public:
  explicit QStackedWidget(QWidget* parent=nullptr) : QWidget(parent) {}
  const char* metaClassName() const override { return "QStackedWidget"; }

  int  addWidget(QWidget* w)           { _pages.push_back(w);if(_pages.size()==1)setCurrentIndex(0);return _pages.size()-1; }
  void insertWidget(int idx,QWidget* w){ int clamp=idx<0?0:idx>(int)_pages.size()?(int)_pages.size():idx; _pages.insert(clamp,w); }
  void removeWidget(QWidget* w)        { _pages.erase(std::remove(_pages.begin(),_pages.end(),w),_pages.end()); }
  QWidget* widget(int idx)       const { return idx>=0&&idx<(int)_pages.size()?_pages[idx]:nullptr; }
  QWidget* currentWidget()       const { return widget(_current); }
  int  currentIndex()            const { return _current; }
  int  count()                   const { return _pages.size(); }
  int  indexOf(QWidget* w)       const { for(int i=0;i<(int)_pages.size();i++)if(_pages[i]==w)return i;return -1; }

  void setCurrentIndex(int idx){
    if(idx<0||idx>=(int)_pages.size())return;
    if(_current>=0&&_current<(int)_pages.size())_pages[_current]->hide();
    _current=idx; _pages[_current]->show(); update();
    emit_signal("currentChanged",{QVariant(idx)});
  }
  void setCurrentWidget(QWidget* w){ setCurrentIndex(indexOf(w)); }

  void paintEvent(QPainter* p) override {
    if(!isVisible())return;
    p->fillRect(QRect{_x,_y,_w,_h},_palette.color(QPalette::Window));
    if(_current>=0&&_current<(int)_pages.size()&&_pages[_current]->isVisible()){
      _pages[_current]->setGeometry(_x,_y,_w,_h);
      QPainter cp(_pages[_current]); _pages[_current]->paintEvent(&cp);
    }
  }
  bool handleTouch(int tx,int ty) override {
    if(!isVisible()||!isEnabled())return false;
    if(_current>=0&&_current<(int)_pages.size())return _pages[_current]->handleTouch(tx,ty);
    return false;
  }
  // Signals: currentChanged(int), widgetRemoved(int)
private:
  QList<QWidget*> _pages;
  int _current=-1;
};

} // namespace NoorQt

using NoorQt::QAbstractButton;
using NoorQt::QPushButton;
using NoorQt::QLabel;
using NoorQt::QLineEdit;
using NoorQt::QTextEdit;
using NoorQt::QPlainTextEdit;
using NoorQt::QCheckBox;
using NoorQt::QRadioButton;
using NoorQt::QAbstractSlider;
using NoorQt::QSlider;
using NoorQt::QSpinBox;
using NoorQt::QDoubleSpinBox;
using NoorQt::QProgressBar;
using NoorQt::QComboBox;
using NoorQt::QListWidgetItem;
using NoorQt::QListWidget;
using NoorQt::QTabWidget;
using NoorQt::QScrollArea;
using NoorQt::QGroupBox;
using NoorQt::QToolButton;
using NoorQt::QDial;
using NoorQt::QLCDNumber;
using NoorQt::QFrame;
using NoorQt::QSplitter;
using NoorQt::QStackedWidget;
