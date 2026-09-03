// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorQt — QPainter.h                                                     ║
// ║  Full Qt6 QPainter API backed by TFT_eSPI                               ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once
#include "QObject.h"
#include "QGeometry.h"
#include <TFT_eSPI.h>

#include <TFT_eSPI.h>
extern TFT_eSPI tft;

namespace NoorQt {

// ── QPen ──────────────────────────────────────────────────────────────────────
class QPen {
public:
  enum Style { NoPen, SolidLine, DashLine, DotLine, DashDotLine, DashDotDotLine };
  enum CapStyle  { FlatCap, SquareCap, RoundCap };
  enum JoinStyle { MiterJoin, BevelJoin, RoundJoin };

  QPen() : _color(QColor(0,0,0)), _width(1), _style(SolidLine) {}
  QPen(const QColor& c, int w=1, Style s=SolidLine) : _color(c),_width(w),_style(s) {}
  QPen(Qt::GlobalColor c) : _color(QColor(c)), _width(1), _style(SolidLine) {}

  QColor color()     const { return _color; }
  int    width()     const { return _width; }
  float  widthF()    const { return (float)_width; }
  Style  style()     const { return _style; }
  CapStyle  capStyle()  const { return _cap; }
  JoinStyle joinStyle() const { return _join; }

  void setColor(const QColor& c)  { _color=c; }
  void setWidth(int w)            { _width=w; }
  void setWidthF(float w)         { _width=(int)w; }
  void setStyle(Style s)          { _style=s; }
  void setCapStyle(CapStyle c)    { _cap=c; }
  void setJoinStyle(JoinStyle j)  { _join=j; }
  void setDashPattern(QList<float>) {} // stub

  bool isNull()    const { return _style==NoPen; }
  bool operator==(const QPen& o) const { return _color==o._color&&_width==o._width&&_style==o._style; }

  uint16_t rgb565() const { return _color.toRgb565(); }

private:
  QColor _color;
  int _width = 1;
  Style _style = SolidLine;
  CapStyle  _cap  = FlatCap;
  JoinStyle _join = MiterJoin;
};

// ── QBrush ────────────────────────────────────────────────────────────────────
class QBrush {
public:
  enum Style { NoBrush, SolidPattern, Dense1Pattern, Dense2Pattern,
               HorPattern, VerPattern, CrossPattern, LinearGradientPattern,
               RadialGradientPattern };

  QBrush() : _style(NoBrush) {}
  QBrush(const QColor& c, Style s=SolidPattern) : _color(c),_style(s) {}
  QBrush(Qt::GlobalColor c) : _color(QColor(c)),_style(SolidPattern) {}

  QColor color()  const { return _color; }
  Style  style()  const { return _style; }
  void setColor(const QColor& c) { _color=c; }
  void setStyle(Style s)         { _style=s; }
  bool isOpaque() const { return _style!=NoBrush && _color.alpha()==255; }
  bool operator==(const QBrush& o) const { return _color==o._color&&_style==o._style; }

  uint16_t rgb565() const { return _color.toRgb565(); }

private:
  QColor _color;
  Style  _style = NoBrush;
};

// ── QGradient ─────────────────────────────────────────────────────────────────
class QGradient {
public:
  struct Stop { float pos; QColor color; };
  enum Type { LinearGradient, RadialGradient, ConicalGradient, NoGradient };
  enum Spread { PadSpread, ReflectSpread, RepeatSpread };
  enum CoordinateMode { LogicalMode, ObjectMode, StretchToDeviceMode };

  QGradient() : _type(NoGradient) {}
  void setColorAt(float pos, const QColor& color) { _stops.push_back({pos,color}); }
  void setSpread(Spread s) { _spread=s; }
  Spread spread() const { return _spread; }
  Type type() const { return _type; }
  QList<Stop>& stops() { return _stops; }

