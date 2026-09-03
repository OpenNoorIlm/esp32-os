// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorQt — QLayout.h                                                      ║
// ║  Full Qt6 layout system                                                  ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once
#include "QWidget.h"

// Arduino defines min/max as macros which break C++ template/iterator
// arithmetic inside std::vector. Undefine them here; they'll be
// redefined by subsequent Arduino headers if needed.
#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif
#include <algorithm>
using std::min;
using std::max;

namespace NoorQt {

// ── QLayoutItem ───────────────────────────────────────────────────────────────
class QLayoutItem {
public:
  virtual ~QLayoutItem() {}
  virtual QSize    sizeHint()    const = 0;
  virtual QSize    minimumSize() const = 0;
  virtual QSize    maximumSize() const { return {9999,9999}; }
  virtual void     setGeometry(QRect r) = 0;
  virtual QRect    geometry()    const = 0;
  virtual bool     isEmpty()     const = 0;
  virtual QWidget* widget()            { return nullptr; }
  virtual bool     hasHeightForWidth() const { return false; }
  virtual int      heightForWidth(int) const { return -1; }
  virtual Qt::Orientations expandingDirections() const { return 0; }
  virtual void     invalidate()        {}
};

// ── QWidgetItem ───────────────────────────────────────────────────────────────
class QWidgetItem : public QLayoutItem {
public:
  explicit QWidgetItem(QWidget* w) : _w(w) {}
  QSize  sizeHint()    const override { return _w->sizeHint(); }
  QSize  minimumSize() const override { return _w->minimumSize(); }
  QSize  maximumSize() const override { return _w->maximumSize(); }
  void   setGeometry(QRect r) override { _w->setGeometry(r); }
  QRect  geometry()    const override { return _w->geometry(); }
  bool   isEmpty()     const override { return !_w->isVisible(); }
  QWidget* widget()          override { return _w; }
private:
  QWidget* _w;
};

// ── QSpacerItem ───────────────────────────────────────────────────────────────
class QSpacerItem : public QLayoutItem {
public:
  QSpacerItem(int w,int h,QSizePolicy::Policy hp=QSizePolicy::Minimum,
              QSizePolicy::Policy vp=QSizePolicy::Minimum)
    : _w(w),_h(h),_hp(hp),_vp(vp) {}
  QSize sizeHint()    const override { return {_w,_h}; }
  QSize minimumSize() const override { return {_vp==QSizePolicy::Expanding?0:_w, _hp==QSizePolicy::Expanding?0:_h}; }
  void  setGeometry(QRect r) override { _rect=r; }
  QRect geometry()    const override { return _rect; }
  bool  isEmpty()     const override { return true; }
  void  changeSize(int w,int h,QSizePolicy::Policy hp=QSizePolicy::Minimum,QSizePolicy::Policy vp=QSizePolicy::Minimum)
        { _w=w;_h=h;_hp=hp;_vp=vp; }
  Qt::Orientations expandingDirections() const override {
    Qt::Orientations o=0;
    if(_hp==QSizePolicy::Expanding||_hp==QSizePolicy::MinimumExpanding) o|=Qt::Horizontal;
    if(_vp==QSizePolicy::Expanding||_vp==QSizePolicy::MinimumExpanding) o|=Qt::Vertical;
    return o;
  }
private:
  int _w,_h;
  QSizePolicy::Policy _hp,_vp;
  QRect _rect;
};

// ── QLayout (abstract base) ───────────────────────────────────────────────────
class QLayout : public QObject, public QLayoutItem {
public:
  enum SizeConstraint {
    SetDefaultConstraint, SetNoConstraint, SetMinimumSize,
    SetFixedSize, SetMaximumSize, SetMinAndMaxSize
  };

  explicit QLayout(QWidget* parent=nullptr) : QObject(parent), _parent(parent) {
    if(parent) parent->setLayout(this);
  }
  virtual ~QLayout() { for(auto*i:_items) delete i; }

  // ── Pure virtuals (subclasses implement) ──────────────────────────────────
  virtual void addItem(QLayoutItem* item) = 0;
  virtual int  count()             const = 0;
  virtual QLayoutItem* itemAt(int) const = 0;
  virtual QLayoutItem* takeAt(int)       = 0;

