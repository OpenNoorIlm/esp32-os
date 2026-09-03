// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorQt — QGeometry.h                                                    ║
// ║  QColor, QFont, QPoint, QPointF, QSize, QSizeF, QRect, QRectF,          ║
// ║  QMargins, QLine, QLineF, QPolygon                                       ║
// ║  Mirrors Qt6 API exactly.                                                ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once
#include "QObject.h"
#include <math.h>

namespace NoorQt {

// ════════════════════════════════════════════════════════════════════════════
// QColor — full color system (RGB, RGBA, HSV, HSL, CMYK, named, hex, RGB565)
// ════════════════════════════════════════════════════════════════════════════
class QColor {
public:
  // ── Constructors ──────────────────────────────────────────────────────────
  QColor() : _r(0),_g(0),_b(0),_a(255),_valid(false) {}
  QColor(int r,int g,int b,int a=255) : _r(clamp(r)),_g(clamp(g)),_b(clamp(b)),_a(clamp(a)),_valid(true) {}
  explicit QColor(Qt::GlobalColor c) { *this=fromGlobal(c); }
  explicit QColor(uint32_t rgba)     { _r=(rgba>>16)&0xFF;_g=(rgba>>8)&0xFF;_b=rgba&0xFF;_a=(rgba>>24)&0xFF;_valid=true; }
  explicit QColor(const QString& name){ *this=fromName(name); }
  explicit QColor(uint16_t rgb565)   { *this=fromRgb565(rgb565); }

  // ── Validity ──────────────────────────────────────────────────────────────
  bool isValid()  const { return _valid; }
  static QColor Invalid() { return QColor(); }

  // ── RGB getters/setters ───────────────────────────────────────────────────
  int red()       const { return _r; }
  int green()     const { return _g; }
  int blue()      const { return _b; }
  int alpha()     const { return _a; }
  float redF()    const { return _r/255.0f; }
  float greenF()  const { return _g/255.0f; }
  float blueF()   const { return _b/255.0f; }
  float alphaF()  const { return _a/255.0f; }

  void setRed(int r)    { _r=clamp(r); }
  void setGreen(int g)  { _g=clamp(g); }
  void setBlue(int b)   { _b=clamp(b); }
  void setAlpha(int a)  { _a=clamp(a); }
  void setRedF(float r) { _r=clamp((int)(r*255)); }
  void setGreenF(float g){_g=clamp((int)(g*255)); }
  void setBlueF(float b) { _b=clamp((int)(b*255)); }
  void setAlphaF(float a){ _a=clamp((int)(a*255)); }

  void setRgb(int r,int g,int b,int a=255){ _r=clamp(r);_g=clamp(g);_b=clamp(b);_a=clamp(a);_valid=true; }
  void getRgb(int*r,int*g,int*b,int*a=nullptr) const { *r=_r;*g=_g;*b=_b;if(a)*a=_a; }
  uint32_t rgba() const { return ((uint32_t)_a<<24)|((uint32_t)_r<<16)|((uint32_t)_g<<8)|_b; }
  uint32_t rgb()  const { return ((uint32_t)_r<<16)|((uint32_t)_g<<8)|_b; }

  // ── HSV ───────────────────────────────────────────────────────────────────
  int hsvHue()        const { return (int)(_hsv().h*360); }
  int hsvSaturation() const { return (int)(_hsv().s*255); }
  int value()         const { return (int)(_hsv().v*255); }
  float hsvHueF()        const { return _hsv().h; }
  float hsvSaturationF() const { return _hsv().s; }
  float valueF()         const { return _hsv().v; }

  void setHsv(int h,int s,int v,int a=255) {
    float hf=h/360.0f, sf=s/255.0f, vf=v/255.0f;
    auto rgb=hsvToRgb(hf,sf,vf);
    _r=rgb.r;_g=rgb.g;_b=rgb.b;_a=clamp(a);_valid=true;
  }
  void getHsv(int*h,int*s,int*v,int*a=nullptr) const {
    auto hsv=_hsv(); *h=(int)(hsv.h*360);*s=(int)(hsv.s*255);*v=(int)(hsv.v*255);
    if(a)*a=_a;
  }

  static QColor fromHsv(int h,int s,int v,int a=255){
    QColor c; c.setHsv(h,s,v,a); return c;
  }
  static QColor fromHsvF(float h,float s,float v,float a=1.0f){
    auto rgb=hsvToRgb(h,s,v);
    return QColor(rgb.r,rgb.g,rgb.b,(int)(a*255));
  }

  // ── HSL ───────────────────────────────────────────────────────────────────
  int hslHue()        const { return (int)(_hsl().h*360); }
  int hslSaturation() const { return (int)(_hsl().s*255); }
  int lightness()     const { return (int)(_hsl().l*255); }

  void setHsl(int h,int s,int l,int a=255) {
    auto rgb=hslToRgb(h/360.0f,s/255.0f,l/255.0f);
    _r=rgb.r;_g=rgb.g;_b=rgb.b;_a=clamp(a);_valid=true;
  }
  static QColor fromHsl(int h,int s,int l,int a=255){
    QColor c; c.setHsl(h,s,l,a); return c;
  }

  // ── CMYK ──────────────────────────────────────────────────────────────────
  int cyan()    const { int C,M,Y,K; getCmyk(&C,&M,&Y,&K); return C; }
  int magenta() const { int C,M,Y,K; getCmyk(&C,&M,&Y,&K); return M; }
  int yellow()  const { int C,M,Y,K; getCmyk(&C,&M,&Y,&K); return Y; }
  int black()   const { int C,M,Y,K; getCmyk(&C,&M,&Y,&K); return K; }

  void getCmyk(int*c,int*m,int*y,int*k,int*a=nullptr) const {
    float rf=_r/255.0f,gf=_g/255.0f,bf=_b/255.0f;
    float kf=1-max({rf,gf,bf});
    *k=(int)(kf*255);
    if (kf<1) { *c=(int)((1-rf-kf)/(1-kf)*255); *m=(int)((1-gf-kf)/(1-kf)*255); *y=(int)((1-bf-kf)/(1-kf)*255); }
    else { *c=0;*m=0;*y=0; }
    if(a)*a=_a;
  }