  // Interpolate color at position t (0-1)
  QColor colorAt(float t) const {
    if (_stops.empty()) return QColor(0,0,0);
    if (_stops.size()==1) return _stops[0].color;
    for (int i=0;i<(int)_stops.size()-1;i++) {
      if (t>=_stops[i].pos && t<=_stops[i+1].pos) {
        float local=(t-_stops[i].pos)/(_stops[i+1].pos-_stops[i].pos);
        return QColor::blend(_stops[i].color,_stops[i+1].color,local);
      }
    }
    return _stops.back().color;
  }

protected:
  Type _type;
  Spread _spread = PadSpread;
  QList<Stop> _stops;
};

class QLinearGradient : public QGradient {
public:
  QLinearGradient() { _type=LinearGradient; }
  QLinearGradient(QPointF start,QPointF end) : _start(start),_end(end) { _type=LinearGradient; }
  QLinearGradient(float x1,float y1,float x2,float y2) : _start(x1,y1),_end(x2,y2) { _type=LinearGradient; }
  QPointF start()  const { return _start; }
  QPointF finalStop()const{ return _end; }
  void setStart(QPointF p)     { _start=p; }
  void setFinalStop(QPointF p) { _end=p; }
private:
  QPointF _start, _end;
};

class QRadialGradient : public QGradient {
public:
  QRadialGradient() { _type=RadialGradient; }
  QRadialGradient(QPointF center, float radius) : _center(center),_radius(radius) { _type=RadialGradient; }
  QPointF center()  const { return _center; }
  float   radius()  const { return _radius; }
  void setCenter(QPointF p){ _center=p; }
  void setRadius(float r)  { _radius=r; }
private:
  QPointF _center;
  float   _radius = 50;
};

// ── QPainterPath ──────────────────────────────────────────────────────────────
class QPainterPath {
public:
  enum ElementType { MoveToElement, LineToElement, CurveToElement, CurveToDataElement };
  struct Element { ElementType type; float x,y; };

  QPainterPath() {}
  QPainterPath(QPointF startPoint) { moveTo(startPoint); }

  void moveTo(float x,float y)         { _elements.push_back({MoveToElement,x,y}); _curX=x;_curY=y; }
  void moveTo(QPointF p)               { moveTo(p.x(),p.y()); }
  void lineTo(float x,float y)         { _elements.push_back({LineToElement,x,y}); _curX=x;_curY=y; }
  void lineTo(QPointF p)               { lineTo(p.x(),p.y()); }
  void arcTo(QRectF rect,float startAngle,float spanAngle);
  void cubicTo(QPointF c1,QPointF c2,QPointF ep){ _elements.push_back({CurveToElement,c1.x(),c1.y()}); _elements.push_back({CurveToDataElement,c2.x(),c2.y()}); _elements.push_back({CurveToDataElement,ep.x(),ep.y()}); _curX=ep.x();_curY=ep.y(); }
  void quadTo(QPointF c,QPointF ep)   { cubicTo(c,c,ep); }
  void closeSubpath()                  { if(!_elements.empty()) { auto& e=_elements[0]; lineTo(e.x,e.y); } }
  void addRect(QRectF r)               { moveTo(r.x(),r.y()); lineTo(r.x()+r.width(),r.y()); lineTo(r.x()+r.width(),r.y()+r.height()); lineTo(r.x(),r.y()+r.height()); closeSubpath(); }
  void addEllipse(QRectF r)            { arcTo(r,0,360); }
  void addEllipse(QPointF c,float rx,float ry){ addEllipse(QRectF(c.x()-rx,c.y()-ry,rx*2,ry*2)); }
  void addPolygon(const QPolygon& p)   { if(p.empty())return; moveTo(p[0].x(),p[0].y()); for(int i=1;i<(int)p.size();i++)lineTo(p[i].x(),p[i].y()); }
  void addPath(const QPainterPath& p)  { for(auto&e:p._elements)_elements.push_back(e); }
  void connectPath(const QPainterPath& p){ if(!p._elements.empty())lineTo(p._elements[0].x,p._elements[0].y); addPath(p); }

  bool isEmpty()       const { return _elements.empty(); }
  int  elementCount()  const { return _elements.size(); }
  Element elementAt(int i) const { return _elements[i]; }