  // ── Widget helpers ────────────────────────────────────────────────────────
  void addWidget(QWidget* w)        { addItem(new QWidgetItem(w)); invalidate(); }
  void removeWidget(QWidget* w)     {
    for(int i=0;i<count();i++) if(itemAt(i)->widget()==w){ delete takeAt(i); break; }
    invalidate();
  }
  void removeItem(QLayoutItem* item){ for(int i=0;i<count();i++) if(itemAt(i)==item){ takeAt(i); break; } }

  // ── Spacing / Margins ─────────────────────────────────────────────────────
  int  spacing()         const { return _spacing; }
  void setSpacing(int s)       { _spacing=s; invalidate(); }
  void setContentsMargins(int l,int t,int r,int b){ _margins=QMargins(l,t,r,b); invalidate(); }
  void setContentsMargins(QMargins m){ _margins=m; invalidate(); }
  void getContentsMargins(int*l,int*t,int*r,int*b)const{ *l=_margins.left();*t=_margins.top();*r=_margins.right();*b=_margins.bottom(); }
  QMargins contentsMargins() const { return _margins; }
  QRect contentsRect()       const { return _geometry.marginsRemoved(_margins); }

  // ── Size constraint ───────────────────────────────────────────────────────
  void setSizeConstraint(SizeConstraint c){ _constraint=c; }
  SizeConstraint sizeConstraint()   const { return _constraint; }

  // ── Activation / invalidation ─────────────────────────────────────────────
  virtual void invalidate() override { _dirty=true; if(_parent)_parent->update(); }
  virtual void activate()   { if(_dirty){ _dirty=false; doLayout(_parent?_parent->contentsRect():_geometry); } }
  bool isEnabled()    const { return _enabled; }
  void setEnabled(bool e)   { _enabled=e; }

  // ── QLayoutItem impl ──────────────────────────────────────────────────────
  void     setGeometry(QRect r) override { _geometry=r; doLayout(r.marginsRemoved(_margins)); }
  QRect    geometry()    const override  { return _geometry; }
  bool     isEmpty()     const override  { return count()==0; }
  QSize    sizeHint()    const override  { return minimumSize(); }
  QSize    minimumSize() const override  { return {100,50}; }

  // ── Subclasses implement this ─────────────────────────────────────────────
  virtual void doLayout(QRect rect) = 0;

  // ── Alignment ─────────────────────────────────────────────────────────────
  Qt::Alignment alignment() const { return _alignment; }
  void setAlignment(Qt::Alignment a){ _alignment=a; }
  bool setAlignment(QWidget* w,Qt::Alignment a){ return false; }
  bool setAlignment(QLayout* l,Qt::Alignment a){ return false; }

  QWidget* parentWidget() const { return _parent; }

protected:
  QWidget* _parent  = nullptr;
  int _spacing      = 4;
  QMargins _margins{4,4,4,4};
  SizeConstraint _constraint = SetDefaultConstraint;
  bool _dirty   = true;
  bool _enabled = true;
  QRect _geometry;
  Qt::Alignment _alignment = Qt::AlignLeft|Qt::AlignTop;
  QList<QLayoutItem*> _items;
};

// ── QBoxLayout ────────────────────────────────────────────────────────────────
class QBoxLayout : public QLayout {
public:
  enum Direction { LeftToRight, RightToLeft, TopToBottom, BottomToTop };

  explicit QBoxLayout(Direction dir,QWidget* parent=nullptr)
    : QLayout(parent), _dir(dir) {}

  Direction direction() const { return _dir; }
  void setDirection(Direction d){ _dir=d; invalidate(); }
  void reverse()                { _dir=(_dir==LeftToRight?RightToLeft:_dir==RightToLeft?LeftToRight:_dir==TopToBottom?BottomToTop:TopToBottom); invalidate(); }