  // ── RGB565 (TFT native format) ────────────────────────────────────────────
  uint16_t toRgb565() const {
    return (uint16_t)((_r>>3)<<11)|((_g>>2)<<5)|(_b>>3);
  }
  static QColor fromRgb565(uint16_t c) {
    return QColor(((c>>11)&0x1F)<<3, ((c>>5)&0x3F)<<2, (c&0x1F)<<3);
  }

  // ── Name / hex ────────────────────────────────────────────────────────────
  QString name() const {
    char buf[10];
    snprintf(buf,sizeof(buf),"#%02x%02x%02x",_r,_g,_b);
    return String(buf);
  }
  QString nameARGB() const {
    char buf[12];
    snprintf(buf,sizeof(buf),"#%02x%02x%02x%02x",_a,_r,_g,_b);
    return String(buf);
  }

  static QColor fromName(const QString& name) {
    QString n=name; n.toLowerCase(); n.trim();
    // HTML named colors
    struct NC{const char*name;int r,g,b;};
    static const NC TABLE[]={
      {"black",0,0,0},{"white",255,255,255},{"red",255,0,0},
      {"green",0,128,0},{"lime",0,255,0},{"blue",0,0,255},
      {"cyan",0,255,255},{"magenta",255,0,255},{"yellow",255,255,0},
      {"orange",255,165,0},{"purple",128,0,128},{"pink",255,192,203},
      {"brown",165,42,42},{"grey",128,128,128},{"gray",128,128,128},
      {"darkgrey",169,169,169},{"lightgrey",211,211,211},
      {"darkgray",169,169,169},{"lightgray",211,211,211},
      {"navy",0,0,128},{"teal",0,128,128},{"maroon",128,0,0},
      {"olive",128,128,0},{"silver",192,192,192},{"gold",255,215,0},
      {"coral",255,127,80},{"salmon",250,128,114},{"crimson",220,20,60},
      {"indigo",75,0,130},{"violet",238,130,238},{"turquoise",64,224,208},
      {"transparent",0,0,0},{nullptr,0,0,0}
    };
    for (const NC* nc=TABLE;nc->name;nc++) {
      if (n==nc->name) { QColor c(nc->r,nc->g,nc->b); if(n=="transparent")c._a=0; return c; }
    }
    // Hex
    if (n.startsWith("#")) {
      n=n.substring(1);
      if (n.length()==3) {
        char r=n[0],g=n[1],b=n[2];
        char sr[3]={r,r,0}, sg[3]={g,g,0}, sb[3]={b,b,0};
        return QColor((int)strtol(sr,nullptr,16),(int)strtol(sg,nullptr,16),(int)strtol(sb,nullptr,16));
      }
      if (n.length()==6) {
        long v=strtol(n.c_str(),nullptr,16);
        return QColor((v>>16)&0xFF,(v>>8)&0xFF,v&0xFF);
      }
      if (n.length()==8) {
        long v=strtol(n.c_str(),nullptr,16);
        return QColor((v>>16)&0xFF,(v>>8)&0xFF,v&0xFF,(v>>24)&0xFF);
      }
    }
    // rgb(r,g,b)
    if (n.startsWith("rgb(")) {
      n=n.substring(4,n.indexOf(')'));
      auto parts=QStringList::split(n,",");
      if (parts.size()>=3) return QColor(parts[0].toInt(),parts[1].toInt(),parts[2].toInt());
    }
    return QColor();
  }

  static QColor fromGlobal(Qt::GlobalColor c) {
    switch(c) {
      case Qt::white:     return QColor(255,255,255);
      case Qt::black:     return QColor(0,0,0);
      case Qt::red:       return QColor(255,0,0);
      case Qt::darkRed:   return QColor(128,0,0);
      case Qt::green:     return QColor(0,128,0);
      case Qt::darkGreen: return QColor(0,64,0);
      case Qt::blue:      return QColor(0,0,255);
      case Qt::darkBlue:  return QColor(0,0,128);
      case Qt::cyan:      return QColor(0,255,255);
      case Qt::darkCyan:  return QColor(0,128,128);
      case Qt::magenta:   return QColor(255,0,255);
      case Qt::darkMagenta:return QColor(128,0,128);
      case Qt::yellow:    return QColor(255,255,0);
      case Qt::darkYellow:return QColor(128,128,0);
      case Qt::gray:      return QColor(160,160,164);
      case Qt::darkGray:  return QColor(128,128,128);
      case Qt::lightGray: return QColor(192,192,192);
      case Qt::transparent:return QColor(0,0,0,0);
      default:            return QColor(0,0,0);
    }
  }

  // ── Color operations ──────────────────────────────────────────────────────
  QColor lighter(int factor=150) const {
    float f=factor/100.0f;
    return QColor(min(255,(int)(_r*f)),min(255,(int)(_g*f)),min(255,(int)(_b*f)),_a);
  }
  QColor darker(int factor=200) const {
    float f=100.0f/factor;
    return QColor((int)(_r*f),(int)(_g*f),(int)(_b*f),_a);
  }
  QColor toRgb() const { return QColor(_r,_g,_b,255); }

  // Blend two colors
  static QColor blend(const QColor& a,const QColor& b,float t=0.5f) {
    return QColor((int)(a._r+(b._r-a._r)*t),(int)(a._g+(b._g-a._g)*t),
                  (int)(a._b+(b._b-a._b)*t),(int)(a._a+(b._a-a._a)*t));
  }

  // ── Comparison ────────────────────────────────────────────────────────────
  bool operator==(const QColor& o) const { return _r==o._r&&_g==o._g&&_b==o._b&&_a==o._a; }
  bool operator!=(const QColor& o) const { return !(*this==o); }

