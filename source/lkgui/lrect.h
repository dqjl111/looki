#ifndef LRECT_H
#define LRECT_H

#include "lkgui/lmargins.h"
#include "lkgui/lpoint.h"
#include "lkcore/ldebug.h"

LK_NAMESPACE_BEGIN

#define IS_LOG_INVALID＿RECT 0

#define LOG_INVALID＿RECT(i) \
    if(m_width <= 0 || m_height <=0){ \
        if(m_x != 0 || m_y != 0){ \
            lWarn() << "Init invalid rect" << i <<":" << m_x << "," << m_y << "," << m_width << "," << m_height; \
        } \
    }

template<typename T>
class LRect
{
public:
    T m_x = 0;
    T m_y = 0;
    T m_width = 0;
    T m_height = 0;

    LRect() noexcept{
    }

    LRect(T x1, T y1, T x2, T y2, bool isCoords = true){
        if(isCoords){
            setCoords(x1, y1, x2, y2);
        }else{
            m_x = x1;
            m_y = y1;
            m_width = x2;
            m_height = y2;
#if IS_LOG_INVALID＿RECT
            LOG_INVALID＿RECT(0)
#endif
        }
    }

    LRect(const LRect<T>& s):m_x(s.m_x), m_y(s.m_y), m_width(s.m_width), m_height(s.m_height){
#if IS_LOG_INVALID＿RECT
        LOG_INVALID＿RECT(2)
#endif
    }

    LRect(const LPoint<T> &topleft, const LPoint<T> &bottomright)
        :LRect<T>(topleft.m_x, topleft.m_y, bottomright.m_x, bottomright.m_y){
#if IS_LOG_INVALID＿RECT
        LOG_INVALID＿RECT(3)
#endif
    }

    LRect(const LPoint<T> &topleft, T width, T height)
        : m_x(topleft.m_x), m_y(topleft.m_y), m_width(width), m_height(height){
#if IS_LOG_INVALID＿RECT
        LOG_INVALID＿RECT(4)
#endif
    }

    LRect(T left, T top, const LSize<T>& size)
        :m_x(left), m_y(top), m_width(size.m_x), m_height(size.m_y){
#if IS_LOG_INVALID＿RECT
        LOG_INVALID＿RECT(5)
#endif
    }

    LRect(const LPoint<T> &topleft, const LSize<T>& size)
        : m_x(topleft.m_x), m_y(topleft.m_y), m_width(size.m_x), m_height(size.m_y){
#if IS_LOG_INVALID＿RECT
        LOG_INVALID＿RECT(6)
#endif
    }

    LRect(const LCbor& v){
        fromCbor(v);
    }

    void reset(){
        m_x = 0;
        m_y = 0;
        m_width = 0;
        m_height = 0;
    }

    //整数左右相等，上下相等的时候，可以是线或点   
    inline bool isNull() const noexcept
    { return lFuzzyIsZero(m_width) || lFuzzyIsZero(m_height); }


    inline bool isEmpty() const noexcept
    { return m_width <= 0 || m_height <= 0; }

    inline bool isValid() const noexcept
    { return m_width >0 && m_height >0; }

    inline T x1() const{
        return m_x;
    }

    inline T y1() const{
        return m_y;
    }

    inline T x2() const{
        return m_x + m_width;
    }

    inline T y2() const{
        return m_y + m_height;
    }

    inline void setX1(const T& v, bool keepWidth = false){
        if(keepWidth){
            m_x = v;
        }else{
            m_width = m_x+m_width-v;
            m_x = v;
        }
    }

    inline void setY1(const T& v, bool keepHeight = false){
        if(keepHeight){
            m_y = v;
        }else{
            m_height = m_y+m_height-v;
            m_y = v;
        }
    }

    inline void setX2(const T& v, bool keepWidth = false){
        if(keepWidth){
            m_x = v-m_width;
        }else{
            m_width = v - m_x;
        }
    }

    inline void setY2(const T& v, bool keepHeight = false){
        if(keepHeight){
            m_y = v-m_height;
        }else{
            m_height = v-m_y;
        }
    }


