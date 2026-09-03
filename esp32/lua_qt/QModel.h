// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorQt — QModel.h                                                       ║
// ║  Item models — mirrors Qt6 QtCore model/view API exactly.                ║
// ║  Backed by std::vector / std::map (no Qt Model/View framework needed).  ║
// ║  MIT License — NoorRobot / OpenNoorIlm                                  ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once
#include "QObject.h"
#include <vector>
#include <functional>

namespace NoorQt {

// ── QModelIndex ───────────────────────────────────────────────────────────────
class QModelIndex {
public:
  QModelIndex() {}
  QModelIndex(int row, int col, void* ptr=nullptr)
    : _row(row), _col(col), _ptr(ptr), _valid(true) {}

  int   row()     const { return _row; }
  int   column()  const { return _col; }
  bool  isValid() const { return _valid; }
  void* internalPointer() const { return _ptr; }

  bool operator==(const QModelIndex& o) const {
    return _row==o._row && _col==o._col && _ptr==o._ptr;
  }
  bool operator!=(const QModelIndex& o) const { return !(*this==o); }

private:
  int   _row=-1, _col=-1;
  void* _ptr=nullptr;
  bool  _valid=false;
};


// ── QAbstractItemModel ────────────────────────────────────────────────────────
class QAbstractItemModel : public QObject {
public:
  Signal<QModelIndex,QModelIndex> dataChanged     {"dataChanged"};
  Signal<QModelIndex,int,int>     rowsInserted    {"rowsInserted"};
  Signal<QModelIndex,int,int>     rowsRemoved     {"rowsRemoved"};
  Signal<QModelIndex,int,int>     columnsInserted {"columnsInserted"};
  Signal<QModelIndex,int,int>     columnsRemoved  {"columnsRemoved"};
  Signal<void>                    modelReset      {"modelReset"};
  Signal<void>                    layoutChanged   {"layoutChanged"};

  explicit QAbstractItemModel(QObject* parent=nullptr) : QObject(parent) {}

  virtual int      rowCount(const QModelIndex& parent=QModelIndex())    const = 0;
  virtual int      columnCount(const QModelIndex& parent=QModelIndex()) const = 0;
  virtual QVariant data(const QModelIndex& idx, int role=Qt::DisplayRole) const = 0;
  virtual QModelIndex index(int row, int col, const QModelIndex& parent=QModelIndex()) const = 0;
  virtual QModelIndex parent(const QModelIndex&) const { return QModelIndex(); }

  virtual bool setData(const QModelIndex&, const QVariant&, int=Qt::EditRole){ return false; }
  virtual QVariant headerData(int, int /*orientation*/, int role=Qt::DisplayRole) const {
    (void)role; return QVariant();
  }
  virtual bool setHeaderData(int,int,const QVariant&,int=Qt::EditRole){ return false; }

  virtual bool insertRows(int row, int count, const QModelIndex& parent=QModelIndex()) {
    (void)row;(void)count;(void)parent; return false;
  }
  virtual bool removeRows(int row, int count, const QModelIndex& parent=QModelIndex()) {
    (void)row;(void)count;(void)parent; return false;
  }
  virtual bool insertColumns(int,int,const QModelIndex& =QModelIndex()){ return false; }
  virtual bool removeColumns(int,int,const QModelIndex& =QModelIndex()){ return false; }

  virtual void sort(int col, Qt::SortOrder order=Qt::AscendingOrder){ (void)col;(void)order; }

  QModelIndex createIndex(int row, int col, void* ptr=nullptr) const {
    return QModelIndex(row,col,ptr);
  }

  void beginResetModel() {}
  void endResetModel()   { modelReset.emit(); }
  void beginInsertRows(const QModelIndex& p, int first, int last) {
    (void)p;(void)first;(void)last;
  }
  void endInsertRows()   {}
  void beginRemoveRows(const QModelIndex& p, int first, int last) {
    (void)p;(void)first;(void)last;
  }
  void endRemoveRows()   {}
  void dataChangedEmit(const QModelIndex& tl, const QModelIndex& br) {
    dataChanged.emit(tl,br);
  }
};

// ── QStandardItem ─────────────────────────────────────────────────────────────
class QStandardItem {
public:
  QStandardItem() {}
  explicit QStandardItem(const QString& text) { setData(QVariant(text),Qt::DisplayRole); }
  QStandardItem(const QString& text, const QString& icon) {
    setData(QVariant(text),Qt::DisplayRole);
    setData(QVariant(icon),Qt::DecorationRole);
  }