  // ── toString ──────────────────────────────────────────────────────────────
  QString toString() const { return name(); }

  // ── Static color constants ────────────────────────────────────────────────
  static QColor fromRed()       { return QColor(255,0,0); }
  static QColor fromGreen()     { return QColor(0,128,0); }
  static QColor fromBlue()      { return QColor(0,0,255); }
  static QColor fromWhite()     { return QColor(255,255,255); }
  static QColor fromBlack()     { return QColor(0,0,0); }
  static QColor fromYellow()    { return QColor(255,255,0); }
  static QColor fromCyan()      { return QColor(0,255,255); }
  static QColor fromMagenta()   { return QColor(255,0,255); }
  static QColor fromTransparent(){ return QColor(0,0,0,0); }
  static QColor fromGray()      { return QColor(128,128,128); }

private:
  int  _r=0,_g=0,_b=0,_a=255;
  bool _valid=false;

  static int clamp(int v){ return v<0?0:v>255?255:v; }

  struct HSV{float h,s,v;};
  struct HSL{float h,s,l;};
  struct RGB{int r,g,b;};

  HSV _hsv() const {
    float r=_r/255.0f,g=_g/255.0f,b=_b/255.0f;
    float mx=max({r,g,b}),mn=min({r,g,b}),d=mx-mn;
    HSV h{0,0,mx};
    if (d==0) return h;
    h.s=d/mx;
    if (mx==r) h.h=(g-b)/d/6+(g<b?1:0);
    else if (mx==g) h.h=(b-r)/d/6+1.0f/3;
    else h.h=(r-g)/d/6+2.0f/3;
    return h;
  }

  HSL _hsl() const {
    float r=_r/255.0f,g=_g/255.0f,b=_b/255.0f;
    float mx=max({r,g,b}),mn=min({r,g,b});
    HSL hsl{0,0,(mx+mn)/2};
    if (mx==mn) return hsl;
    float d=mx-mn;
    hsl.s=hsl.l>0.5f?d/(2-mx-mn):d/(mx+mn);
    if(mx==r) hsl.h=(g-b)/d+(g<b?6:0);
    else if(mx==g) hsl.h=(b-r)/d+2;
    else hsl.h=(r-g)/d+4;
    hsl.h/=6;
    return hsl;
  }

  static RGB hsvToRgb(float h,float s,float v) {
    float r,g,b;
    int i=(int)(h*6); float f=h*6-i;
    float p=v*(1-s),q=v*(1-f*s),t=v*(1-(1-f)*s);
    switch(i%6){
      case 0:r=v;g=t;b=p;break; case 1:r=q;g=v;b=p;break;
      case 2:r=p;g=v;b=t;break; case 3:r=p;g=q;b=v;break;
      case 4:r=t;g=p;b=v;break; default:r=v;g=p;b=q;break;
    }
    return {(int)(r*255),(int)(g*255),(int)(b*255)};
  }

  static float hslHelper(float p,float q,float t){
    if(t<0)t+=1; if(t>1)t-=1;
    if(t<1/6.0f)return p+(q-p)*6*t;
    if(t<0.5f)return q;
    if(t<2/3.0f)return p+(q-p)*(2/3.0f-t)*6;
    return p;
  }
  static RGB hslToRgb(float h,float s,float l){
    if(s==0){int g=(int)(l*255);return{g,g,g};}
    float q=l<0.5f?l*(1+s):l+s-l*s, p=2*l-q;
    return{(int)(hslHelper(p,q,h+1/3.0f)*255),
           (int)(hslHelper(p,q,h)*255),
           (int)(hslHelper(p,q,h-1/3.0f)*255)};
  }
};

// ════════════════════════════════════════════════════════════════════════════
// QFont
// ════════════════════════════════════════════════════════════════════════════
class QFont {
public:
  enum Weight { Thin=100,ExtraLight=200,Light=300,Normal=400,Medium=500,
                SemiBold=600,Bold=700,ExtraBold=800,Black=900 };
  enum Style  { StyleNormal, StyleItalic, StyleOblique };
  enum Stretch{ UltraCondensed=50,Condensed=75,Normal2=100,Expanded=125,UltraExpanded=200 };

  QFont() {}
  QFont(const QString& family,int pointSize=-1,int weight=Normal,bool italic=false)
    : _family(family),_pointSize(pointSize),_weight(weight),_italic(italic) {}

  void setFamily(const QString& f)    { _family=f; }
  void setPointSize(int s)            { _pointSize=s; }
  void setPixelSize(int s)            { _pixelSize=s; }
  void setWeight(int w)               { _weight=w; }
  void setBold(bool b)                { _weight=b?Bold:Normal; }
  void setItalic(bool i)              { _italic=i; }
  void setUnderline(bool u)           { _underline=u; }
  void setStrikeOut(bool s)           { _strikeOut=s; }
  void setOverline(bool o)            { _overline=o; }
  void setFixedPitch(bool f)          { _fixedPitch=f; }
  void setLetterSpacing(int spacing)  { _letterSpacing=spacing; }
  void setWordSpacing(int spacing)    { _wordSpacing=spacing; }
  void setCapitalization(int cap)     { _caps=cap; }
  void setStyle(Style s)              { _style=s; }
  void setStretch(int s)              { _stretch=s; }

  QString family()    const { return _family; }
  int pointSize()     const { return _pointSize; }
  int pixelSize()     const { return _pixelSize; }
  int weight()        const { return _weight; }
  bool bold()         const { return _weight>=Bold; }
  bool italic()       const { return _italic; }
  bool underline()    const { return _underline; }
  bool strikeOut()    const { return _strikeOut; }
  bool overline()     const { return _overline; }
  bool fixedPitch()   const { return _fixedPitch; }
  int letterSpacing() const { return _letterSpacing; }
  int wordSpacing()   const { return _wordSpacing; }
  Style style()       const { return _style; }

