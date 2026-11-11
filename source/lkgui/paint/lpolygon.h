#ifndef LPOLYGON_H
#define LPOLYGON_H

#include "lkgui/lrect.h"
#include <vector>

LK_NAMESPACE_BEGIN

template<typename T>
class LPolygon : public std::vector<LPoint<T> >
{
public:
    using std::vector<LPoint<T> >::at;
    using std::vector<LPoint<T> >::reserve;
    using std::vector<LPoint<T> >::data;
    using std::vector<LPoint<T> >::size;
    using std::vector<LPoint<T> >::resize;
    using std::vector<LPoint<T> >::empty;
    using std::vector<LPoint<T> >::front;
    using std::vector<LPoint<T> >::back;

    inline LPolygon() {}
    inline explicit LPolygon(int size):std::vector<LPoint<T> >(size){}
    inline LPolygon(const  std::vector<LPoint<T> > &v) :  std::vector<LPoint<T> >(v) {}
    LPolygon(std::vector<LPoint<T> > &&v) noexcept : std::vector<LPoint<T> >(std::move(v)) {}
    LPolygon(const LRect<T> &r, bool closed=false){
        reserve(closed ? 5 : 4);
        std::vector<LPoint<T> >::push_back(LPoint<T>(r.m_x, r.m_y));
        std::vector<LPoint<T> >::push_back(LPoint<T>(r.x2(), r.m_y));
        std::vector<LPoint<T> >::push_back(LPoint<T>(r.x2(), r.y2()));
        std::vector<LPoint<T> >::push_back(LPoint<T>(r.m_x, r.y2()));

        if(closed){
            std::vector<LPoint<T> >::push_back(LPoint<T>(r.m_x, r.m_y));
        }
    }

    LPolygon(const LPolygon<T> &other) : std::vector<LPoint<T> >(other) {}
    LPolygon(LPolygon<T> &&other) noexcept : std::vector<LPoint<T> >(std::move(other)) {}
    LPolygon &operator=(LPolygon<T> &&other) noexcept { swap(other); return *this; }
    LPolygon &operator=(const LPolygon<T> &other) { std::vector<LPoint<T> >::operator=(other); return *this; }
    void swap(LPolygon<T> &other) noexcept { std::vector<LPoint<T> >::swap(other); }

    void translate(T dx, T dy){
        if (lFuzzyIsZero(dx) == 0 && lFuzzyIsZero(dy) == 0){
            return;
        }

        LPoint<T> *p = data();
        int i = size();
        while (i--) {
            p->m_x += dx;
            p->m_y += dy;
            ++p;
        }
    }

    void translate(const LPoint<T> &offset){
        translate(offset.x(), offset.y());
    }

    LPolygon<T> translated(int dx, int dy) const{
        LPolygon<T> copy(*this);
        copy.translate(dx, dy);
        return copy;
    }
    inline LPolygon<T> translated(const LPoint<T> &offset) const{
        return translated(offset.x(), offset.y());
    }

    LRect<T> boundingRect() const{
        const LPoint<T> *pd = data();
        const LPoint<T> *pe = pd + size();
        if (pd == pe){
            return LRect<T>(0, 0, 0, 0, false);
        }
        T minx, maxx, miny, maxy;
        minx = maxx = pd->m_x;
        miny = maxy = pd->m_y;
        ++pd;
        while (pd != pe) {
            if (pd->m_x < minx){
                minx = pd->m_x;
            }
            else if (pd->m_x > maxx){
                maxx = pd->m_x;
            }
            if (pd->m_y < miny){
                miny = pd->m_y;
            }
            else if (pd->m_y > maxy){
                maxy = pd->m_y;
            }
            ++pd;
        }
        return LRect<T>(minx, miny, maxx, maxy, true);
    }

    void point(int index, T *x, T *y) const{
        LPoint<T> p = at(index);
        if (x){
            *x = p.m_x;
        }
        if (y){
            *y = p.m_y;
        }
    }
    inline LPoint<T> point(int index) const{
        return at(index);
    }
    void setPoint(int index, T x, T y){
        (*this)[index] = LPoint<T>(x, y);;
    }
    void setPoint(int index, const LPoint<T> &p){
        (*this)[index] = p;
    }

    void putPoints(int index, int nPoints, const LPolygon<T> & from, int fromIndex=0){
        if (index + nPoints > size()){
            resize(index + nPoints);
        }
        if (nPoints <= 0){
            return;
        }
        int n = 0;
        while(n < nPoints) {
            setPoint(index + n, from[fromIndex+n]);
            ++n;
        }
    }

    static void polygonIsectLine(const LPoint<T> &p1,
                                 const LPoint<T> &p2,
                                 const LPoint<T> &pos,
                                 int *winding)
    {
        T x1 = p1.m_x;
        T y1 = p1.m_y;
        T x2 = p2.m_x;
        T y2 = p2.m_y;
        T y = pos.m_y;

        int dir = 1;

        if (lFuzzyCompare(y1, y2)) {
            // ignore horizontal lines according to scan conversion rule
            return;
        } else if (y2 < y1) {
            T x_tmp = x2; x2 = x1; x1 = x_tmp;
            T y_tmp = y2; y2 = y1; y1 = y_tmp;
            dir = -1;
        }

        if (y >= y1 && y < y2) {
            if ((x2 - x1) * (y - y1)<=(pos.m_x - x1)* (y2 - y1)) {
                (*winding) += dir;
            }
        }
    }


    bool containsPoint(const LPoint<T> &pt, LFillRule fillRule) const{
        if (empty()){
            return false;
        }

        int winding_number = 0;

        LPoint<T> last_pt = at(0);
        LPoint<T> last_start = at(0);
        for (int i = 1; i < size(); ++i) {
            const LPoint<T> &e = at(i);
            polygonIsectLine(last_pt, e, pt, &winding_number);
            last_pt = e;
        }

        // implicitly close last subpath
        if (last_pt != last_start){
            polygonIsectLine(last_pt, last_start, pt, &winding_number);
        }

        return (fillRule == LFillRule::Winding
                    ? (winding_number != 0)
                    : ((winding_number % 2) != 0));

    }

    template<typename C=int,
             typename std::enable_if<std::is_integral<C>::value
             && std::is_floating_point<T>::value, bool>::type = true>
    LPolygon<C> toPolygon() const
    {
        LPolygon<C> a;
        a.reserve(size());
        for (int i=0; i<size(); ++i){
            a.push_back(at(i).toPoint<C>());
        }
        return a;
    }


    template<typename C=double,
             typename std::enable_if<std::is_floating_point<C>::value
             && std::is_integral<T>::value, bool>::type = true>
    LPolygon<C> toPolygonF() const
    {
        LPolygon<C> a;
        a.reserve(size());
        for (int i=0; i<size(); ++i){
            a.push_back(at(i).toPointF<C>());
        }
        return a;
    }

    bool isClosed() const { return !empty() && front() == back(); }
};

LK_NAMESPACE_END

#endif // LPOLYGON_H