    inline LPoint<T> topLeft() const noexcept
    { return LPoint<T>(m_x, m_y); }

    inline LPoint<T> bottomRight() const noexcept
    { return LPoint<T>(m_x+m_width, m_y+m_height); }

    inline LPoint<T> topRight() const noexcept
    { return LPoint<T>(m_x+m_width, m_y); }

    inline LPoint<T> bottomLeft() const noexcept
    { return LPoint<T>(m_x, m_y+m_height); }


    inline void setTopLeft(const LPoint<T> &p) noexcept
    { 
        m_x = p.m_x;
        m_y = p.m_y;
    }

    inline void setBottomRight(const LPoint<T> &p) noexcept
    { 
        m_width = p.m_x - m_x;
        m_height = p.m_y - m_y;
    }

    inline void setTopRight(const LPoint<T> &p) noexcept
    { 
        m_width = p.m_x - m_x;
        m_y = p.m_y;
    }

    inline void setBottomLeft(const LPoint<T> &p) noexcept
    { 
        m_x = p.m_x;
        m_height = p.m_y - m_y;
    }


    inline LPoint<T> center() const noexcept
    { return LPoint<T>(m_x+ m_width/2, m_y+ m_height/2); }

    inline T width() const noexcept
    { return  m_width; }

    inline T height() const noexcept
    { return  m_height; }

    inline LSize<T> size() const noexcept
    { return LSize<T>(width(), height()); }


    inline void setCoords(T x1, T y1, T x2, T y2) noexcept
    {
        if(x2>=x1){
            m_x = x1;
            m_width = x2-x1;
        }else{
            m_x = x2;
            m_width = x1-x2;
        }

        if(y2>=y1){
            m_y = y1;
            m_height = y2-y1;
        }else{
            m_y = y2;
            m_height = y1-y2;
        }
#if IS_LOG_INVALID＿RECT
        LOG_INVALID＿RECT(1)
#endif
    }

    inline void setRect(T xp1, T yp1, T w, T h) noexcept
    {
        m_x = xp1;
        m_y = yp1;
        m_width = w;
        m_height = h;
    }

    inline void translate(T dx, T dy) noexcept
    {
        m_x += dx;
        m_y += dy;
    }

    inline void translate(const LPoint<T> &p) noexcept
    {
        m_x += p.m_x;
        m_y += p.m_y;
    }

    inline LRect<T> translated(T dx, T dy) const noexcept{
        return LRect<T>(m_x + dx, m_y + dy, m_width, m_height, false);
    }

    inline LRect<T> translated(const LPoint<T> &p) const noexcept{
        return LRect<T>(m_x+p.m_x, m_y + p.m_y, m_width, m_height, false);
    }

    inline LRect<T> transposed() const noexcept{
        return LRect<T>(m_x, m_y, m_height, m_width, false);
    }

    inline void moveTo(T ax, T ay) noexcept{
        m_x = ax;
        m_y = ay;
    }

    inline void moveTo(const LPoint<T> &p) noexcept{
        m_x = p.m_x;
        m_y = p.m_x;
    }

    inline void moveLeft(T pos) noexcept{
        m_x = pos;
    }

    inline void moveTop(T pos) noexcept{
        m_y = pos;
    }

    inline void moveRight(T pos) noexcept{
        m_x = pos - m_width;
    }

    inline void moveBottom(T pos) noexcept{
        m_y = pos - m_height;
    }

    inline void moveTopLeft(const LPoint<T> &p) noexcept{
        moveLeft(p.m_x);
        moveTop(p.m_y);
    }

    inline void moveBottomRight(const LPoint<T> &p) noexcept{
        moveRight(p.m_x);
        moveBottom(p.m_y);
    }

    inline void moveTopRight(const LPoint<T> &p) noexcept{
        moveRight(p.m_x);
        moveTop(p.m_y);
    }

    inline void moveBottomLeft(const LPoint<T> &p) noexcept{
        moveLeft(p.m_x);
        moveBottom(p.m_y);
    }

    inline void moveCenter(const LPoint<T> &p) noexcept
    {
        m_x = p.m_x - m_width/2;
        m_y = p.m_y - m_height/2;
    }