  // TFT font size (1-4 maps to TFT_eSPI textSize)
  int tftSize() const {
    if (_pixelSize>0) return max(1,_pixelSize/8);
    if (_pointSize>0) return max(1,_pointSize/8);
    return 1;
  }

  bool operator==(const QFont& o) const {
    return _family==o._family&&_pointSize==o._pointSize&&_weight==o._weight&&_italic==o._italic;
  }
  bool operator!=(const QFont& o) const { return !(*this==o); }

  QString toString() const {
    return _family+","+String(_pointSize)+","+String(_weight)+","+(_italic?"italic":"normal");
  }

private:
  QString _family   = "default";
  int _pointSize    = 8;
  int _pixelSize    = -1;
  int _weight       = Normal;
  bool _italic      = false;
  bool _underline   = false;
  bool _strikeOut   = false;
  bool _overline    = false;
  bool _fixedPitch  = false;
  int _letterSpacing= 0;
  int _wordSpacing  = 0;
  Style _style      = StyleNormal;
  int _stretch      = 100;
  int _caps         = 0;
};

// ════════════════════════════════════════════════════════════════════════════
// QPoint / QPointF
// ════════════════════════════════════════════════════════════════════════════
class QPoint {
public:
  QPoint() : _x(0),_y(0) {}
  QPoint(int x,int y) : _x(x),_y(y) {}
  int x() const { return _x; }
  int y() const { return _y; }
  void setX(int x){ _x=x; }
  void setY(int y){ _y=y; }
  int& rx(){ return _x; }
  int& ry(){ return _y; }
  bool isNull() const { return _x==0&&_y==0; }
  QPoint operator+(const QPoint& o) const { return {_x+o._x,_y+o._y}; }
  QPoint operator-(const QPoint& o) const { return {_x-o._x,_y-o._y}; }
  QPoint operator*(int f)           const { return {_x*f,_y*f}; }
  QPoint operator/(int f)           const { return {_x/f,_y/f}; }
  QPoint& operator+=(const QPoint& o){ _x+=o._x;_y+=o._y;return *this; }
  QPoint& operator-=(const QPoint& o){ _x-=o._x;_y-=o._y;return *this; }
  bool operator==(const QPoint& o) const { return _x==o._x&&_y==o._y; }
  bool operator!=(const QPoint& o) const { return !(*this==o); }
  QPoint transposed() const { return {_y,_x}; }
  static int dotProduct(const QPoint& a,const QPoint& b){ return a._x*b._x+a._y*b._y; }
  float manhattanLength() const { return abs(_x)+abs(_y); }
private:
  int _x,_y;
};

class QPointF {
public:
  QPointF() : _x(0),_y(0) {}
  QPointF(float x,float y) : _x(x),_y(y) {}
  QPointF(QPoint p) : _x(p.x()),_y(p.y()) {}
  float x() const { return _x; }
  float y() const { return _y; }
  void setX(float x){ _x=x; }
  void setY(float y){ _y=y; }
  float& rx(){ return _x; }
  float& ry(){ return _y; }
  bool isNull() const { return _x==0&&_y==0; }
  QPointF operator+(const QPointF& o) const { return {_x+o._x,_y+o._y}; }
  QPointF operator-(const QPointF& o) const { return {_x-o._x,_y-o._y}; }
  QPointF operator*(float f)          const { return {_x*f,_y*f}; }
  QPointF operator/(float f)          const { return {_x/f,_y/f}; }
  bool operator==(const QPointF& o)   const { return abs(_x-o._x)<0.001f&&abs(_y-o._y)<0.001f; }
  QPoint toPoint() const { return {(int)roundf(_x),(int)roundf(_y)}; }
  float manhattanLength() const { return fabsf(_x)+fabsf(_y); }
  static float dotProduct(const QPointF& a,const QPointF& b){ return a._x*b._x+a._y*b._y; }
private:
  float _x,_y;
};

// ════════════════════════════════════════════════════════════════════════════
// QSize / QSizeF
// ════════════════════════════════════════════════════════════════════════════
class QSize {
public:
  QSize() : _w(-1),_h(-1) {}
  QSize(int w,int h) : _w(w),_h(h) {}
  int width()      const { return _w; }
  int height()     const { return _h; }
  void setWidth(int w)  { _w=w; }
  void setHeight(int h) { _h=h; }
  int& rwidth()         { return _w; }
  int& rheight()        { return _h; }
  bool isValid()   const { return _w>=0&&_h>=0; }
  bool isEmpty()   const { return _w<=0||_h<=0; }
  bool isNull()    const { return _w==0&&_h==0; }
  void transpose()       { std::swap(_w,_h); }
  QSize transposed()const{ return {_h,_w}; }
  QSize expandedTo(const QSize& o) const { return {max(_w,o._w),max(_h,o._h)}; }
  QSize boundedTo(const QSize& o)  const { return {min(_w,o._w),min(_h,o._h)}; }
  QSize grownBy(int m)             const { return {_w+m*2,_h+m*2}; }
  QSize shrunkBy(int m)            const { return {_w-m*2,_h-m*2}; }
  QSize scaled(int w,int h,Qt::AspectRatioMode mode=Qt::KeepAspectRatio) const {
    // stub — returns target size
    return {w,h};
  }
  QSize operator+(const QSize& o) const { return {_w+o._w,_h+o._h}; }
  QSize operator-(const QSize& o) const { return {_w-o._w,_h-o._h}; }
  QSize operator*(int f)          const { return {_w*f,_h*f}; }
  QSize operator/(int f)          const { return {_w/f,_h/f}; }
  bool operator==(const QSize& o) const { return _w==o._w&&_h==o._h; }
  bool operator!=(const QSize& o) const { return !(*this==o); }
private:
  int _w,_h;
};

class QSizeF {
public:
  QSizeF():_w(-1),_h(-1){}
  QSizeF(float w,float h):_w(w),_h(h){}
  QSizeF(QSize s):_w(s.width()),_h(s.height()){}
  float width()  const{return _w;}
  float height() const{return _h;}
  void setWidth(float w){_w=w;}
  void setHeight(float h){_h=h;}
  bool isValid() const{return _w>=0&&_h>=0;}
  bool isEmpty() const{return _w<=0||_h<=0;}
  bool isNull()  const{return _w==0&&_h==0;}
  QSize toSize() const{return{(int)roundf(_w),(int)roundf(_h)};}
  QSizeF expandedTo(const QSizeF& o)const{return{max(_w,o._w),max(_h,o._h)};}
  QSizeF boundedTo(const QSizeF& o) const{return{min(_w,o._w),min(_h,o._h)};}
  bool operator==(const QSizeF& o)const{return fabsf(_w-o._w)<0.001f&&fabsf(_h-o._h)<0.001f;}
private:
  float _w,_h;
};

// ════════════════════════════════════════════════════════════════════════════
// QMargins / QMarginsF
// ════════════════════════════════════════════════════════════════════════════
class QMargins {
public:
  QMargins():_l(0),_t(0),_r(0),_b(0){}
  QMargins(int l,int t,int r,int b):_l(l),_t(t),_r(r),_b(b){}
  int left()   const{return _l;} int top()    const{return _t;}
  int right()  const{return _r;} int bottom() const{return _b;}
  void setLeft(int l){_l=l;}  void setTop(int t){_t=t;}
  void setRight(int r){_r=r;} void setBottom(int b){_b=b;}
  bool isNull()const{return _l==0&&_t==0&&_r==0&&_b==0;}
  QMargins operator+(const QMargins& o)const{return{_l+o._l,_t+o._t,_r+o._r,_b+o._b};}
  QMargins operator*(int f)            const{return{_l*f,_t*f,_r*f,_b*f};}
  bool operator==(const QMargins& o)   const{return _l==o._l&&_t==o._t&&_r==o._r&&_b==o._b;}
  static QMargins uniform(int m){return{m,m,m,m};}
private:
  int _l,_t,_r,_b;
};

// ════════════════════════════════════════════════════════════════════════════
// QRect / QRectF
// ════════════════════════════════════════════════════════════════════════════
class QRect {
public:
  QRect():_x(0),_y(0),_w(0),_h(0){}
  QRect(int x,int y,int w,int h):_x(x),_y(y),_w(w),_h(h){}
  QRect(QPoint topLeft,QSize size):_x(topLeft.x()),_y(topLeft.y()),_w(size.width()),_h(size.height()){}
  QRect(QPoint topLeft,QPoint bottomRight):_x(topLeft.x()),_y(topLeft.y()),_w(bottomRight.x()-topLeft.x()),_h(bottomRight.y()-topLeft.y()){}