  QString  text()          const { return data(Qt::DisplayRole).toString(); }
  void     setText(const QString& t){ setData(QVariant(t),Qt::DisplayRole); }

  QString  toolTip()       const { return data(Qt::ToolTipRole).toString(); }
  void     setToolTip(const QString& t){ setData(QVariant(t),Qt::ToolTipRole); }

  bool     isCheckable()   const { return _flags & Qt::ItemIsCheckable; }
  void     setCheckable(bool c)  { _flags=c?_flags|Qt::ItemIsCheckable:_flags&~Qt::ItemIsCheckable; }
  Qt::CheckState checkState() const { return (Qt::CheckState)data(Qt::CheckStateRole).toInt(); }
  void setCheckState(Qt::CheckState s){ setData(QVariant((int)s),Qt::CheckStateRole); }

  bool     isEnabled()     const { return _flags & Qt::ItemIsEnabled; }
  void     setEnabled(bool e)    { _flags=e?_flags|Qt::ItemIsEnabled:_flags&~Qt::ItemIsEnabled; }
  bool     isEditable()    const { return _flags & Qt::ItemIsEditable; }
  void     setEditable(bool e)   { _flags=e?_flags|Qt::ItemIsEditable:_flags&~Qt::ItemIsEditable; }
  bool     isSelectable()  const { return _flags & Qt::ItemIsSelectable; }
  void     setSelectable(bool s) { _flags=s?_flags|Qt::ItemIsSelectable:_flags&~Qt::ItemIsSelectable; }

  QVariant data(int role=Qt::DisplayRole) const {
    auto it=_data.find(role); return it!=_data.end()?it->second:QVariant();
  }
  void setData(const QVariant& v, int role=Qt::UserRole){ _data[role]=v; }

  // Child items
  void appendRow(QStandardItem* child) { _children.push_back(child); child->_parent=this; }
  void appendRow(std::initializer_list<QStandardItem*> items) {
    for(auto* i:items) appendRow(i);
  }
  void insertRow(int row, QStandardItem* child) {
    if(row<=(int)_children.size()) { _children.insert(_children.begin()+row,child); child->_parent=this; }
  }
  void removeRow(int row) {
    if(row<(int)_children.size()){ delete _children[row]; _children.erase(_children.begin()+row); }
  }

  int           rowCount()     const { return (int)_children.size(); }
  int           columnCount()  const { return _colCount>0?_colCount:1; }
  QStandardItem* child(int row, int col=0) const {
    (void)col; return row<(int)_children.size()?_children[row]:nullptr;
  }
  QStandardItem* parent()      const { return _parent; }
  int            row()         const {
    if(!_parent) return -1;
    for(int i=0;i<(int)_parent->_children.size();i++)
      if(_parent->_children[i]==this) return i;
    return -1;
  }

  QStandardItem* clone()       const { auto* c=new QStandardItem(); c->_data=_data; return c; }

private:
  std::map<int,QVariant>      _data;
  int                         _flags   = Qt::ItemIsSelectable|Qt::ItemIsEnabled;
  std::vector<QStandardItem*> _children;
  QStandardItem*              _parent  = nullptr;
  int                         _colCount= 1;
};

// ── QStandardItemModel ────────────────────────────────────────────────────────
class QStandardItemModel : public QAbstractItemModel {
public:
  explicit QStandardItemModel(QObject* parent=nullptr)
    : QAbstractItemModel(parent), _root(new QStandardItem()) {}
  QStandardItemModel(int rows, int cols, QObject* parent=nullptr)
    : QAbstractItemModel(parent), _root(new QStandardItem()) {
    for(int r=0;r<rows;r++){
      std::vector<QStandardItem*> row;
      for(int c=0;c<cols;c++) row.push_back(new QStandardItem());
      _rows.push_back(row);
    }
    _cols=cols;
  }
  ~QStandardItemModel() { clear(); delete _root; }

  // ── Row/column access ─────────────────────────────────────────────────────
  int rowCount(const QModelIndex& parent=QModelIndex()) const override {
    if(!parent.isValid()) return (int)_rows.size();
    auto* item=itemFromIndex(parent);
    return item?item->rowCount():0;
  }
  int columnCount(const QModelIndex& parent=QModelIndex()) const override {
    (void)parent; return _cols>0?_cols:1;
  }

  QModelIndex index(int row, int col, const QModelIndex& parent=QModelIndex()) const override {
    if(!parent.isValid()){
      if(row<(int)_rows.size()&&col<(int)_rows[row].size())
        return createIndex(row,col,_rows[row][col]);
    } else {
      auto* p=itemFromIndex(parent);
      if(p&&row<p->rowCount()) return createIndex(row,col,p->child(row,col));
    }
    return QModelIndex();
  }