    inline LRect<T> adjusted(T xp1, T yp1, T xp2, T yp2) const noexcept
    { return LRect<T>(m_x + xp1, m_y + yp1, LSize<T>(m_width+xp2-xp1, m_height+yp2-yp1)); }

    inline void adjust(T dx1, T dy1, T dx2, T dy2) noexcept
    {
        m_x += dx1;
        m_y += dy1;

        m_width += dx2-dx1;
        m_height += dy2-dy1;
    }


    inline void setWidth(T w) noexcept
    { m_width = w; }

    inline void setHeight(T h) noexcept
    { m_height = h; }

    inline void setSize(const LSize<T> &s) noexcept
    {
        m_width = s.m_x;
        m_height = s.m_y;
    }

    inline bool contains(T ax, T ay) const noexcept
    {
        return contains(LPoint<T>(ax, ay));
    }


    bool contains(const LPoint<T>&p) const noexcept
    {
        if(isNull()){
            return false;
        }

        T l = m_x;
        T r = m_x;
        if (m_width < 0){
            l += m_width;
        }
        else{
            r += m_width;
        }


        if (p.m_x < l || p.m_x >= r){
            return false;
        }

        T t = m_y;
        T b = m_y;
        if (m_height < 0){
            t += m_height;
        }
        else{
            b += m_height;
        }

        if (p.m_y < t || p.m_y >= b){
            return false;
        }

        return true;
    }

    bool contains(const LRect<T> &r) const noexcept
    {
        if (isNull() || r.isNull()){
            return false;
        }

        T l1 = m_x;
        T r1 = m_x;
        if (m_width < 0){
            l1 += m_width;
        }
        else{
            r1 += m_width;
        }

        T l2 = r.m_x;
        T r2 = r.m_x;
        if (r.m_width < 0){
            l2 += r.m_width;
        }
        else{
            r2 += r.m_width;
        }

        if (l2 < l1 || r2 > r1){
            return false;
        }

        T t1 = m_y;
        T b1 = m_y;
        if (m_height < 0){
            t1 += m_height;
        }
        else{
            b1 += m_height;
        }

        T t2 = r.m_y;
        T b2 = r.m_y;
        if (r.m_height < 0){
            t2 += r.m_height;
        }
        else{
            b2 += r.m_height;
        }

        if (t2 < t1 || b2 > b1){
            return false;
        }
        return true;
    }

    LRect<T>& operator=(const LRect<T> &r) = default;

    LRect<T> operator|(const LRect<T> &r) const noexcept
    {
        if (isNull()){
            return r;
        }
        if (r.isNull()){
            return *this;
        }

        T l1 = m_x;
        T r1 = m_x;
        if (m_width < 0){
            l1 += m_width;
        }
        else{
            r1 += m_width;
        }

        T l2 = r.m_x;
        T r2 = r.m_x;
        if (r.m_width < 0){
            l2 += r.m_width;
        }
        else{
            r2 += r.m_width;
        }

        T t1 = m_y;
        T b1 = m_y;
        if (m_height < 0){
            t1 += m_height;
        }
        else{
            b1 += m_height;
        }

        T t2 = r.m_y;
        T b2 = r.m_y;
        if (r.m_height < 0){
            t2 += r.m_height;
        }
        else{
            b2 += r.m_height;
        }


        T m_left = std::min<T>(l1, l2);
        T m_right = std::max<T>(r1, r2);
        T m_top = std::min<T>(t1, t2);
        T m_bottom = std::max<T>(b1, b2);
        return LRect<T>(m_left, m_top, LSize<T>(m_right-m_left, m_bottom-m_top));
    }