  int x()      const{return _x;} int y()      const{return _y;}
  int width()  const{return _w;} int height() const{return _h;}
  int left()   const{return _x;} int top()    const{return _y;}
  int right()  const{return _x+_w-1;} int bottom()const{return _y+_h-1;}
  void setX(int x){_x=x;}       void setY(int y){_y=y;}
  void setWidth(int w){_w=w;}    void setHeight(int h){_h=h;}
  void setLeft(int l){_x=l;}     void setTop(int t){_y=t;}
  void setRight(int r){_w=r-_x+1;} void setBottom(int b){_h=b-_y+1;}
  void setRect(int x,int y,int w,int h){_x=x;_y=y;_w=w;_h=h;}
  void getRect(int*x,int*y,int*w,int*h)const{*x=_x;*y=_y;*w=_w;*h=_h;}

  QPoint topLeft()     const{return{_x,_y};}
  QPoint topRight()    const{return{_x+_w-1,_y};}
  QPoint bottomLeft()  const{return{_x,_y+_h-1};}
  QPoint bottomRight() const{return{_x+_w-1,_y+_h-1};}
  QPoint center()      const{return{_x+_w/2,_y+_h/2};}
  QSize  size()        const{return{_w,_h};}

  void setTopLeft(QPoint p){_x=p.x();_y=p.y();}
  void setBottomRight(QPoint p){_w=p.x()-_x+1;_h=p.y()-_y+1;}
  void setTopRight(QPoint p){_w=p.x()-_x+1;_y=p.y();}
  void setBottomLeft(QPoint p){_x=p.x();_h=p.y()-_y+1;}
  void setSize(QSize s){_w=s.width();_h=s.height();}
  void moveTo(int x,int y){_x=x;_y=y;}
  void moveTo(QPoint p){_x=p.x();_y=p.y();}
  void moveLeft(int l){_x=l;}    void moveTop(int t){_y=t;}
  void moveRight(int r){_x=r-_w+1;} void moveBottom(int b){_y=b-_h+1;}
  void moveCenter(QPoint p){_x=p.x()-_w/2;_y=p.y()-_h/2;}
  void translate(int dx,int dy){_x+=dx;_y+=dy;}
  void translate(QPoint p){_x+=p.x();_y+=p.y();}
  QRect translated(int dx,int dy) const{return{_x+dx,_y+dy,_w,_h};}
  QRect translated(QPoint p)      const{return{_x+p.x(),_y+p.y(),_w,_h};}
  void adjust(int l,int t,int r,int b){_x+=l;_y+=t;_w+=r-l;_h+=b-t;}
  QRect adjusted(int l,int t,int r,int b)const{return{_x+l,_y+t,_w+r-l,_h+b-t};}
  QRect marginsAdded(QMargins m)const{return adjusted(-m.left(),-m.top(),m.right(),m.bottom());}
  QRect marginsRemoved(QMargins m)const{return adjusted(m.left(),m.top(),-m.right(),-m.bottom());}
  QRect operator|(const QRect& o)const{return united(o);}
  QRect operator&(const QRect& o)const{return intersected(o);}