  // addWidget with stretch and alignment
  void addWidget(QWidget* w,int stretch=0,Qt::Alignment align=Qt::AlignLeft){
    Entry e; e.item=new QWidgetItem(w); e.stretch=stretch; e.align=align;
    _entries.push_back(e); invalidate();
  }
  void insertWidget(int idx,QWidget* w,int stretch=0,Qt::Alignment align=Qt::AlignLeft){
    Entry e; e.item=new QWidgetItem(w); e.stretch=stretch; e.align=align;
    int clamp=idx<0?0:idx>(int)_entries.size()?(int)_entries.size():idx;
    _entries.insert(clamp,e); invalidate();
  }

  void addLayout(QLayout* l,int stretch=0){
    Entry e; e.item=l; e.stretch=stretch;
    _entries.push_back(e); invalidate();
  }
  void insertLayout(int idx,QLayout* l,int stretch=0){
    Entry e; e.item=l; e.stretch=stretch;
    int clamp=idx<0?0:idx>(int)_entries.size()?(int)_entries.size():idx;
    _entries.insert(clamp,e); invalidate();
  }

  void addSpacing(int size)           { addItem(new QSpacerItem(isHoriz()?size:0,isHoriz()?0:size)); }
  void insertSpacing(int idx,int size){ insertItem(idx,new QSpacerItem(isHoriz()?size:0,isHoriz()?0:size)); }
  void addStretch(int stretch=0)      { addItem(new QSpacerItem(0,0,isHoriz()?QSizePolicy::Expanding:QSizePolicy::Minimum,isHoriz()?QSizePolicy::Minimum:QSizePolicy::Expanding)); _entries.back().stretch=max(1,stretch); }
  void insertStretch(int idx,int s=0) { insertItem(idx,new QSpacerItem(0,0,QSizePolicy::Expanding,QSizePolicy::Expanding)); }
  void addSpacerItem(QSpacerItem* s)  { addItem(s); }
  void insertSpacerItem(int idx,QSpacerItem* s){ insertItem(idx,s); }

  void addItem(QLayoutItem* item) override {
    Entry e; e.item=item; _entries.push_back(e); invalidate();
  }
  void insertItem(int idx,QLayoutItem* item){
    Entry e; e.item=item;
    int clamp=idx<0?0:idx>(int)_entries.size()?(int)_entries.size():idx;
    _entries.insert(clamp,e); invalidate();
  }

  int  count() const override { return _entries.size(); }
  QLayoutItem* itemAt(int i) const override { return i>=0&&i<(int)_entries.size()?_entries[i].item:nullptr; }
  QLayoutItem* takeAt(int i) override {
    if(i<0||i>=(int)_entries.size())return nullptr;
    auto* item=_entries[i].item; _entries.erase(_entries.begin()+i); invalidate(); return item;
  }

  void setStretch(int idx,int stretch){ if(idx>=0&&idx<(int)_entries.size())_entries[idx].stretch=stretch; invalidate(); }
  int  stretch(int idx) const { return idx>=0&&idx<(int)_entries.size()?_entries[idx].stretch:0; }
  void setStretchFactor(QWidget* w,int s){ for(auto&e:_entries)if(e.item->widget()==w){e.stretch=s;break;} invalidate(); }
  void setStretchFactor(QLayout* l,int s){ for(auto&e:_entries)if(e.item==l){e.stretch=s;break;} invalidate(); }

  QSize sizeHint() const override { return minimumSize(); }
  QSize minimumSize() const override {
    int w=_margins.left()+_margins.right(), h=_margins.top()+_margins.bottom();
    for(auto&e:_entries){
      auto s=e.item->sizeHint();
      if(isHoriz()){w+=s.width()+_spacing;h=max(h,s.height());}
      else{h+=s.height()+_spacing;w=max(w,s.width());}
    }
    return{w,h};
  }

  void doLayout(QRect rect) override {
    int n=_entries.size(); if(n==0)return;
    bool horiz=isHoriz();
    int totalSpace=horiz?rect.width():rect.height();
    int totalSpacing=_spacing*(n-1);
    int fixedSize=0; int totalStretch=0;
    for(auto&e:_entries){
      if(e.stretch==0){ auto s=e.item->sizeHint(); fixedSize+=horiz?s.width():s.height(); }
      else totalStretch+=e.stretch;
    }
    int stretchSpace=max(0,totalSpace-fixedSize-totalSpacing);
    int pos=horiz?rect.x():rect.y();
    for(auto&e:_entries){
      auto hint=e.item->sizeHint();
      int size=e.stretch>0?(totalStretch>0?stretchSpace*e.stretch/totalStretch:0):(horiz?hint.width():hint.height());
      QRect r;
      if(horiz) r={pos,rect.y(),size,rect.height()};
      else      r={rect.x(),pos,rect.width(),size};
      e.item->setGeometry(r);
      pos+=size+_spacing;
    }
  }