    LRect<T> operator&(const LRect<T> &r) const noexcept
    {
        if (isNull() || r.isNull()){
            return LRect<T>();
        }

        T l1 = m_x;
        T r1 = m_x;
        if (m_width < 0){
            l1 += m_width;
        }
        else{
            r1 += m_width;
        }

        T l2 = r.m_x;
        T r2 = r.m_x;
        if (r.m_width < 0){
            l2 += r.m_width;
        }
        else{
            r2 += r.m_width;
        }

        T t1 = m_y;
        T b1 = m_y;
        if (m_height < 0){
            t1 += m_height;
        }
        else{
            b1 += m_height;
        }

        T t2 = r.m_y;
        T b2 = r.m_y;
        if (r.m_height < 0){
            t2 += r.m_height;
        }
        else{
            b2 += r.m_height;
        }


        T m_left  = std::max<T>(l1, l2);
        T m_right = std::min<T>(r1, r2);
        T m_top = std::max<T>(t1, t2);
        T m_bottom = std::min<T>(b1, b2);
        return LRect<T>(m_left, m_top, LSize<T>(m_right-m_left, m_bottom-m_top));
    }

    inline LRect<T>& operator|=(const LRect<T> &r) noexcept{
        *this = *this | r;
        return *this;
    }

    inline LRect<T>& operator&=(const LRect<T> &r) noexcept{
        *this = *this & r;
        return *this;
    }

    inline LRect<T> intersected(const LRect<T> &r) const noexcept{
        return *this & r;
    }

    inline LRect<T> united(const LRect<T> &r) const noexcept{
        return *this | r;
    }

    inline LRect<T> marginsAdded(const LMargins<T> &margins) const noexcept{
        return LRect<T>(m_x - margins.m_left, m_y - margins.m_top,
                        LSize<T>(m_width + margins.m_left + margins.m_right,
                                 m_height + margins.m_top + margins.m_bottom));
    }

    inline LRect<T> marginsRemoved(const LMargins<T> &margins) const noexcept{
        return LRect<T>(m_x + margins.m_left, m_y + margins.m_top,
                        LSize<T>(m_width - margins.m_left - margins.m_right,
                                 m_height - margins.m_top - margins.m_bottom));
    }

    inline LRect<T> &operator+=(const LMargins<T> &margins) noexcept{
        *this = marginsAdded(margins);
        return this;
    }

    inline LRect<T> &operator-=(const LMargins<T> &margins) noexcept{
        *this = marginsRemoved(margins);
        return this;
    }


    LRect<T> normalized() const noexcept{
        LRect<T> r = *this;
        if (r.m_width < 0) {
            r.m_x += r.m_width;
            r.m_width = -r.m_width;
        }
        if (r.m_height < 0) {
            r.m_y += r.m_height;
            r.m_height = -r.m_height;
        }
        return r;
    }

    LRect<int> toRect() const
    {
        return LRect<int>(std::lround(m_x), std::lround(m_y), LSize<int>(std::lround(m_width), std::lround(m_height)));
    }

    LRect<double> toRectF() const
    {
        return LRect<double>(m_x, m_y, m_width, m_height, false);
    }

    LRect<int> toAlignedRect() const
    {
        return LRect<int>(std::floor(m_x), std::floor(m_y), std::ceil(m_x+m_width), std::ceil(m_y+m_height), true);
    }

    bool isIntersects(const LRect<T> &r) const noexcept{
        if (isNull() || r.isNull()){
            return false;
        }

        T l1 = m_x;
        T r1 = m_x;
        if (m_width < 0){
            l1 += m_width;
        }
        else{
            r1 += m_width;
        }

        T l2 = r.m_x;
        T r2 = r.m_x;
        if (r.m_width < 0){
            l2 += r.m_width;
        }
        else{
            r2 += r.m_width;
        }

        if (l1 >= r2 || l2 >= r1){
            return false;
        }

        T t1 = m_y;
        T b1 = m_y;
        if (m_height < 0){
            t1 += m_height;
        }
        else{
            b1 += m_height;
        }

        T t2 = r.m_y;
        T b2 = r.m_y;
        if (r.m_height < 0){
            t2 += r.m_height;
        }
        else{
            b2 += r.m_height;
        }

        if (t1 >= b2 || t2 >= b1){
            return false;
        }
        return true;
    }

    LCbor toCbor(){
        LCbor::map ret;
        ret[0] = m_x;
        ret[1] = m_y;
        ret[2] = m_width;
        ret[3] = m_height;
        return ret;
    }