  bool isValid()  const{return _w>0&&_h>0;}
  bool isEmpty()  const{return _w<=0||_h<=0;}
  bool isNull()   const{return _w==0&&_h==0;}
  bool contains(int x,int y)       const{return x>=_x&&x<_x+_w&&y>=_y&&y<_y+_h;}
  bool contains(QPoint p)          const{return contains(p.x(),p.y());}
  bool contains(const QRect& r)    const{return r._x>=_x&&r._y>=_y&&r._x+r._w<=_x+_w&&r._y+r._h<=_y+_h;}
  bool intersects(const QRect& r)  const{return !(_x>=r._x+r._w||_x+_w<=r._x||_y>=r._y+r._h||_y+_h<=r._y);}
  QRect intersected(const QRect& r)const{
    int x1=max(_x,r._x),y1=max(_y,r._y),x2=min(_x+_w,r._x+r._w),y2=min(_y+_h,r._y+r._h);
    if(x1>=x2||y1>=y2)return{};
    return{x1,y1,x2-x1,y2-y1};
  }
  QRect united(const QRect& r)const{
    if(!isValid())return r;if(!r.isValid())return *this;
    int x1=min(_x,r._x),y1=min(_y,r._y),x2=max(_x+_w,r._x+r._w),y2=max(_y+_h,r._y+r._h);
    return{x1,y1,x2-x1,y2-y1};
  }
  bool operator==(const QRect& o)const{return _x==o._x&&_y==o._y&&_w==o._w&&_h==o._h;}
  bool operator!=(const QRect& o)const{return !(*this==o);}
  void normalize(){if(_w<0){_x+=_w;_w=-_w;}if(_h<0){_y+=_h;_h=-_h;}}
  QRect normalized()const{QRect r=*this;r.normalize();return r;}
  int area() const{return _w*_h;}

private:
  int _x,_y,_w,_h;
};

class QRectF {
public:
  QRectF():_x(0),_y(0),_w(0),_h(0){}
  QRectF(float x,float y,float w,float h):_x(x),_y(y),_w(w),_h(h){}
  QRectF(QPointF tl,QSizeF s):_x(tl.x()),_y(tl.y()),_w(s.width()),_h(s.height()){}
  QRectF(QRect r):_x(r.x()),_y(r.y()),_w(r.width()),_h(r.height()){}
  float x()const{return _x;}float y()const{return _y;}
  float width()const{return _w;}float height()const{return _h;}
  float left()const{return _x;}float top()const{return _y;}
  float right()const{return _x+_w;}float bottom()const{return _y+_h;}
  QPointF center()const{return{_x+_w/2,_y+_h/2};}
  QSizeF size()const{return{_w,_h};}
  QRect toRect()const{return{(int)_x,(int)_y,(int)ceilf(_w),(int)ceilf(_h)};}
  QRect toAlignedRect()const{return toRect();}
  bool contains(float x,float y)const{return x>=_x&&x<_x+_w&&y>=_y&&y<_y+_h;}
  bool contains(QPointF p)const{return contains(p.x(),p.y());}
  bool isValid()const{return _w>0&&_h>0;}
  bool isEmpty()const{return _w<=0||_h<=0;}
  void translate(float dx,float dy){_x+=dx;_y+=dy;}
  QRectF translated(float dx,float dy)const{return{_x+dx,_y+dy,_w,_h};}
  QRectF adjusted(float l,float t,float r,float b)const{return{_x+l,_y+t,_w+r-l,_h+b-t};}
  bool intersects(const QRectF& r)const{return !(_x>=r._x+r._w||_x+_w<=r._x||_y>=r._y+r._h||_y+_h<=r._y);}
  bool operator==(const QRectF& o)const{return fabsf(_x-o._x)<0.001f&&fabsf(_y-o._y)<0.001f&&fabsf(_w-o._w)<0.001f&&fabsf(_h-o._h)<0.001f;}
private:
  float _x,_y,_w,_h;
};

// ════════════════════════════════════════════════════════════════════════════
// QLine / QLineF
// ════════════════════════════════════════════════════════════════════════════
class QLine {
public:
  QLine():_x1(0),_y1(0),_x2(0),_y2(0){}
  QLine(int x1,int y1,int x2,int y2):_x1(x1),_y1(y1),_x2(x2),_y2(y2){}
  QLine(QPoint p1,QPoint p2):_x1(p1.x()),_y1(p1.y()),_x2(p2.x()),_y2(p2.y()){}
  int x1()const{return _x1;}int y1()const{return _y1;}
  int x2()const{return _x2;}int y2()const{return _y2;}
  QPoint p1()const{return{_x1,_y1};}
  QPoint p2()const{return{_x2,_y2};}
  QPoint center()const{return{(_x1+_x2)/2,(_y1+_y2)/2};}
  QPoint pointAt(float t)const{return{(int)(_x1+(_x2-_x1)*t),(int)(_y1+(_y2-_y1)*t)};}
  int dx()const{return _x2-_x1;}
  int dy()const{return _y2-_y1;}
  bool isNull()const{return _x1==_x2&&_y1==_y2;}
  void setP1(QPoint p){_x1=p.x();_y1=p.y();}
  void setP2(QPoint p){_x2=p.x();_y2=p.y();}
  void translate(int dx,int dy){_x1+=dx;_y1+=dy;_x2+=dx;_y2+=dy;}
  bool operator==(const QLine& o)const{return _x1==o._x1&&_y1==o._y1&&_x2==o._x2&&_y2==o._y2;}
private:
  int _x1,_y1,_x2,_y2;
};

class QLineF {
public:
  QLineF():_x1(0),_y1(0),_x2(0),_y2(0){}
  QLineF(float x1,float y1,float x2,float y2):_x1(x1),_y1(y1),_x2(x2),_y2(y2){}
  QLineF(QPointF p1,QPointF p2):_x1(p1.x()),_y1(p1.y()),_x2(p2.x()),_y2(p2.y()){}
  QLineF(QLine l):_x1(l.x1()),_y1(l.y1()),_x2(l.x2()),_y2(l.y2()){}
  float x1()const{return _x1;}float y1()const{return _y1;}
  float x2()const{return _x2;}float y2()const{return _y2;}
  QPointF p1()const{return{_x1,_y1};}
  QPointF p2()const{return{_x2,_y2};}
  float dx()const{return _x2-_x1;}
  float dy()const{return _y2-_y1;}
  float length()const{return sqrtf(dx()*dx()+dy()*dy());}
  float angle()const{return atan2f(dy(),dx())*180/M_PI;}
  bool isNull()const{return length()<0.001f;}
  QPointF pointAt(float t)const{return{_x1+dx()*t,_y1+dy()*t};}
  QPointF center()const{return{(_x1+_x2)/2,(_y1+_y2)/2};}
  void translate(float dx2,float dy2){_x1+=dx2;_y1+=dy2;_x2+=dx2;_y2+=dy2;}
  QLineF normalVector()const{float d=length();return{_x1,_y1,_x1-dy()/d,_y1+dx()/d};}
  QLineF unitVector()const{float d=length();return{_x1,_y1,_x1+dx()/d,_y1+dy()/d};}
  QLine toLine()const{return{(int)_x1,(int)_y1,(int)_x2,(int)_y2};}
private:
  float _x1,_y1,_x2,_y2;
};

// ════════════════════════════════════════════════════════════════════════════
// QPolygon / QPolygonF
// ════════════════════════════════════════════════════════════════════════════
class QPolygon : public QList<QPoint> {
public:
  QPolygon(){}
  QPolygon(std::initializer_list<QPoint> pts):QList<QPoint>(pts){}
  QRect boundingRect() const {
    if(empty())return{};
    int x1=(*this)[0].x(),y1=(*this)[0].y(),x2=x1,y2=y1;
    for(auto&p:*this){x1=min(x1,p.x());y1=min(y1,p.y());x2=max(x2,p.x());y2=max(y2,p.y());}
    return{x1,y1,x2-x1,y2-y1};
  }
  bool containsPoint(QPoint p) const {
    // Ray casting
    bool inside=false;
    int n=size();
    for(int i=0,j=n-1;i<n;j=i++){
      auto& pi=(*this)[i];auto& pj=(*this)[j];
      if(((pi.y()>p.y())!=(pj.y()>p.y()))&&(p.x()<(pj.x()-pi.x())*(p.y()-pi.y())/(pj.y()-pi.y())+pi.x()))
        inside=!inside;
    }
    return inside;
  }
  void translate(int dx,int dy){for(auto&p:*this){p.setX(p.x()+dx);p.setY(p.y()+dy);}}
  QPolygon translated(int dx,int dy)const{QPolygon r=*this;r.translate(dx,dy);return r;}
};

// ════════════════════════════════════════════════════════════════════════════
// QTransform (2D affine transform, mirrors Qt6)
// ════════════════════════════════════════════════════════════════════════════
class QTransform {
public:
  // Matrix: [m11 m12 dx] [m21 m22 dy] [0 0 1]
  QTransform():_m11(1),_m12(0),_m21(0),_m22(1),_dx(0),_dy(0){}
  QTransform(float m11,float m12,float m21,float m22,float dx,float dy)
    :_m11(m11),_m12(m12),_m21(m21),_m22(m22),_dx(dx),_dy(dy){}

