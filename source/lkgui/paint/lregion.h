#ifndef LREGION_H
#define LREGION_H
#include "lkgui/paint/lpainterpath.h"
#include <vector>

LK_NAMESPACE_BEGIN
class LKGUIDEF LRegion : public std::enable_shared_from_this<LRegion>
{
public:
    enum RegionType { Rectangle, Ellipse };
    virtual ~LRegion();

    LRegion();
    LRegion(int x, int y, int w, int h, RegionType t = Rectangle);
    LRegion(const LRect<int> &r, RegionType t = Rectangle);
    LRegion(const LPolygon<int> &pa, LFillRule fillRule = LFillRule::OddEven);
    LRegion(const LRegion& r);
    LRegion(std::shared_ptr<LRegion> r);
    LRegion &operator=(const LRegion &r);
    LRegion &operator=(const LRect<int> &r);

    bool operator==(const LRegion &r) const;

    inline std::shared_ptr<LRegion> getPtr() const{
        return const_cast<LRegion*>(this)->shared_from_this();
    }

    void initPolygonRegion(const std::vector<LPoint<int>>& points, int rule);

    inline bool isEmpty() const{
        return m_numRects == 0 || m_extents.isEmpty();
    }

    void clear();

    inline int rectCount() const{
        return m_numRects;
    }

    typedef const LRect<int> *const_iterator;
    typedef _STD reverse_iterator<const_iterator> const_reverse_iterator;

    const_iterator begin()  const noexcept;
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator end()    const noexcept;
    const_iterator cend()   const noexcept { return end(); }
    const_reverse_iterator rbegin()  const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    const_reverse_iterator rend()    const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend()   const noexcept { return rend(); }

    LRegion& unite(const std::shared_ptr<LRegion> r);
    LRegion& unite(const LRect<int> &r);

    inline std::shared_ptr<LRegion> united(const std::shared_ptr<LRegion> r) const{
        std::shared_ptr<LRegion> ret = std::make_shared<LRegion>(*this);
        ret->unite(r);
        return ret;
    }

    inline std::shared_ptr<LRegion> united(const LRect<int> &r) const{
        std::shared_ptr<LRegion> ret = std::make_shared<LRegion>(*this);
        ret->unite(r);
        return ret;
    }

    LRegion& intersect(const std::shared_ptr<LRegion> r);
    LRegion& intersect(const LRect<int> &r);

    inline std::shared_ptr<LRegion> intersected(const std::shared_ptr<LRegion> r) const{
        std::shared_ptr<LRegion> ret = std::make_shared<LRegion>(*this);
        ret->intersect(r);
        return ret;
    }

    inline std::shared_ptr<LRegion> intersected(const LRect<int> &r) const{
        std::shared_ptr<LRegion> ret = std::make_shared<LRegion>(*this);
        ret->intersect(r);
        return ret;
    }

    LRegion& subtract(const std::shared_ptr<LRegion> r);
    inline LRegion& subtract(const LRect<int> &r){
        std::shared_ptr<LRegion> t = std::make_shared<LRegion>(r);
        return subtract(t);
    }

    inline std::shared_ptr<LRegion> subtracted(const std::shared_ptr<LRegion> r) const{
        std::shared_ptr<LRegion> ret = std::make_shared<LRegion>(*this);
        ret->subtract(r);
        return ret;
    }

    inline std::shared_ptr<LRegion> subtracted(const LRect<int> &r) const{
        std::shared_ptr<LRegion> t = std::make_shared<LRegion>(r);
        return subtracted(t);
    }

    LRegion& exor(const std::shared_ptr<LRegion> r);

    std::shared_ptr<LRegion> exored(const std::shared_ptr<LRegion> r) const{
        std::shared_ptr<LRegion> ret = std::make_shared<LRegion>(*this);
        ret->exor(r);
        return ret;
    }

    bool contains(const LPoint<int> &p) const;
    bool contains(const LRect<int> &r) const;

    bool isIntersects(const std::shared_ptr<LRegion> r) const;
    bool isIntersects(const LRect<int> r) const;

    static bool equalRegion(const LRegion *r1, const LRegion *r2);
    static bool regionStrictContains(const LRegion *region, const LRect<int>& rect);
    static std::shared_ptr<LPainterPath> regionToPath(const LRegion* region);

    //static
    void translate(int dx, int dy);
    inline void translate(const LPoint<int> &p) { translate(p.m_x, p.m_y); }
    std::shared_ptr<LRegion> translated(int dx, int dy) const{
        std::shared_ptr<LRegion> ret = std::make_shared<LRegion>(*this);
        ret->translate(dx, dy);
        return ret;
    }
    inline std::shared_ptr<LRegion> translated(const LPoint<int> &p) const { return translated(p.m_x, p.m_y); }