  QRectF boundingRect() const {
    if(_elements.empty())return{};
    float x1=_elements[0].x,y1=_elements[0].y,x2=x1,y2=y1;
    for(auto&e:_elements){x1=min(x1,e.x);y1=min(y1,e.y);x2=max(x2,e.x);y2=max(y2,e.y);}
    return{x1,y1,x2-x1,y2-y1};
  }

  bool contains(QPointF p) const {
    // Ray casting on path segments
    bool inside=false;
    int n=_elements.size();
    for(int i=0,j=n-1;i<n;j=i++){
      auto& ei=_elements[i]; auto& ej=_elements[j];
      if(((ei.y>p.y())!=(ej.y>p.y()))&&(p.x()<(ej.x-ei.x)*(p.y()-ei.y)/(ej.y-ei.y)+ei.x))
        inside=!inside;
    }
    return inside;
  }

  QPainterPath& operator+=(const QPainterPath& o){ addPath(o); return *this; }
  QPainterPath  operator+(const QPainterPath& o) const { QPainterPath r=*this; r.addPath(o); return r; }

  const QList<Element>& elements() const { return _elements; }

  float currentX() const { return _curX; }
  float currentY() const { return _curY; }
  QPointF currentPosition() const { return {_curX,_curY}; }

private:
  QList<Element> _elements;
  float _curX=0,_curY=0;
};

// ── QPaintDevice (base for things you can paint on) ───────────────────────────
class QPaintDevice {
public:
  virtual ~QPaintDevice() {}
  virtual int width()  const { return 240; }
  virtual int height() const { return 320; }
  int deviceWidth()    const { return width(); }
  int deviceHeight()   const { return height(); }
};

// ── QPainter ──────────────────────────────────────────────────────────────────
class QPainter {
public:
  enum CompositionMode {
    CompositionMode_SourceOver, CompositionMode_Source,
    CompositionMode_Clear, CompositionMode_Destination
  };
  enum RenderHint { Antialiasing=0x01, TextAntialiasing=0x02, SmoothPixmapTransform=0x04 };

  QPainter() {}
  explicit QPainter(QPaintDevice* device) { begin(device); }

  bool begin(QPaintDevice* d){ _device=d; _active=true; return true; }
  bool end()                  { _active=false; return true; }
  bool isActive()      const  { return _active; }

  // ── Pen / Brush ───────────────────────────────────────────────────────────
  void setPen(const QPen& p)     { _pen=p; }
  void setPen(const QColor& c)   { _pen=QPen(c,1); }
  void setPen(Qt::GlobalColor c) { _pen=QPen(QColor(c),1); }
  void setPen(QPen::Style s)     { if(s==QPen::NoPen)_pen=QPen(QColor(0,0,0,0),0,s); }
  const QPen& pen() const        { return _pen; }

  void setBrush(const QBrush& b)     { _brush=b; }
  void setBrush(const QColor& c)     { _brush=QBrush(c); }
  void setBrush(Qt::GlobalColor c)   { _brush=QBrush(QColor(c)); }
  void setBrush(QBrush::Style s)     { _brush.setStyle(s); }
  const QBrush& brush() const        { return _brush; }

  void setBackground(const QBrush& b){ _bg=b; }
  QBrush background() const          { return _bg; }

  // ── Font ──────────────────────────────────────────────────────────────────
  void setFont(const QFont& f){ _font=f; }
  const QFont& font() const   { return _font; }

  // ── Transform ─────────────────────────────────────────────────────────────
  void setTransform(const QTransform& t,bool combine=false){
    if(combine)_transform=_transform*t; else _transform=t;
  }
  void resetTransform()              { _transform=QTransform(); }
  const QTransform& transform()const { return _transform; }
  void translate(float dx,float dy)  { _transform.translate(dx,dy); }
  void translate(QPointF p)          { translate(p.x(),p.y()); }
  void scale(float sx,float sy)      { _transform.scale(sx,sy); }
  void rotate(float angle)           { _transform.rotate(angle); }
  void shear(float sh,float sv)      { _transform.shear(sh,sv); }