  float m11()const{return _m11;}float m12()const{return _m12;}
  float m21()const{return _m21;}float m22()const{return _m22;}
  float dx()const{return _dx;}  float dy()const{return _dy;}

  static QTransform fromTranslate(float dx,float dy){return{1,0,0,1,dx,dy};}
  static QTransform fromScale(float sx,float sy)    {return{sx,0,0,sy,0,0};}
  static QTransform fromRotate(float angle) {
    float rad=angle*M_PI/180, c=cosf(rad),s=sinf(rad);
    return{c,-s,s,c,0,0};
  }

  QTransform& translate(float dx,float dy){_dx+=dx;_dy+=dy;return *this;}
  QTransform& scale(float sx,float sy){_m11*=sx;_m22*=sy;return *this;}
  QTransform& rotate(float angle){
    float rad=angle*M_PI/180,c=cosf(rad),s=sinf(rad);
    float nm11=_m11*c+_m12*s, nm12=-_m11*s+_m12*c;
    float nm21=_m21*c+_m22*s, nm22=-_m21*s+_m22*c;
    _m11=nm11;_m12=nm12;_m21=nm21;_m22=nm22;
    return *this;
  }
  QTransform& shear(float sh,float sv){
    _m12+=_m11*sv;_m11+=_m12*sh;
    _m21+=_m22*sh;_m22+=_m21*sv;
    return *this;
  }
  QTransform inverted(bool* ok=nullptr) const {
    float det=_m11*_m22-_m12*_m21;
    if(fabsf(det)<0.0001f){if(ok)*ok=false;return{};}
    if(ok)*ok=true;
    float inv=1/det;
    return{_m22*inv,-_m12*inv,-_m21*inv,_m11*inv,
           (_m21*_dy-_m22*_dx)*inv,(_m12*_dx-_m11*_dy)*inv};
  }
  QTransform operator*(const QTransform& o) const {
    return{_m11*o._m11+_m12*o._m21, _m11*o._m12+_m12*o._m22,
           _m21*o._m11+_m22*o._m21, _m21*o._m12+_m22*o._m22,
           _dx*o._m11+_dy*o._m21+o._dx, _dx*o._m12+_dy*o._m22+o._dy};
  }
  QPointF map(QPointF p) const {
    return{_m11*p.x()+_m21*p.y()+_dx, _m12*p.x()+_m22*p.y()+_dy};
  }
  QPoint map(QPoint p) const { return map(QPointF(p)).toPoint(); }
  bool isIdentity()const{return _m11==1&&_m12==0&&_m21==0&&_m22==1&&_dx==0&&_dy==0;}
  bool isTranslating()const{return _m11==1&&_m12==0&&_m21==0&&_m22==1;}
  bool isScaling()const{return _m12==0&&_m21==0;}
  bool isRotating()const{return _m12!=0||_m21!=0;}
  void reset(){_m11=1;_m12=0;_m21=0;_m22=1;_dx=0;_dy=0;}

private:
  float _m11,_m12,_m21,_m22,_dx,_dy;
};

// ════════════════════════════════════════════════════════════════════════════
// QDateTime / QDate / QTime (lightweight, uses millis())
// ════════════════════════════════════════════════════════════════════════════
class QTime {
public:
  QTime():_h(0),_m(0),_s(0),_ms(0){}
  QTime(int h,int m,int s=0,int ms=0):_h(h),_m(m),_s(s),_ms(ms){}
  int hour()   const{return _h;} int minute()const{return _m;}
  int second() const{return _s;} int msec()  const{return _ms;}
  bool isValid()const{return _h>=0&&_h<24&&_m>=0&&_m<60&&_s>=0&&_s<60&&_ms>=0&&_ms<1000;}
  int  msecsTo(QTime o)const{return (o._h*3600000+o._m*60000+o._s*1000+o._ms)-(_h*3600000+_m*60000+_s*1000+_ms);}
  int  secsTo(QTime o) const{return msecsTo(o)/1000;}
  QTime addMSecs(int ms)const{int total=_h*3600000+_m*60000+_s*1000+_ms+ms;total%=86400000;if(total<0)total+=86400000;return{total/3600000,(total%3600000)/60000,(total%60000)/1000,total%1000};}
  QTime addSecs(int s)  const{return addMSecs(s*1000);}
  QString toString(const QString& fmt="hh:mm:ss")const{
    char buf[20]; snprintf(buf,sizeof(buf),"%02d:%02d:%02d",_h,_m,_s); return String(buf);
  }
  static QTime currentTime(){unsigned long ms=millis();return{(int)(ms/3600000)%24,(int)(ms/60000)%60,(int)(ms/1000)%60,(int)(ms%1000)};}
  bool operator==(const QTime& o)const{return _h==o._h&&_m==o._m&&_s==o._s&&_ms==o._ms;}
  bool operator<(const QTime& o) const{return msecsTo(o)>0;}
private:
  int _h,_m,_s,_ms;
};

class QDate {
public:
  QDate():_y(2024),_m(1),_d(1){}
  QDate(int y,int m,int d):_y(y),_m(m),_d(d){}
  int year() const{return _y;} int month()const{return _m;} int day()const{return _d;}
  bool isValid()const{return _y>0&&_m>=1&&_m<=12&&_d>=1&&_d<=31;}
  QString toString(const QString&)const{ char buf[12];snprintf(buf,sizeof(buf),"%04d-%02d-%02d",_y,_m,_d);return String(buf);}
  bool operator==(const QDate& o)const{return _y==o._y&&_m==o._m&&_d==o._d;}
  bool operator<(const QDate& o) const{return _y<o._y||(_y==o._y&&_m<o._m)||(_y==o._y&&_m==o._m&&_d<o._d);}
private:
  int _y,_m,_d;
};

class QDateTime {
public:
  QDateTime(){}
  QDateTime(QDate d,QTime t):_date(d),_time(t){}
  QDate date()const{return _date;}
  QTime time()const{return _time;}
  bool isValid()const{return _date.isValid()&&_time.isValid();}
  qint64 toMSecsSinceEpoch()const{return millis();}
  static QDateTime currentDateTime(){return{QDate(2024,1,1),QTime::currentTime()};}
  QString toString(const QString& fmt="")const{return _date.toString("")+" "+_time.toString("");}
private:
  QDate _date;
  QTime _time;
};

// ════════════════════════════════════════════════════════════════════════════
// QTimer (proper Qt6 API)
// ════════════════════════════════════════════════════════════════════════════
class QTimer : public QObject {
public:
  explicit QTimer(QObject* parent=nullptr) : QObject(parent) {}