  // Qt::Horizontal/Vertical
  Qt::Orientations expandingDirections() const override {
    return isHoriz()?Qt::Horizontal:Qt::Vertical;
  }

private:
  struct Entry { QLayoutItem* item=nullptr; int stretch=0; Qt::Alignment align=Qt::AlignLeft; };
  QList<Entry> _entries;
  Direction _dir;
  bool isHoriz() const { return _dir==LeftToRight||_dir==RightToLeft; }
};

// ── QHBoxLayout ───────────────────────────────────────────────────────────────
class QHBoxLayout : public QBoxLayout {
public:
  explicit QHBoxLayout(QWidget* parent=nullptr) : QBoxLayout(LeftToRight,parent) {}
};

// ── QVBoxLayout ───────────────────────────────────────────────────────────────
class QVBoxLayout : public QBoxLayout {
public:
  explicit QVBoxLayout(QWidget* parent=nullptr) : QBoxLayout(TopToBottom,parent) {}
};

// ── QGridLayout ───────────────────────────────────────────────────────────────
class QGridLayout : public QLayout {
public:
  explicit QGridLayout(QWidget* parent=nullptr) : QLayout(parent) {}

  void addWidget(QWidget* w,int row,int col,Qt::Alignment align=Qt::AlignLeft){
    addWidget(w,row,col,1,1,align);
  }
  void addWidget(QWidget* w,int row,int col,int rowSpan,int colSpan,Qt::Alignment align=Qt::AlignLeft){
    Cell c; c.item=new QWidgetItem(w); c.row=row;c.col=col;c.rowSpan=rowSpan;c.colSpan=colSpan;c.align=align;
    _cells.push_back(c);
    _rows=max(_rows,row+rowSpan);
    _cols=max(_cols,col+colSpan);
    invalidate();
  }
  void addLayout(QLayout* l,int row,int col,int rowSpan=1,int colSpan=1,Qt::Alignment align=Qt::AlignLeft){
    Cell c; c.item=l; c.row=row;c.col=col;c.rowSpan=rowSpan;c.colSpan=colSpan;c.align=align;
    _cells.push_back(c);
    _rows=max(_rows,row+rowSpan);
    _cols=max(_cols,col+colSpan);
    invalidate();
  }
  void addItem(QLayoutItem* item) override { Cell c; c.item=item; c.row=_rows;c.col=0; _cells.push_back(c); _rows++; invalidate(); }

  int  rowCount()    const { return _rows; }
  int  columnCount() const { return _cols; }
  int  count()       const override { return _cells.size(); }
  QLayoutItem* itemAt(int i)  const override { return i>=0&&i<(int)_cells.size()?_cells[i].item:nullptr; }
  QLayoutItem* takeAt(int i)        override { if(i<0||i>=(int)_cells.size())return nullptr; auto*it=_cells[i].item;_cells.erase(_cells.begin()+i);invalidate();return it; }
  QLayoutItem* itemAtPosition(int r,int c)const{ for(auto&cell:_cells)if(cell.row==r&&cell.col==c)return cell.item;return nullptr; }