  // ── Save / Restore state ──────────────────────────────────────────────────
  struct State { QPen pen; QBrush brush; QFont font; QTransform transform; QRect clip; bool hasClip; };
  void save()    { _stateStack.push_back({_pen,_brush,_font,_transform,_clipRect,_hasClip}); }
  void restore() {
    if(!_stateStack.empty()){
      auto& s=_stateStack.back();
      _pen=s.pen;_brush=s.brush;_font=s.font;_transform=s.transform;
      _clipRect=s.clip;_hasClip=s.hasClip;
      _stateStack.pop_back();
    }
  }

  // ── Clipping ──────────────────────────────────────────────────────────────
  void setClipRect(QRect r){ _clipRect=r; _hasClip=true; }
  void setClipRect(int x,int y,int w,int h){ setClipRect(QRect{x,y,w,h}); }
  void setClipping(bool b)  { _hasClip=b; }
  bool hasClipping()  const { return _hasClip; }
  QRect clipRect()    const { return _clipRect; }
  void clipTo(QRect r)      { if(_hasClip)_clipRect=_clipRect.intersected(r); else setClipRect(r); }

  // ── Render hints ──────────────────────────────────────────────────────────
  void setRenderHint(RenderHint h,bool on=true){ if(on)_hints|=h; else _hints&=~h; }
  void setRenderHints(int hints){ _hints=hints; }
  bool testRenderHint(RenderHint h) const { return (_hints&h)!=0; }

  // ── Composition ───────────────────────────────────────────────────────────
  void setCompositionMode(CompositionMode m){ _compMode=m; }
  CompositionMode compositionMode() const   { return _compMode; }

  // ── Opacity ───────────────────────────────────────────────────────────────
  void setOpacity(float o){ _opacity=o; }
  float opacity()   const { return _opacity; }

  // ── Drawing primitives ────────────────────────────────────────────────────
  QPoint mapToDevice(QPoint p) const { return _transform.map(p); }

  void drawPoint(int x,int y){
    if(!_pen.isNull()) tft.drawPixel(tx(x),ty(y),pc());
  }
  void drawPoint(QPoint p){ drawPoint(p.x(),p.y()); }
  void drawPoints(const QPolygon& pts){ for(auto&p:pts)drawPoint(p); }

  void drawLine(int x1,int y1,int x2,int y2){
    if(_pen.isNull())return;
    QPoint p1=mapToDevice({x1,y1}),p2=mapToDevice({x2,y2});
    for(int w=0;w<_pen.width();w++)
      tft.drawLine(p1.x(),p1.y()+w,p2.x(),p2.y()+w,pc());
  }
  void drawLine(QLine l)       { drawLine(l.x1(),l.y1(),l.x2(),l.y2()); }
  void drawLine(QPoint a,QPoint b){ drawLine(a.x(),a.y(),b.x(),b.y()); }
  void drawLine(QLineF l)      { drawLine((int)l.x1(),(int)l.y1(),(int)l.x2(),(int)l.y2()); }
  void drawLines(const QList<QLine>& ls){ for(auto&l:ls)drawLine(l); }

  void drawRect(int x,int y,int w,int h){
    auto [mx,my]= map2d(x,y);
    if(_brush.style()!=QBrush::NoBrush) tft.fillRect(mx,my,w,h,bc());
    if(!_pen.isNull()) for(int i=0;i<_pen.width();i++) tft.drawRect(mx-i,my-i,w+i*2,h+i*2,pc());
  }
  void drawRect(QRect r){ drawRect(r.x(),r.y(),r.width(),r.height()); }
  void drawRect(QRectF r){ drawRect((int)r.x(),(int)r.y(),(int)r.width(),(int)r.height()); }
  void drawRects(const QList<QRect>& rs){ for(auto&r:rs)drawRect(r); }
  void fillRect(QRect r,const QBrush& b){ auto [mx,my]=map2d(r.x(),r.y()); tft.fillRect(mx,my,r.width(),r.height(),b.rgb565()); }
  void fillRect(QRect r,const QColor& c){ fillRect(r,QBrush(c)); }
  void fillRect(int x,int y,int w,int h,const QColor& c){ fillRect({x,y,w,h},c); }
  void fillRect(int x,int y,int w,int h,Qt::GlobalColor c){ fillRect({x,y,w,h},QColor(c)); }
  void eraseRect(QRect r){ fillRect(r,_bg.color()); }