    LRect<int> boundingRect() const noexcept;
    void setRects(const LRect<int> *rect, int num);

protected:
    struct LRegionSpan {
        LRegionSpan() {}
        LRegionSpan(int x1_, int x2_) : x1(x1_), x2(x2_) {}

        int x1;
        int x2;
        int width() const { return x2 - x1; }
    };


    int m_numRects = 0;
    int m_innerArea = -1;
    std::vector<LRect<int> > m_rects;
    LRect<int> m_extents;
    LRect<int> m_innerRect;

    void vectorize();

    void initPolygonRegionEx(const std::vector<LPoint<int>>& points, int rule);
    inline void updateInnerRect(const LRect<int> &rect) {
        const int area = rect.width() * rect.height();
        if (area > m_innerArea) {
            m_innerArea = area;
            m_innerRect = rect;
        }
    }

    void flushRow(const std::vector<LRegionSpan>& spans, int y, int numSpans, int *lastRow, int *extendTo, bool *needsExtend);

    bool containsInner(const LRect<int> &r) const;

    inline bool containsInner(const LRegion* r) const {
        return containsInner(r->m_extents);
    }
    bool within(const LRect<int> &r1) const;

    bool rectInRegion(int rx, int ry, uint32_t rwidth, uint32_t rheight) const;
    bool pointInRegion(int x, int y) const;

    bool canAppend(const LRect<int>* r) const;
    inline bool canAppend(const LRegion* r) const
    {
        return canAppend(r->m_numRects == 1 ? &r->m_extents : r->m_rects.data());
    }
    bool canPrepend(const LRect<int>* r) const;
    bool canPrepend(const LRegion* r) const
    {
        return canPrepend(r->m_numRects == 1 ? &r->m_extents : &(r->m_rects[r->m_numRects - 1]));
    }

    void append(const LRect<int> *r);
    void append(const LRegion *r);
    void prepend(const LRect<int> *r);
    void prepend(const LRegion *r);

    bool mergeFromRight(LRect<int> *left, const LRect<int> *right);
    bool mergeFromLeft(LRect<int> *right, const LRect<int> *left);
    bool mergeFromBelow(LRect<int> *top, const LRect<int> *bottom,
                        const LRect<int> *nextToTop,
                        const LRect<int> *nextToBottom);

    bool mergeFromAbove(LRect<int> *bottom, const LRect<int> *top,
                        const LRect<int> *nextToBottom,
                        const LRect<int> *nextToTop);

    void intersect0(const LRect<int> &rect);

    typedef void (*OverlapFunc)(LRegion* dest, const LRect<int> *r1, const LRect<int> *r1End,
                                const LRect<int> *r2, const LRect<int> *r2End, int y1, int y2);
    typedef void (*NonOverlapFunc)(LRegion* dest, const LRect<int> *r, const LRect<int> *rEnd, int y1, int y2);


    static void miUnionNonO(LRegion* dest, const LRect<int> *r, const LRect<int> *rEnd, int y1, int y2);
    static void miUnionO(LRegion* dest, const LRect<int> *r1, const LRect<int> *r1End,
                         const LRect<int> *r2, const LRect<int> *r2End, int y1, int y2);
    static int miCoalesce(LRegion* dest, int prevStart, int curStart);

    static void miRegionOp(LRegion* dest,
                           const LRegion* reg1, const LRegion* reg2,
                           OverlapFunc overlapFunc, NonOverlapFunc nonOverlap1Func,
                           NonOverlapFunc nonOverlap2Func);

    static void miSetExtents(LRegion* dest);
    static void miIntersectO(LRegion* dest, const LRect<int> *r1, const LRect<int> *r1End,
                             const LRect<int> *r2, const LRect<int> *r2End, int y1, int y2);

    static void unionRegion(const LRegion* reg1, const LRegion* reg2, LRegion* dest);

    static void miSubtractNonO1(LRegion* dest, const LRect<int> *r,
                                const LRect<int> *rEnd, int y1, int y2);
    static void miSubtractO(LRegion* dest, const LRect<int> *r1, const LRect<int> *r1End,
                            const LRect<int> *r2, const LRect<int> *r2End, int y1, int y2);

    static void subtractRegion(const LRegion *regM, const LRegion *regS, LRegion*  dest);

    static void exorRegion(const LRegion *sra, const LRegion *srb, LRegion* dest);

    static void offsetRegion(LRegion* region, int x, int y);
};

LK_NAMESPACE_END

#endif // LREGION_H