  void setRowStretch(int row,int stretch)   { while((int)_rowStretch.size()<=row)_rowStretch.push_back(0); _rowStretch[row]=stretch; invalidate(); }
  void setColumnStretch(int col,int stretch){ while((int)_colStretch.size()<=col)_colStretch.push_back(0); _colStretch[col]=stretch; invalidate(); }
  int  rowStretch(int r)    const { return r<(int)_rowStretch.size()?_rowStretch[r]:0; }
  int  columnStretch(int c) const { return c<(int)_colStretch.size()?_colStretch[c]:0; }
  void setRowMinimumHeight(int r,int h){ while((int)_rowMin.size()<=r)_rowMin.push_back(0); _rowMin[r]=h; invalidate(); }
  void setColumnMinimumWidth(int c,int w){ while((int)_colMin.size()<=c)_colMin.push_back(0); _colMin[c]=w; invalidate(); }
  int  rowMinimumHeight(int r)    const { return r<(int)_rowMin.size()?_rowMin[r]:0; }
  int  columnMinimumWidth(int c)  const { return c<(int)_colMin.size()?_colMin[c]:0; }
  void setHorizontalSpacing(int s){ _hSpacing=s; invalidate(); }
  void setVerticalSpacing(int s)  { _vSpacing=s; invalidate(); }
  int  horizontalSpacing() const  { return _hSpacing; }
  int  verticalSpacing()   const  { return _vSpacing; }
  void setSpacing(int s)          { _hSpacing=_vSpacing=s; invalidate(); }

  QSize sizeHint() const override { return minimumSize(); }
  QSize minimumSize() const override {
    int cellW=(_geometry.width()-_margins.left()-_margins.right()-(_cols-1)*_hSpacing)/max(1,_cols);
    int cellH=(_geometry.height()-_margins.top()-_margins.bottom()-(_rows-1)*_vSpacing)/max(1,_rows);
    return{cellW*_cols+(_cols-1)*_hSpacing,cellH*_rows+(_rows-1)*_vSpacing};
  }

  void doLayout(QRect rect) override {
    if(_rows==0||_cols==0)return;
    int cellW=(rect.width()-(_cols-1)*_hSpacing)/max(1,_cols);
    int cellH=(rect.height()-(_rows-1)*_vSpacing)/max(1,_rows);
    // Apply column/row stretches
    QList<int> colWidths(_cols,cellW), rowHeights(_rows,cellH);
    int totalColStretch=0,totalRowStretch=0;
    for(int c=0;c<_cols;c++) totalColStretch+=columnStretch(c);
    for(int r=0;r<_rows;r++) totalRowStretch+=rowStretch(r);
    if(totalColStretch>0){
      int extra=rect.width()-(_cols-1)*_hSpacing;
      for(int c=0;c<_cols;c++) colWidths[c]=extra*max(1,columnStretch(c))/max(1,totalColStretch);
    }
    if(totalRowStretch>0){
      int extra=rect.height()-(_rows-1)*_vSpacing;
      for(int r=0;r<_rows;r++) rowHeights[r]=extra*max(1,rowStretch(r))/max(1,totalRowStretch);
    }
    // Position cells
    QList<int> colX(_cols,0),rowY(_rows,0);
    colX[0]=rect.x(); for(int c=1;c<_cols;c++) colX[c]=colX[c-1]+colWidths[c-1]+_hSpacing;
    rowY[0]=rect.y(); for(int r=1;r<_rows;r++) rowY[r]=rowY[r-1]+rowHeights[r-1]+_vSpacing;
    for(auto&cell:_cells){
      int x=colX[cell.col],y=rowY[cell.row];
      int w=0,h=0;
      for(int c=cell.col;c<cell.col+cell.colSpan&&c<_cols;c++) w+=colWidths[c]+(c>cell.col?_hSpacing:0);
      for(int r=cell.row;r<cell.row+cell.rowSpan&&r<_rows;r++) h+=rowHeights[r]+(r>cell.row?_vSpacing:0);
      cell.item->setGeometry({x,y,w,h});
    }
  }

private:
  struct Cell { QLayoutItem* item=nullptr; int row=0,col=0,rowSpan=1,colSpan=1; Qt::Alignment align=Qt::AlignLeft; };
  QList<Cell> _cells;
  int _rows=0,_cols=0;
  int _hSpacing=4,_vSpacing=4;
  QList<int> _rowStretch,_colStretch,_rowMin,_colMin;
};

// ── QFormLayout ───────────────────────────────────────────────────────────────
class QFormLayout : public QLayout {
public:
  enum FieldGrowthPolicy { FieldsStayAtSizeHint, ExpandingFieldsGrow, AllNonFixedFieldsGrow };
  enum LabelAlignment    { LabelAlignLeft=Qt::AlignLeft, LabelAlignRight=Qt::AlignRight, LabelAlignHCenter=Qt::AlignHCenter };
  enum RowWrapPolicy     { DontWrapRows, WrapLongRows, WrapAllRows };
  enum ItemRole          { LabelRole=0, FieldRole=1, SpanningRole=2 };