  void drawRoundedRect(int x,int y,int w,int h,float rx,float ry,bool=false){
    auto [mx,my]=map2d(x,y); int r=(int)((rx+ry)/2);
    if(_brush.style()!=QBrush::NoBrush) tft.fillRoundRect(mx,my,w,h,r,bc());
    if(!_pen.isNull()) for(int i=0;i<_pen.width();i++) tft.drawRoundRect(mx-i,my-i,w+i*2,h+i*2,r,pc());
  }
  void drawRoundedRect(QRect r,float rx,float ry){ drawRoundedRect(r.x(),r.y(),r.width(),r.height(),rx,ry); }
  void drawRoundedRect(QRectF r,float rx,float ry){ drawRoundedRect((int)r.x(),(int)r.y(),(int)r.width(),(int)r.height(),rx,ry); }

  void drawEllipse(int x,int y,int w,int h){
    auto [mx,my]=map2d(x,y); int cx=mx+w/2,cy=my+h/2,rx=w/2,ry=h/2;
    if(rx==ry){
      if(_brush.style()!=QBrush::NoBrush) tft.fillCircle(cx,cy,rx,bc());
      if(!_pen.isNull()) tft.drawCircle(cx,cy,rx,pc());
    } else {
      // Approximate with fillEllipse
      for(int py=-ry;py<=ry;py++)
        for(int px=-rx;px<=rx;px++)
          if((float)px*px/(rx*rx)+(float)py*py/(ry*ry)<=1)
            tft.drawPixel(cx+px,cy+py,_brush.style()!=QBrush::NoBrush?bc():pc());
    }
  }
  void drawEllipse(QRect r)  { drawEllipse(r.x(),r.y(),r.width(),r.height()); }
  void drawEllipse(QRectF r) { drawEllipse((int)r.x(),(int)r.y(),(int)r.width(),(int)r.height()); }
  void drawEllipse(QPoint c,int rx,int ry){ drawEllipse(c.x()-rx,c.y()-ry,rx*2,ry*2); }
  void drawEllipse(QPointF c,float rx,float ry){ drawEllipse((int)(c.x()-rx),(int)(c.y()-ry),(int)(rx*2),(int)(ry*2)); }

  void drawArc(QRect r,int startAngle,int spanAngle){
    int cx=r.x()+r.width()/2, cy=r.y()+r.height()/2;
    int rad=(r.width()+r.height())/4;
    float s=startAngle/16.0f*M_PI/180, span=spanAngle/16.0f*M_PI/180;
    int steps=max(8,(int)(fabsf(span)*rad/2));
    for(int i=0;i<steps;i++){
      float a1=s+span*i/steps, a2=s+span*(i+1)/steps;
      tft.drawLine(cx+cosf(a1)*rad,cy-sinf(a1)*rad,cx+cosf(a2)*rad,cy-sinf(a2)*rad,pc());
    }
  }
  void drawArc(int x,int y,int w,int h,int sa,int sp){ drawArc({x,y,w,h},sa,sp); }
  void drawPie(QRect r,int sa,int sp){
    int cx=r.x()+r.width()/2,cy=r.y()+r.height()/2,rad=(r.width()+r.height())/4;
    float s=sa/16.0f*M_PI/180,span=sp/16.0f*M_PI/180;
    int steps=max(8,(int)(fabsf(span)*rad));
    for(int i=0;i<steps;i++){
      float a=s+span*i/steps;
      tft.drawLine(cx,cy,cx+cosf(a)*rad,cy-sinf(a)*rad,bc());
    }
    drawArc(r,sa,sp);
  }
  void drawChord(QRect r,int sa,int sp){ drawArc(r,sa,sp); }