  QVariant data(const QModelIndex& idx, int role=Qt::DisplayRole) const override {
    auto* item=itemFromIndex(idx);
    return item?item->data(role):QVariant();
  }

  bool setData(const QModelIndex& idx, const QVariant& v, int role=Qt::EditRole) override {
    auto* item=itemFromIndex(idx);
    if(!item) return false;
    item->setData(v,role);
    dataChanged.emit(idx,idx);
    return true;
  }

  QVariant headerData(int section, int orientation, int role=Qt::DisplayRole) const override {
    if(role!=Qt::DisplayRole) return QVariant();
    auto& hdrs = orientation==1 ? _hHdrs : _vHdrs; // 1=Horizontal,2=Vertical
    return section<(int)hdrs.size()?QVariant(hdrs[section]):QVariant(section+1);
  }
  bool setHeaderData(int section, int orientation, const QVariant& v, int=Qt::EditRole) override {
    auto& hdrs = orientation==1 ? _hHdrs : _vHdrs;
    while((int)hdrs.size()<=section) hdrs.push_back(String());
    hdrs[section]=v.toString(); return true;
  }

  // ── Item access ──────────────────────────────────────────────────────────
  QStandardItem* item(int row, int col=0) const {
    if(row<(int)_rows.size()&&col<(int)_rows[row].size()) return _rows[row][col];
    return nullptr;
  }
  void setItem(int row, int col, QStandardItem* i) {
    while((int)_rows.size()<=row) _rows.push_back({});
    while((int)_rows[row].size()<=col) _rows[row].push_back(new QStandardItem());
    delete _rows[row][col]; _rows[row][col]=i;
    if(col+1>(int)_cols) _cols=col+1;
  }
  void setItem(int row, QStandardItem* i) { setItem(row,0,i); }

  QStandardItem* invisibleRootItem() const { return _root; }

  void appendRow(QStandardItem* i) {
    _rows.push_back({i}); if(_cols<1)_cols=1;
    rowsInserted.emit(QModelIndex(),(int)_rows.size()-1,(int)_rows.size()-1);
  }
  void appendRow(std::initializer_list<QStandardItem*> items) {
    std::vector<QStandardItem*> row(items);
    if((int)row.size()>(int)_cols)_cols=(int)row.size();
    _rows.push_back(row);
    rowsInserted.emit(QModelIndex(),(int)_rows.size()-1,(int)_rows.size()-1);
  }
  void insertRow(int row, QStandardItem* i) {
    if(row<=(int)_rows.size()) _rows.insert(_rows.begin()+row,{i});
    rowsInserted.emit(QModelIndex(),row,row);
  }
  bool removeRow(int row, const QModelIndex& =QModelIndex()) {
    if(row>=(int)_rows.size()) return false;
    for(auto* i:_rows[row]) delete i;
    _rows.erase(_rows.begin()+row);
    rowsRemoved.emit(QModelIndex(),row,row);
    return true;
  }
  bool removeRows(int row, int count, const QModelIndex& parent=QModelIndex()) override {
    for(int i=0;i<count;i++) removeRow(row,parent);
    return true;
  }

  void clear() {
    for(auto& row:_rows) for(auto* i:row) delete i;
    _rows.clear(); _cols=0;
    modelReset.emit();
  }

  // ── Find ──────────────────────────────────────────────────────────────────
  std::vector<QModelIndex> match(const QModelIndex& start, int role,
                                  const QVariant& val, int hits=1,
                                  int flags=Qt::MatchExactly) const {
    std::vector<QModelIndex> results;
    for(int r=start.row();r<(int)_rows.size()&&(hits<0||(int)results.size()<hits);r++){
      for(int c=0;c<(int)_rows[r].size();c++){
        auto* item=_rows[r][c];
        if(!item) continue;
        QString d=item->data(role).toString();
        QString v=val.toString();
        bool match=false;
        if(flags&Qt::MatchContains)    match=d.indexOf(v)>=0;
        else if(flags&Qt::MatchStartsWith) match=d.startsWith(v);
        else if(flags&Qt::MatchEndsWith)   match=d.endsWith(v);
        else                               match=(d==v);
        if(match) results.push_back(createIndex(r,c,item));
      }
    }
    return results;
  }