  explicit QFormLayout(QWidget* parent=nullptr) : QLayout(parent) {}

  void addRow(const QString& label,QWidget* field){ addRow(new QLabel2(label),field); }
  void addRow(const QString& label,QLayout* field){ Row r; r.labelStr=label; r.field=field; _rows.push_back(r); invalidate(); }
  void addRow(QWidget* label,QWidget* field)      { Row r; r.labelW=label; r.fieldW=field; _rows.push_back(r); invalidate(); }
  void addRow(QWidget* label,QLayout* field)      { Row r; r.labelW=label; r.field=field; _rows.push_back(r); invalidate(); }
  void addRow(QWidget* w)                          { Row r; r.fieldW=w; r.spanning=true; _rows.push_back(r); invalidate(); }
  void addRow(QLayout* l)                          { Row r; r.field=l; r.spanning=true; _rows.push_back(r); invalidate(); }

  void insertRow(int idx,const QString& label,QWidget* field){ Row r; r.labelStr=label; r.fieldW=field; int clamp=idx<0?0:idx>(int)_rows.size()?(int)_rows.size():idx; _rows.insert(clamp,r); invalidate(); }
  void removeRow(int idx){ if(idx>=0&&idx<(int)_rows.size())_rows.erase(_rows.begin()+idx); invalidate(); }

  int  rowCount() const { return _rows.size(); }
  int  count()    const override { return _rows.size()*2; }
  QLayoutItem* itemAt(int i)  const override { return nullptr; } // stub
  QLayoutItem* takeAt(int i)        override { return nullptr; } // stub
  void addItem(QLayoutItem*)        override {}

  void setLabelAlignment(Qt::Alignment a){ _labelAlign=a; invalidate(); }
  void setFormAlignment(Qt::Alignment a) { _formAlign=a; invalidate(); }
  void setFieldGrowthPolicy(FieldGrowthPolicy p){ _growPolicy=p; invalidate(); }
  void setRowWrapPolicy(RowWrapPolicy p)  { _wrapPolicy=p; invalidate(); }
  void setHorizontalSpacing(int s)        { _hSpacing=s; invalidate(); }
  void setVerticalSpacing(int s)          { _vSpacing=s; invalidate(); }
  int  horizontalSpacing() const          { return _hSpacing; }
  int  verticalSpacing()   const          { return _vSpacing; }
  Qt::Alignment labelAlignment() const    { return _labelAlign; }

  QSize sizeHint()    const override { return {240,(int)_rows.size()*(_rowH+_vSpacing)}; }
  QSize minimumSize() const override { return sizeHint(); }

  void doLayout(QRect rect) override {
    int labelW=80, fieldW=rect.width()-labelW-_hSpacing-_margins.left()-_margins.right();
    int y=rect.y()+_margins.top();
    for(auto&r:_rows){
      if(r.spanning){
        if(r.fieldW) r.fieldW->setGeometry({rect.x()+_margins.left(),y,rect.width()-_margins.left()-_margins.right(),_rowH});
        if(r.field)  r.field->setGeometry({rect.x()+_margins.left(),y,rect.width()-_margins.left()-_margins.right(),_rowH});
      } else {
        int lx=rect.x()+_margins.left();
        if(r.labelW) r.labelW->setGeometry({lx,y,labelW,_rowH});
        int fx=lx+labelW+_hSpacing;
        if(r.fieldW) r.fieldW->setGeometry({fx,y,fieldW,_rowH});
        if(r.field)  r.field->setGeometry({fx,y,fieldW,_rowH});
      }
      y+=_rowH+_vSpacing;
    }
  }

private:
  struct Row {
    QString labelStr;
    QWidget* labelW=nullptr;
    QWidget* fieldW=nullptr;
    QLayout* field=nullptr;
    bool spanning=false;
  };
  QList<Row> _rows;
  int _rowH=28, _hSpacing=8, _vSpacing=4;
  Qt::Alignment _labelAlign=Qt::AlignRight;
  Qt::Alignment _formAlign=Qt::AlignLeft|Qt::AlignTop;
  FieldGrowthPolicy _growPolicy=ExpandingFieldsGrow;
  RowWrapPolicy _wrapPolicy=DontWrapRows;