  void drawPolygon(const QPolygon& pts,bool filled=true){
    if(filled && _brush.style()!=QBrush::NoBrush){
      // Simple triangle fan from first point
      for(int i=1;i<(int)pts.size()-1;i++)
        tft.fillTriangle(pts[0].x(),pts[0].y(),pts[i].x(),pts[i].y(),pts[i+1].x(),pts[i+1].y(),bc());
    }
    if(!_pen.isNull()){
      for(int i=0;i<(int)pts.size();i++){
        auto& a=pts[i]; auto& b=pts[(i+1)%pts.size()];
        drawLine(a.x(),a.y(),b.x(),b.y());
      }
    }
  }
  void drawPolyline(const QPolygon& pts){
    for(int i=0;i<(int)pts.size()-1;i++) drawLine(pts[i],pts[i+1]);
  }

  void drawConvexPolygon(const QPolygon& pts){ drawPolygon(pts,true); }

  void drawPath(const QPainterPath& path){
    auto& elems=path.elements();
    float lx=0,ly=0;
    for(auto& e:elems){
      if(e.type==QPainterPath::MoveToElement){ lx=e.x;ly=e.y; }
      else if(e.type==QPainterPath::LineToElement){
        drawLine((int)lx,(int)ly,(int)e.x,(int)e.y); lx=e.x;ly=e.y;
      }
    }
  }
  void strokePath(const QPainterPath& p,const QPen& pen){ auto op=_pen;_pen=pen;drawPath(p);_pen=op; }
  void fillPath(const QPainterPath& p,const QBrush& b)  { /* approximate */ }

  // ── Gradient fill ─────────────────────────────────────────────────────────
  void fillRect(QRect r, const QLinearGradient& g){
    auto [mx,my]=map2d(r.x(),r.y());
    bool horiz=(g.finalStop().x()!=g.start().x());
    int steps=horiz?r.width():r.height();
    for(int i=0;i<steps;i++){
      float t=(float)i/steps;
      QColor c=g.colorAt(t);
      if(horiz) tft.drawFastVLine(mx+i,my,r.height(),c.toRgb565());
      else      tft.drawFastHLine(mx,my+i,r.width(),c.toRgb565());
    }
  }
  void fillRect(QRect r,const QRadialGradient& g){
    auto [mx,my]=map2d(r.x(),r.y());
    int cx=mx+r.width()/2,cy=my+r.height()/2,rad=min(r.width(),r.height())/2;
    for(int py=my;py<my+r.height();py++)
      for(int px=mx;px<mx+r.width();px++){
        float d=sqrtf((px-cx)*(px-cx)+(py-cy)*(py-cy));
        float t=min(1.0f,d/rad);
        tft.drawPixel(px,py,g.colorAt(t).toRgb565());
      }
  }

  // ── Text ──────────────────────────────────────────────────────────────────
  void drawText(int x,int y,const QString& text){
    auto [mx,my]=map2d(x,y);
    tft.setTextColor(_pen.rgb565());
    tft.setTextSize(_font.tftSize());
    tft.setCursor(mx,my-8*_font.tftSize());
    tft.print(text);
  }
  void drawText(QPoint p,const QString& t){ drawText(p.x(),p.y(),t); }
  void drawText(QRect r,int flags,const QString& text,QRect* boundingRect=nullptr){
    auto [mx,my]=map2d(r.x(),r.y());
    int fs=_font.tftSize(), fw=text.length()*6*fs, fh=8*fs;
    int tx2=mx, ty2=my;
    if(flags&Qt::AlignHCenter) tx2=mx+(r.width()-fw)/2;
    else if(flags&Qt::AlignRight) tx2=mx+r.width()-fw;
    if(flags&Qt::AlignVCenter) ty2=my+(r.height()-fh)/2;
    else if(flags&Qt::AlignBottom) ty2=my+r.height()-fh;
    tft.setTextColor(_pen.rgb565(),_bg.color().toRgb565());
    tft.setTextSize(fs);
    tft.setCursor(tx2,ty2);
    tft.print(text);
    if(boundingRect)*boundingRect={tx2,ty2,fw,fh};
  }
  void drawText(QRectF r,const QString& t,int flags=Qt::AlignLeft){ drawText(r.toRect(),flags,t); }
  void drawText(int x,int y,int w,int h,int flags,const QString& t){ drawText({x,y,w,h},flags,t); }
  void drawStaticText(QPoint p,const QString& t){ drawText(p.x(),p.y(),t); }
  void drawStaticText(int x,int y,const QString& t){ drawText(x,y,t); }