  QStandardItem* itemFromIndex(const QModelIndex& idx) const {
    if(!idx.isValid()) return nullptr;
    return static_cast<QStandardItem*>(idx.internalPointer());
  }
  QModelIndex indexFromItem(const QStandardItem* item) const {
    for(int r=0;r<(int)_rows.size();r++)
      for(int c=0;c<(int)_rows[r].size();c++)
        if(_rows[r][c]==item) return createIndex(r,c,const_cast<QStandardItem*>(item));
    return QModelIndex();
  }

  void setRowCount(int n) {
    while((int)_rows.size()<n) _rows.push_back({});
    while((int)_rows.size()>n) removeRow((int)_rows.size()-1);
  }
  void setColumnCount(int n) { _cols=n; }

private:
  std::vector<std::vector<QStandardItem*>> _rows;
  int                                      _cols=0;
  QStandardItem*                           _root;
  std::vector<QString>                     _hHdrs, _vHdrs;
};

// ── QSortFilterProxyModel ─────────────────────────────────────────────────────
class QSortFilterProxyModel : public QAbstractItemModel {
public:
  explicit QSortFilterProxyModel(QObject* parent=nullptr) : QAbstractItemModel(parent) {}

  void setSourceModel(QAbstractItemModel* m) {
    _src=m;
    if(m){ m->dataChanged.connect([this](std::vector<QVariant>){ dataChanged.emit(QModelIndex(),QModelIndex()); }); }
    _rebuild();
  }
  QAbstractItemModel* sourceModel() const { return _src; }

  void setFilterFixedString(const QString& s)  { _filter=s; _rebuild(); }
  void setFilterRegularExpression(const QString& s){ _filter=s; _rebuild(); }
  void setFilterKeyColumn(int c)               { _filterCol=c; _rebuild(); }
  void setFilterRole(int r)                    { _filterRole=r; _rebuild(); }
  void setSortRole(int r)                      { _sortRole=r; }
  void setFilterCaseSensitivity(int)           {}

  void sort(int col, Qt::SortOrder order=Qt::AscendingOrder) override {
    _sortCol=col; _sortOrder=order; _rebuild();
  }

  int rowCount(const QModelIndex& =QModelIndex())    const override { return (int)_map.size(); }
  int columnCount(const QModelIndex& =QModelIndex()) const override { return _src?_src->columnCount():0; }
  QModelIndex index(int row, int col, const QModelIndex& =QModelIndex()) const override {
    return createIndex(row,col,nullptr);
  }
  QVariant data(const QModelIndex& idx, int role=Qt::DisplayRole) const override {
    if(!_src||idx.row()>=(int)_map.size()) return QVariant();
    return _src->data(_src->index(_map[idx.row()],idx.column()),role);
  }

  QModelIndex mapToSource(const QModelIndex& proxy) const {
    if(!_src||proxy.row()>=(int)_map.size()) return QModelIndex();
    return _src->index(_map[proxy.row()],proxy.column());
  }
  QModelIndex mapFromSource(const QModelIndex& src) const {
    for(int i=0;i<(int)_map.size();i++)
      if(_map[i]==src.row()) return createIndex(i,src.column(),nullptr);
    return QModelIndex();
  }

private:
  QAbstractItemModel* _src=nullptr;
  QString  _filter;
  int      _filterCol=0, _filterRole=Qt::DisplayRole;
  int      _sortCol=-1;
  Qt::SortOrder _sortOrder=Qt::AscendingOrder;
  int      _sortRole=Qt::DisplayRole;
  std::vector<int> _map; // proxy row → source row

  void _rebuild() {
    _map.clear();
    if(!_src) return;
    for(int r=0;r<_src->rowCount();r++){
      if(_filter.isEmpty()){
        _map.push_back(r);
      } else {
        QString d=_src->data(_src->index(r,_filterCol),_filterRole).toString();
        if(d.indexOf(_filter)>=0) _map.push_back(r);
      }
    }
    if(_sortCol>=0){
      std::sort(_map.begin(),_map.end(),[this](int a,int b){
        QString da=_src->data(_src->index(a,_sortCol),_sortRole).toString();
        QString db=_src->data(_src->index(b,_sortCol),_sortRole).toString();
        return _sortOrder==Qt::AscendingOrder ? da<db : da>db;
      });
    }
    layoutChanged.emit();
  }
};

} // namespace NoorQt

using NoorQt::QModelIndex;
using NoorQt::QAbstractItemModel;
using NoorQt::QStandardItem;
using NoorQt::QStandardItemModel;
using NoorQt::QSortFilterProxyModel;
namespace Qt {
  using namespace NoorQt::Qt;
}