  // Stub QLabel for string labels
  struct QLabel2 : QWidget {
    QString text;
    QLabel2(const QString& t):text(t){}
    void paintEvent(QPainter* p) override {
      p->setPen(QColor(200,200,200));
      p->setFont(QFont("default",8));
      p->drawText(geometry(),Qt::AlignRight|Qt::AlignVCenter,text);
    }
  };
};

// ── QStackedLayout ────────────────────────────────────────────────────────────
class QStackedLayout : public QLayout {
public:
  enum StackingMode { StackOne, StackAll };

  explicit QStackedLayout(QWidget* parent=nullptr) : QLayout(parent) {}
  explicit QStackedLayout(QLayout* parentLayout) : QLayout() { parentLayout->addItem(this); }

  void addItem(QLayoutItem* item) override { _items.push_back(item); invalidate(); }
  void addWidget(QWidget* w)              { addItem(new QWidgetItem(w)); }
  void insertWidget(int idx,QWidget* w)   {
    int clamp=idx<0?0:idx>(int)_items.size()?(int)_items.size():idx;
    _items.insert(clamp,new QWidgetItem(w));
    invalidate();
  }
  void removeWidget(QWidget* w)           {
    for(int i=0;i<(int)_items.size();i++) if(_items[i]->widget()==w){ delete _items[i]; _items.erase(_items.begin()+i); break; }
    invalidate();
  }

  int  currentIndex() const { return _current; }
  QWidget* currentWidget() const { return _current>=0&&_current<(int)_items.size()?_items[_current]->widget():nullptr; }
  void setCurrentIndex(int idx){
    if(idx<0||idx>=(int)_items.size())return;
    if(_mode==StackOne&&_current>=0&&_current<(int)_items.size()&&_items[_current]->widget())
      _items[_current]->widget()->hide();
    _current=idx;
    if(_items[_current]->widget())_items[_current]->widget()->show();
    emit_signal("currentChanged",{QVariant(idx)});
  }
  void setCurrentWidget(QWidget* w){
    for(int i=0;i<(int)_items.size();i++) if(_items[i]->widget()==w){ setCurrentIndex(i); return; }
  }

  void setStackingMode(StackingMode m){ _mode=m; invalidate(); }
  StackingMode stackingMode() const   { return _mode; }

  int  count()          const override { return _items.size(); }
  QLayoutItem* itemAt(int i)  const override { return i>=0&&i<(int)_items.size()?_items[i]:nullptr; }
  QLayoutItem* takeAt(int i)        override {
    if(i<0||i>=(int)_items.size())return nullptr;
    auto*it=_items[i];_items.erase(_items.begin()+i);
    if(_current>=i)_current=max(0,_current-1);
    invalidate();return it;
  }

  QSize sizeHint() const override {
    QSize s{0,0};
    for(auto*i:_items)s=s.expandedTo(i->sizeHint());
    return s;
  }
  QSize minimumSize() const override { return sizeHint(); }

  void doLayout(QRect rect) override {
    for(int i=0;i<(int)_items.size();i++){
      _items[i]->setGeometry(rect);
      if(_mode==StackOne&&_items[i]->widget()){
        if(i==_current)_items[i]->widget()->show();
        else _items[i]->widget()->hide();
      }
    }
  }

  // Signals: currentChanged(int), widgetRemoved(int)
private:
  int _current=0;
  StackingMode _mode=StackOne;
};

} // namespace NoorQt

using NoorQt::QLayout;
using NoorQt::QBoxLayout;
using NoorQt::QHBoxLayout;
using NoorQt::QVBoxLayout;
using NoorQt::QGridLayout;
using NoorQt::QFormLayout;
using NoorQt::QStackedLayout;
using NoorQt::QSpacerItem;
using NoorQt::QWidgetItem;