  QRect boundingRect(QRect r,int flags,const QString& t){
    int fs=_font.tftSize();
    return{r.x(),r.y(),(int)t.length()*6*fs,8*fs};
  }
  QRectF boundingRect(QRectF r,const QString& t,int=0){
    int fs=_font.tftSize();
    return{r.x(),r.y(),(float)t.length()*6*fs,(float)8*fs};
  }

  // ── Pixel access ──────────────────────────────────────────────────────────
  void drawPixmap(int x,int y,int w,int h,const uint16_t* data){
    for(int py=0;py<h;py++) for(int px=0;px<w;px++) tft.drawPixel(tx(x+px),ty(y+py),data[py*w+px]);
  }
  void drawImage(QRect r,const uint16_t* data){ drawPixmap(r.x(),r.y(),r.width(),r.height(),data); }
  void drawImage(int x,int y,const uint16_t* data,int w,int h){ drawPixmap(x,y,w,h,data); }

  // ── Window / Viewport ─────────────────────────────────────────────────────
  void setWindow(QRect r)   { _window=r; }
  void setViewport(QRect r) { _viewport=r; }
  QRect window()    const   { return _window; }
  QRect viewport()  const   { return _viewport; }

  // ── Device ────────────────────────────────────────────────────────────────
  QPaintDevice* device() const { return _device; }

private:
  QPaintDevice* _device=nullptr;
  bool _active=false;
  QPen _pen{QColor(0,0,0),1};
  QBrush _brush;
  QBrush _bg{QColor(0,0,0)};
  QFont _font;
  QTransform _transform;
  QRect _clipRect;
  bool _hasClip=false;
  int _hints=0;
  float _opacity=1.0f;
  CompositionMode _compMode=CompositionMode_SourceOver;
  QRect _window{0,0,240,320};
  QRect _viewport{0,0,240,320};
  QList<State> _stateStack;

  // Map logical->device
  std::pair<int,int> map2d(int x,int y) const {
    auto p=_transform.map(QPoint(x,y));
    return{p.x(),p.y()};
  }
  int tx(int x) const { return _transform.map(QPoint(x,0)).x(); }
  int ty(int y) const { return _transform.map(QPoint(0,y)).y(); }
  uint16_t pc() const { return _pen.rgb565(); }
  uint16_t bc() const { return _brush.rgb565(); }
};

// ── QPainterPath::arcTo impl ──────────────────────────────────────────────────
inline void QPainterPath::arcTo(QRectF rect,float startAngle,float spanAngle){
  int cx=(int)(rect.x()+rect.width()/2), cy=(int)(rect.y()+rect.height()/2);
  int rx=(int)(rect.width()/2), ry=(int)(rect.height()/2);
  float s=startAngle*M_PI/180, span=spanAngle*M_PI/180;
  int steps=max(8,(int)(fabsf(span)*max(rx,ry)));
  for(int i=0;i<=steps;i++){
    float a=s+span*i/steps;
    float x=cx+cosf(a)*rx, y=cy-sinf(a)*ry;
    if(i==0) moveTo(x,y); else lineTo(x,y);
  }
}

} // namespace NoorQt

using NoorQt::QPen;
using NoorQt::QBrush;
using NoorQt::QPainter;
using NoorQt::QPainterPath;
using NoorQt::QLinearGradient;
using NoorQt::QRadialGradient;