    void fromCbor(const LCbor& v){
        LCbor::map t = v.toMap();
        m_x = t[0];
        m_y = t[1];
        m_width = t[2];
        m_height = t[3];
    }


    LRect<T> &operator*=(int) noexcept;
    LRect<T> &operator/=(int);
    LRect<T> &operator*=(double) noexcept;
    LRect<T> &operator/=(double);
};

template<typename T>
inline bool operator==(const LRect<T> &r1, const LRect<T> &r2) noexcept
{
    return lFuzzyIsZero(std::abs(r1.m_x-r2.m_x))
           && lFuzzyIsZero(std::abs(r1.m_y-r2.m_y))
           && lFuzzyIsZero(std::abs(r1.m_width-r2.m_width))
           && lFuzzyIsZero(std::abs(r1.m_height-r2.m_height));
}

template<typename T>
inline bool operator!=(const LRect<T> &r1, const LRect<T> &r2) noexcept
{
    return !lFuzzyIsZero(std::abs(r1.m_x-r2.m_x))|| !lFuzzyIsZero(std::abs(r1.m_y-r2.m_y))
           || !lFuzzyIsZero(std::abs(r1.m_width-r2.m_width)) || !lFuzzyIsZero(std::abs(r1.m_height-r2.m_height));
}


template<typename T>
inline LRect<T> operator*(const LRect<T> &rect, int factor) noexcept
{
    return LRect<T>(rect.m_x * factor, rect.m_y * factor,
                    LSize<T>(rect.m_width * factor, rect.m_height * factor));
}

template<typename T>
inline LRect<T> operator*(int factor, const LRect<T> &rect) noexcept
{
    return LRect<T>(rect.m_x * factor, rect.m_y * factor,
                    LSize<T>(rect.m_width * factor, rect.m_height * factor));
}

inline LRect<int> operator*(const LRect<int> &rect, double factor) noexcept
{
    return LRect<int>(std::lround(rect.m_x * factor), std::lround(rect.m_y * factor),
                      LSize<int>(std::lround(rect.m_width * factor), std::lround(rect.m_height * factor)));
}

inline LRect<double> operator*(const LRect<double> &rect, double factor) noexcept
{
    return LRect<double>(rect.m_x * factor, rect.m_y * factor,
                         LSize<double>(rect.m_width * factor, rect.m_height * factor));
}

template<typename T>
inline LRect<int> operator*(double factor, const LRect<T> &rect) noexcept
{
    return operator*(rect, factor);
}

template<typename T>
inline LRect<T> operator/(const LRect<T> &rect, int divisor)
{
    return LRect<T>(rect.m_x / divisor, rect.m_y / divisor,
                    LSize<T>(rect.m_width / divisor, rect.m_height / divisor));
}

inline LRect<int> operator/(const LRect<int> &rect, double divisor)
{
    return LRect<int>(std::lround(rect.m_x / divisor), std::lround(rect.m_y / divisor),
                      LSize<int>(std::lround(rect.m_width / divisor), std::lround(rect.m_height / divisor)));
}

template<typename T>
inline LRect<T> &LRect<T>::operator*=(int factor) noexcept
{
    return *this = *this * factor;
}

template<typename T>
inline LRect<T> &LRect<T>::operator/=(int divisor)
{
    return *this = *this / divisor;
}

template<typename T>
inline LRect<T> &LRect<T>::operator*=(double factor) noexcept
{
    return *this = *this * factor;
}

template<typename T>
inline LRect<T> &LRect<T>::operator/=(double divisor)
{
    return *this = *this / divisor;
}

template<typename T>
inline LRect<T> operator+(const LRect<T> &rectangle, const LMargins<T> &margins) noexcept
{
    return rectangle.marginsAdded(margins);
}

template<typename T>
inline LRect<T> operator+(const LMargins<T> &margins, const LRect<T> &rectangle) noexcept
{
    return rectangle.marginsAdded(margins);
}

template<typename T>
inline LRect<T> operator-(const LRect<T> &rectangle, const LMargins<T> &margins) noexcept
{
    return rectangle.marginsRemoved(margins);
}

LK_NAMESPACE_END
#endif