  void start(int msec)         { _interval=msec; _active=true; _lastFire=millis(); emit_signal("started"); }
  void start()                 { start(_interval); }
  void stop()                  { _active=false; emit_signal("stopped"); }
  bool isActive()    const     { return _active; }
  int  interval()    const     { return _interval; }
  int  remainingTime()const    { if(!_active)return -1; return max(0,(int)(_interval-(int)(millis()-_lastFire))); }
  bool isSingleShot()const     { return _singleShot; }
  void setInterval(int ms)     { _interval=ms; }
  void setSingleShot(bool s)   { _singleShot=s; }

  // Must be called from loop()
  void tick() {
    if (!_active) return;
    unsigned long now=millis();
    if (now-_lastFire>=(unsigned long)_interval) {
      _lastFire=now;
      emit_signal("timeout");
      if (_singleShot) stop();
    }
  }

  static void singleShot(int ms, std::function<void()> fn) {
    // Non-blocking: store and fire once (requires loop() call)
    // For simplicity on ESP32, use delay-based approach
    // In real use: create a timer and connect
    delay(ms); fn();
  }

  const char* metaClassName() const override { return "QTimer"; }

  // Signals: timeout, started, stopped

private:
  int  _interval   = 1000;
  bool _active     = false;
  bool _singleShot = false;
  unsigned long _lastFire = 0;
};

} // namespace NoorQt

// ── Convenience using declarations ────────────────────────────────────────────
using NoorQt::QColor;
using NoorQt::QFont;
using NoorQt::QPoint;
using NoorQt::QPointF;
using NoorQt::QSize;
using NoorQt::QSizeF;
using NoorQt::QRect;
using NoorQt::QRectF;
using NoorQt::QMargins;
using NoorQt::QLine;
using NoorQt::QLineF;
using NoorQt::QPolygon;
using NoorQt::QTransform;
using NoorQt::QTime;
using NoorQt::QDate;
using NoorQt::QDateTime;
using NoorQt::QTimer;
