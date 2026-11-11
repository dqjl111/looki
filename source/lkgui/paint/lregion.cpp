#include "lregion.h"
#include "lpainterpath.h"
#include "lkcore/ldebug.h"
#include <array>

LK_NAMESPACE_BEGIN

struct LSegment
{
    LSegment() {}
    LSegment(const LPoint<int> &p)
        : added(false)
        , point(p)
    {
    }

    int left() const
    {
        return std::min(point.m_x, next->point.m_x);
    }

    int right() const
    {
        return std::max(point.m_x, next->point.m_x);
    }

    bool overlaps(const LSegment &other) const
    {
        return left() < other.right() && other.left() < right();
    }

    void connect(LSegment &other)
    {
        next = &other;
        other.prev = this;

        horizontal = (point.m_y == other.point.m_y);
    }

    void merge(LSegment &other)
    {
        if (right() <= other.right()) {
            LPoint<int> p = other.point;
            LSegment *oprev = other.prev;

            other.point = point;
            other.prev = prev;
            prev->next = &other;

            point = p;
            prev = oprev;
            oprev->next = this;
        } else {
            LSegment *onext = other.next;
            other.next = next;
            next->prev = &other;

            next = onext;
            next->prev = this;
        }
    }

    int horizontal : 1;
    int added : 1;

    LPoint<int> point;
    LSegment *prev;
    LSegment *next;
};

void mergeLSegments(LSegment *a, int na, LSegment *b, int nb)
{
    int i = 0;
    int j = 0;

    while (i != na && j != nb) {
        LSegment &sa = a[i];
        LSegment &sb = b[j];
        const int ra = sa.right();
        const int rb = sb.right();
        if (sa.overlaps(sb))
            sa.merge(sb);
        i += (rb >= ra);
        j += (ra >= rb);
    }
}

void addLSegmentsToPath(LSegment *segment, std::shared_ptr<LPainterPath> path)
{
    LSegment *current = segment;
    path->moveTo(current->point.toPointF());

    current->added = true;

    LSegment *last = current;
    current = current->next;
    while (current != segment) {
        if (current->horizontal != last->horizontal){
            path->lineTo(current->point.toPointF());
        }
        current->added = true;
        last = current;
        current = current->next;
    }
}

/*
 *     This structure contains all of the information needed
 *     to run the bresenham algorithm.
 *     The variables may be hardcoded into the declarations
 *     instead of using this structure to make use of
 *     register declarations.
 */
struct LBresInfo{
    int minorAxis;     /* minor axis        */
    int d;              /* decision variable */
    int m, m1;          /* slope and slope+1 */
    int incr1, incr2;   /* error increments */
};


struct LEdgeTableEntry {
    int ymax;             /* ycoord at which we exit this edge. */
    int clockWise;        /* flag for winding number rule       */
    LBresInfo bres;        /* Bresenham info to run the edge     */
    struct LEdgeTableEntry *next = nullptr;       /* next in the list     */
    struct LEdgeTableEntry *back = nullptr;       /* for insertion sort   */
    struct LEdgeTableEntry *nextWETE = nullptr;   /* for winding num rule */
};


struct LScanLineList{
    int scanline;              /* the scanline represented */
    LEdgeTableEntry *edgelist = nullptr;  /* header node              */
    struct LScanLineList *next = nullptr;  /* next in the list       */
};

struct LEdgeTable{
    int ymax;                 /* ymax for the polygon     */
    int ymin;                 /* ymin for the polygon     */
    LScanLineList scanlines;   /* header node              */
};

/*
 * Here is a struct to help with storage allocation
 * so we can allocate a big chunk at a time, and then take
 * pieces from this heap when we need to.
 */
#define SLLSPERBLOCK 25

struct LScanLineListBlock {
    LScanLineList slls[SLLSPERBLOCK];
    std::shared_ptr<LScanLineListBlock> next = nullptr;
};

/*
 * number of points to buffer before sending them off
 * to scanlines(): Must be an even number
 */
#define NUMPTSTOBUFFER 200
/*
 * used to allocate buffers for points and link
 * the buffers together
 */
struct LPointBlock {
    std::array<LPoint<int>, NUMPTSTOBUFFER> points;
    std::shared_ptr<LPointBlock> next = nullptr;
};

void bresInitPgonStruct(int dy, int x1, int x2, LBresInfo& bres){
    /*
     *  if the edge is horizontal, then it is ignored
     *  and assumed not to be processed.  Otherwise, do this stuff.
     */
    if (dy != 0) {
        bres.minorAxis = (x1);
        int dx = x2 - x1;
        if (dx < 0) {
            bres.m = dx / dy;
            bres.m1 = bres.m - 1;
            bres.incr1 = -2 * dx + 2 * dy * bres.m1;
            bres.incr2 = -2 * dx + 2 * dy * bres.m;
            bres.d = 2 * bres.m * dy - 2 * dx - 2 * dy;
        } else {
            bres.m = dx / dy;
            bres.m1 = bres.m + 1;
            bres.incr1 = 2 * dx - 2 * dy * bres.m1;
            bres.incr2 = 2 * dx - 2 * dy * bres.m;
            bres.d = -2 * bres.m * dy + 2 * dx;
        }
    }
}

void bresIncPgonStruct(LBresInfo& bres){
    if (bres.m1 > 0) {
        if (bres.d > 0) {
            bres.minorAxis += bres.m1;
            bres.d += bres.incr1;
        }
        else {
            bres.minorAxis += bres.m;
            bres.d += bres.incr2;
        }
    } else {
        if (bres.d >= 0) {
            bres.minorAxis += bres.m1;
            bres.d += bres.incr1;
        }
        else {
            bres.minorAxis += bres.m;
            bres.d += bres.incr2;
        }
    }
}

/*
 *     InsertEdgeInET
 *
 *     Insert the given edge into the edge table.
 *     First we must find the correct bucket in the
 *     Edge table, then find the right slot in the
 *     bucket.  Finally, we can insert it.
 *
 */
static std::shared_ptr<LScanLineListBlock> insertEdgeInET(LEdgeTable *et,
                                                          LEdgeTableEntry *ete,
                                                          int scanline,
                                                          std::shared_ptr<LScanLineListBlock> sllBlock,
                                                          int& iSllBlock)
{
    LEdgeTableEntry *start, *prev;
    LScanLineList *pSLL, *pPrevSLL;
    std::shared_ptr<LScanLineListBlock> tmpSLLBlock = nullptr;

    /*
     * find the right bucket to put the edge into
     */
    pPrevSLL = &et->scanlines;
    pSLL = pPrevSLL->next;
    while ((pSLL!=nullptr) && (pSLL->scanline < scanline)) {
        pPrevSLL = pSLL;
        pSLL = pSLL->next;
    }

    /*
     * reassign pSLL (pointer to ScanLineList) if necessary
     */
    if ((pSLL== nullptr) || (pSLL->scanline > scanline)) {
        if (iSllBlock > SLLSPERBLOCK-1)
        {
            tmpSLLBlock = std::make_shared<LScanLineListBlock>();  ;
            if(tmpSLLBlock != nullptr){
                sllBlock->next = tmpSLLBlock;
                tmpSLLBlock->next = nullptr;
                sllBlock= tmpSLLBlock;
                iSllBlock = 0;
            }else{
                return sllBlock;
            }
        }
        pSLL = &(sllBlock->slls[iSllBlock++]);

        pSLL->next = pPrevSLL->next;
        pSLL->edgelist = nullptr;
        pPrevSLL->next = pSLL;
    }
    pSLL->scanline = scanline;

    /*
     * now insert the edge in the right bucket
     */
    prev = nullptr;
    start = pSLL->edgelist;
    while ((start != nullptr) && (start->bres.minorAxis < ete->bres.minorAxis)) {
        prev = start;
        start = start->next;
    }
    ete->next = start;

    if (prev){
        prev->next = ete;
    }
    else{
        pSLL->edgelist = ete;
    }
    return sllBlock;
}


/*
 *     CreateEdgeTable
 *
 *     This routine creates the edge table for
 *     scan converting polygons.
 *     The Edge Table (ET) looks like:
 *
 *    EdgeTable
 *     --------
 *    |  ymax  |        ScanLineLists
 *    |scanline|-->------------>-------------->...
 *     --------   |scanline|   |scanline|
 *                |edgelist|   |edgelist|
 *                ---------    ---------
 *                    |             |
 *                    |             |
 *                    V             V
 *              list of ETEs   list of ETEs
 *
 *     where ETE is an EdgeTableEntry data structure,
 *     and there is one ScanLineList per scanline at
 *     which an edge is initially entered.
 *
 */

static void createETandAET(const std::vector<LPoint<int> >& pts,
                           LEdgeTable *et,
                           LEdgeTableEntry *aet,
                           std::vector<LEdgeTableEntry>& etes,
                           std::shared_ptr<LScanLineListBlock>  sllBlock)
{
    if (pts.size() < 2){
        return;
    }

    const LPoint<int> *top, *bottom, *prevPt, *currPt;
    int iSllBlock = 0;
    int dy;
    LEdgeTableEntry* pEte;
    /*
     *  initialize the Active Edge Table
     */
    aet->next = nullptr;
    aet->back = nullptr;
    aet->nextWETE = nullptr;
    aet->bres.minorAxis = std::numeric_limits<int>::min();

    /*
     *  initialize the Edge Table.
     */
    et->scanlines.next = nullptr;
    et->ymax = std::numeric_limits<int>::min();
    et->ymin = std::numeric_limits<int>::max();
    sllBlock->next = nullptr;

    int count = pts.size();
    prevPt = &pts[count - 1];

    int iPt = 0;
    int iEte = 0;
    /*
     *  for each vertex in the array of points.
     *  In this loop we are dealing with two vertices at
     *  a time -- these make up one edge of the polygon.
     */
    while (count--) {
        currPt = &pts[iPt];
        pEte = &etes[iEte];
        /*
         *  find out which point is above and which is below.
         */
        if (prevPt->m_y > currPt->m_y) {
            bottom = prevPt;
            top = currPt;
            pEte->clockWise = 0;
        } else {
            bottom = currPt;
            top = prevPt;
            pEte->clockWise = 1;
        }

        /*
         * don't add horizontal edges to the Edge table.
         */
        if (bottom->m_y != top->m_y) {
            pEte->ymax = bottom->m_y - 1;  /* -1 so we don't get last scanline */

            /*
             *  initialize integer edge algorithm
             */
            dy = bottom->m_y - top->m_y;
            bresInitPgonStruct(dy, top->m_x, bottom->m_x, pEte->bres);
            sllBlock = insertEdgeInET(et, pEte, top->m_y, sllBlock, iSllBlock);

            if (prevPt->m_y > et->ymax){
                et->ymax = prevPt->m_y;
            }
            if (prevPt->m_y < et->ymin){
                et->ymin = prevPt->m_y;
            }
            ++iEte;
        }

        prevPt = currPt;
        iPt++;
    }
}

/*
 *     loadAet
 *
 *     This routine moves EdgeTableEntries from the
 *     LEdgeTable into the Active Edge Table,
 *     leaving them sorted by smaller x coordinate.
 *
 */

static void loadAet(LEdgeTableEntry *aet, LEdgeTableEntry *etes)
{
    LEdgeTableEntry *pPrevAET;
    LEdgeTableEntry *tmp;

    pPrevAET = aet;
    aet = aet->next;
    while (etes!= nullptr) {
        while ((aet != nullptr) && aet->bres.minorAxis < etes->bres.minorAxis) {
            pPrevAET = aet;
            aet = aet->next;
        }
        tmp = etes->next;
        etes->next = aet;
        if (aet){
            aet->back = etes;
        }
        etes->back = pPrevAET;
        pPrevAET->next = etes;
        pPrevAET = etes;

        etes = tmp;
    }
}

/*
 *     computeWAET
 *
 *     This routine links the AET by the
 *     nextWETE (winding EdgeTableEntry) link for
 *     use by the winding number rule.  The final
 *     Active Edge Table (AET) might look something
 *     like:
 *
 *     AET
 *     ----------  ---------   ---------
 *     |ymax    |  |ymax    |  |ymax    |
 *     | ...    |  |...     |  |...     |
 *     |next    |->|next    |->|next    |->...
 *     |nextWETE|  |nextWETE|  |nextWETE|
 *     ---------   ---------   ^--------
 *         |                   |       |
 *         V------------------->       V---> ...
 *
 */
static void computeWAet(LEdgeTableEntry *aet)
{
    LEdgeTableEntry *pWETE;
    int inside = 1;
    int isInside = 0;

    aet->nextWETE = nullptr;
    pWETE = aet;
    aet = aet->next;
    while (aet) {
        if (aet->clockWise){
            ++isInside;
        }
        else{
            --isInside;
        }

        if ((!inside && !isInside) || (inside && isInside)) {
            pWETE->nextWETE = aet;
            pWETE = aet;
            inside = !inside;
        }
        aet = aet->next;
    }
    pWETE->nextWETE = nullptr;
}

/*
 *     InsertionSort
 *
 *     Just a simple insertion sort using
 *     pointers and back pointers to sort the Active
 *     Edge Table.
 *
 */

static int insertionSort(LEdgeTableEntry *aet)
{
    LEdgeTableEntry *pETEchase;
    LEdgeTableEntry *pETEinsert;
    LEdgeTableEntry *pETEchaseBackTMP;
    int changed = 0;

    aet = aet->next;
    while (aet != nullptr) {
        pETEinsert = aet;
        pETEchase = aet;
        while (pETEchase->back->bres.minorAxis > aet->bres.minorAxis){
            pETEchase = pETEchase->back;
        }

        aet = aet->next;
        if (pETEchase != pETEinsert) {
            pETEchaseBackTMP = pETEchase->back;
            pETEinsert->back->next = aet;
            if (aet != nullptr){
                aet->back = pETEinsert->back;
            }
            pETEinsert->next = pETEchase;
            pETEchase->back->next = pETEinsert;
            pETEchase->back = pETEinsert;
            pETEinsert->back = pETEchaseBackTMP;
            changed = 1;
        }
    }
    return changed;
}

/*  1 if two BOXes overlap.
 *  0 if two BOXes do not overlap.
 *  Remember, x2 and y2 are not in the region
 */
static bool extendCheck(const LRect<int>& r1, const LRect<int>& r2){
    return (r1.x2() > r2.m_x &&
            r1.m_x < r2.x2() &&
            r1.y2() > r2.m_y &&
            r1.m_y < r2.y2());
}

static inline bool canMergeFromRight(const LRect<int> *left, const LRect<int> *right)
{
    return (right->m_y == left->m_y
            && right->y2() == left->y2()
            && right->m_x <= (left->x2()));
}

static inline bool canMergeFromLeft(const LRect<int> *right, const LRect<int> *left)
{
    return canMergeFromRight(left, right);
}

static inline bool canMergeFromBelow(const LRect<int> *top, const LRect<int> *bottom,
                                     const LRect<int> *nextToTop,
                                     const LRect<int> *nextToBottom)
{
    if (nextToTop && nextToTop->m_y == top->m_y){
        return false;
    }
    if (nextToBottom && nextToBottom->m_y == bottom->m_y){
        return false;
    }

    return ((top->y2() >= (bottom->m_y))
            && top->m_x == bottom->m_x
            && top->x2() == bottom->x2());
}

static inline LRect<int> rectIntersectNormalized(const LRect<int> &r1, const LRect<int> &r2)
{
    LRect<int> r;
    r.m_x = std::max(r1.m_x, r2.m_x);
    r.setX2(std::min(r1.x2(), r2.x2()));
    r.m_y = std::max(r1.m_y, r2.m_y);
    r.setY2(std::min(r1.y2(), r2.y2()));
    return r;
}


/////////////////////////////////////////////////////////////////////////////
LRegion::~LRegion()
{
}

LRegion::LRegion(){

}

LRegion::LRegion(const LRegion &r):
    m_numRects(r.m_numRects),
    m_innerArea(r.m_innerArea),
    m_rects(r.m_rects),
    m_extents(r.m_extents),
    m_innerRect(r.m_innerRect)

{
}

LRegion::LRegion(std::shared_ptr<LRegion> r)
{
    if(r != nullptr){
        *this = *r;
    }
}

LRegion &LRegion::operator=(const LRegion &r)
{
    m_numRects = r.m_numRects;
    m_innerArea = r.m_innerArea;
    m_rects = r.m_rects;
    m_extents = r.m_extents;
    m_innerRect = r.m_innerRect;
    return *this;
}


LRegion &LRegion::operator=(const LRect<int> &r)
{
    m_numRects = 1;
    m_innerArea = r.width() * r.height();
    m_rects.clear();
    m_extents = r;
    m_innerRect = r;
    return *this;
}


bool LRegion::operator==(const LRegion &r) const
{
    if(this == &r){
        return true;
    }
    return equalRegion(this, &r);
}

LRegion::LRegion(int x, int y, int w, int h, RegionType t):
    m_numRects(1),
    m_innerArea(w * h)
{
    m_extents.setRect(x, y, w, h);
    m_innerRect = m_extents;
    if (t == Rectangle) {
        //m_data = std::shared_ptr<LRegionData>(r);
    } else if (t == Ellipse) {
        LPainterPath path;
        path.addEllipse(x, y, w, h);
        LPolygon<int> a = path.toSubpathPolygons(LTransform2D()).front().toPolygon();
        initPolygonRegion(a, (int)LFillRule::OddEven);
    }
}

LRegion::LRegion(const LRect<int> &r, RegionType t):
    m_numRects(1),
    m_innerArea(r.width() * r.height()),
    m_extents(r),
    m_innerRect(r)
{
    if (!r.isEmpty()) {
        if (t == Rectangle) {
        } else if (t == Ellipse) {
            LPainterPath path;
            path.addEllipse(r.m_x, r.m_y, r.width(), r.height());
            LPolygon<int> a = path.toSubpathPolygons(LTransform2D()).front().toPolygon();
            initPolygonRegion(a, (int)LFillRule::OddEven);
        }
    }
}


LRegion::LRegion(const LPolygon<int> &a, LFillRule fillRule)
{
    if (a.size() > 2) {
        initPolygonRegion(a, (int)fillRule);
    }
}

void LRegion::initPolygonRegion(const std::vector<LPoint<int> >& points, int rule)
{
    /* special case a rectangle */
    int Count = points.size();
    if (((Count == 4) ||
         ((Count == 5) && (points[4].m_x == points[0].m_x) && (points[4].m_y == points[0].m_y)))
        && (((points[0].m_y == points[1].m_y) && (points[1].m_x == points[2].m_x) && (points[2].m_y == points[3].m_y)
             && (points[3].m_x == points[0].m_x)) || ((points[0].m_x == points[1].m_x)
                && (points[1].m_y == points[2].m_y) && (points[2].m_x == points[3].m_x)
                && (points[3].m_y == points[0].m_y)))) {
        int x = std::min(points[0].m_x, points[2].m_x);
        m_extents.m_x = x;
        int y = std::min(points[0].m_y, points[2].m_y);
        m_extents.m_y = y;
        m_extents.setWidth(std::max(points[0].m_x, points[2].m_x) - x);
        m_extents.setHeight(std::max(points[0].m_y, points[2].m_y) - y);
        if ((m_extents.m_x <= m_extents.x2()) &&
            (m_extents.m_y <= m_extents.y2())) {
            m_numRects = 1;
            m_innerRect = m_extents;
            m_innerArea = m_innerRect.width() * m_innerRect.height();
        }
        return;
    }

    initPolygonRegionEx(points, rule);
}

void LRegion::clear()
{
    m_numRects = 0;
    m_innerArea = 0;
    m_rects.clear();
    m_extents.reset();
    m_innerRect.reset();
}


void LRegion::initPolygonRegionEx(const std::vector<LPoint<int> > &points, int rule)
{
    vectorize();

    LEdgeTableEntry *pAet;   /* Active Edge Table       */
    int y;                  /* current scanline        */
    int iPts = 0;           /* number of pts in buffer */
    LEdgeTableEntry *pWEte;  /* Winding Edge Table Entry*/
    LScanLineList *pSLL;     /* current scanLineList    */
    //QPoint *pts;             /* output buffer           */
    LEdgeTableEntry *pPrevAet;        /* ptr to previous AET     */
    LEdgeTable et;                    /* header node for ET      */
    LEdgeTableEntry aet;             /* header node for AET     */
    //*pETEs;           /* EdgeTableEntries pool   */
    std::shared_ptr<LScanLineListBlock> headBlock = std::make_shared<LScanLineListBlock>();      /* header for scanlinelist */

    int fixWAET = false;
    int numFullPtBlocks = 0;

    std::vector<LEdgeTableEntry> etes(points.size());
    createETandAET(points, &et, &aet, etes, headBlock);

    // sanity check that the region won't become too big...
    if (et.ymax - et.ymin > 100000) {
        lWarn() << "LRegion: creating region from big polygon failed...!";
        return;
    }

    std::shared_ptr<LPointBlock> firstPtBlock = std::make_shared<LPointBlock>();
    std::shared_ptr<LPointBlock> curPtBlock = firstPtBlock;
    pSLL = et.scanlines.next;
    LPoint<int> *pts = &(curPtBlock->points[0]);

    if (rule == (int)LFillRule::OddEven) {
        /*
             *  for each scanline
             */
        for (y = et.ymin; y < et.ymax; ++y) {

            /*
                 *  Add a new edge to the active edge table when we
                 *  get to the next edge.
                 */
            if (pSLL && y == pSLL->scanline) {
                loadAet(&aet, pSLL->edgelist);
                pSLL = pSLL->next;
            }
            pPrevAet = &aet;
            pAet = aet.next;

            /*
                 *  for each active edge
                 */
            while (pAet != nullptr) {
                pts->m_x = pAet->bres.minorAxis;
                pts->m_y = y;
                ++pts;
                ++iPts;

                /*
                     *  send out the buffer
                     */
                if (iPts == NUMPTSTOBUFFER) {
                    std::shared_ptr<LPointBlock> tmpPtBlock = std::make_shared<LPointBlock>();
                    if(tmpPtBlock != nullptr){
                        curPtBlock->next = tmpPtBlock;
                        curPtBlock = tmpPtBlock;
                        pts = &(curPtBlock->points[0]);
                        ++numFullPtBlocks;
                        iPts = 0;
                    }else{
                        return;
                    }
                }

                /*
                 *     Evaluate the given edge at the given scanline.
                 *     If the edge has expired, then we leave it and fix up
                 *     the active edge table; otherwise, we increment the
                 *     x value to be ready for the next scanline.
                 *     The even-odd rule is in effect.
                 */
                if (pAet->ymax == y) {          /* leaving this edge */
                    pPrevAet->next = pAet->next;
                    pAet = pPrevAet->next;
                    if (pAet != nullptr){
                        pAet->back = pPrevAet;
                    }
                }
                else {
                    bresIncPgonStruct(pAet->bres);
                    pPrevAet = pAet;
                    pAet = pAet->next;
                }
            }
            insertionSort(&aet);
        }
    } else {
        /*
             *  for each scanline
             */
        for (y = et.ymin; y < et.ymax; ++y) {
            /*
                 *  Add a new edge to the active edge table when we
                 *  get to the next edge.
                 */
            if(pSLL != nullptr){
                if (y == pSLL->scanline) {
                    loadAet(&aet, pSLL->edgelist);
                    computeWAet(&aet);
                    pSLL = pSLL->next;
                }
            }
            pPrevAet = &aet;
            pAet = aet.next;
            pWEte = pAet;

            /*
                 *  for each active edge
                 */
            while (pAet != nullptr) {
                /*
                     *  add to the buffer only those edges that
                     *  are in the Winding active edge table.
                     */
                if (pWEte == pAet) {
                    pts->m_x = pAet->bres.minorAxis;
                    pts->m_y = y;
                    ++pts;
                    ++iPts;

                    /*
                         *  send out the buffer
                         */
                    if (iPts == NUMPTSTOBUFFER) {
                        std::shared_ptr<LPointBlock> tmpPtBlock = std::make_shared<LPointBlock>();
                        if(tmpPtBlock!= nullptr){
                            curPtBlock->next = tmpPtBlock;
                            curPtBlock = tmpPtBlock;
                            pts = &(curPtBlock->points[0]);
                            ++numFullPtBlocks;
                            iPts = 0;
                        }else{
                            return;
                        }
                    }
                    pWEte = pWEte->nextWETE;
                }

                /*
                 *     Evaluate the given edge at the given scanline.
                 *     If the edge has expired, then we leave it and fix up
                 *     the active edge table; otherwise, we increment the
                 *     x value to be ready for the next scanline.
                 *     The winding number rule is in effect, so we must notify
                 *     the caller when the edge has been removed so he
                 *     can reorder the Winding Active Edge Table.
                 */
                if (pAet->ymax == y) {          /* leaving this edge */
                    pPrevAet->next = pAet->next;
                    pAet = pPrevAet->next;
                    fixWAET = 1;
                    if (pAet != nullptr){
                        pAet->back = pPrevAet;
                    }
                }
                else {
                    bresIncPgonStruct(pAet->bres);
                    pPrevAet = pAet;
                    pAet = pAet->next;
                }
            }

            /*
                 *  recompute the winding active edge table if
                 *  we just resorted or have exited an edge.
                 */
            if (insertionSort(&aet) || fixWAET) {
                computeWAet(&aet);
                fixWAET = false;
            }
        }
    }

    /*
     *     Points to region
     *     Create an array of rectangles from a list of points.
     *     If indeed these things (POINTS, RECTS) are the same,
     *     then this proc is still needed, because it allocates
     *     storage for the array, which was allocated on the
     *     stack by the calling procedure.
     *
     */
    {
        int lastRow = 0;
        int extendTo = 0;
        bool needsExtend = false;
        std::vector<LRegionSpan> row;
        int rowSize = 0;

        m_extents.m_x = INT_MAX;
        m_extents.m_width = 0;
        m_innerArea = -1;

        int iCurPtBlock = iPts;
        curPtBlock = firstPtBlock;
        for (; numFullPtBlocks >= 0; --numFullPtBlocks) {
            /* the loop uses 2 points per iteration */
            int i = NUMPTSTOBUFFER >> 1;
            if (!numFullPtBlocks){ //最后一块，点数未满
                i = iCurPtBlock >> 1;
            }
            if(i) {
                row.resize(std::max<int>(row.size(), rowSize + i));
                for (LPoint<int> *pts = &(curPtBlock->points[0]); i--; pts += 2) {
                    const int width = pts[1].m_x - pts[0].m_x;
                    if (width) {
                        if (rowSize && row[rowSize-1].x2 == pts[0].m_x){
                            row[rowSize-1].x2 = pts[1].m_x;
                        }else{
                            row[rowSize++] = LRegionSpan(pts[0].m_x, pts[1].m_x);
                        }
                    }

                    if (rowSize) {
                        LPoint<int> *next = i ? &pts[2] : (numFullPtBlocks && iCurPtBlock ? &(curPtBlock->next->points[0]) : nullptr);

                        if (!next || next->m_y != pts[0].m_y) {
                            flushRow(row, pts[0].m_y, rowSize, &lastRow, &extendTo, &needsExtend);
                            rowSize = 0;
                        }
                    }
                }
            }
            curPtBlock = curPtBlock->next;
        }

        if (needsExtend) {
            for (int i = lastRow; i < (int)m_rects.size(); ++i){
                m_rects[i].setY2(extendTo);
            }
        }

        m_numRects = m_rects.size();

        if (m_numRects) {
            m_extents.m_y = m_rects[0].m_y;
            m_extents.setY2(m_rects[lastRow].y2());

            for (int i = 0; i < (int)m_rects.size(); ++i){
                updateInnerRect(m_rects[i]);
            }
        } else {
            m_extents.setCoords(0, 0, 0, 0);
        }
    }
}

void LRegion::flushRow(const std::vector<LRegionSpan>& spans, int y, int numSpans, int *lastRow, int *extendTo, bool *needsExtend)
{
    LRect<int> *regRects = m_rects.data() + *lastRow;
    bool canExtend = m_rects.size() - *lastRow == numSpans
                     && !(*needsExtend && *extendTo + 1 != y)
                     && (*needsExtend || regRects[0].m_y + regRects[0].height() == y);

    for (int i = 0; i < numSpans && canExtend; ++i) {
        if (regRects[i].m_x != spans[i].x1 || regRects[i].x2() != spans[i].x2 - 1){
            canExtend = false;
        }
    }

    if (canExtend) {
        *extendTo = y;
        *needsExtend = true;
    } else {
        if (*needsExtend) {
            for (int i = 0; i < (int)m_rects.size() - *lastRow; ++i){
                regRects[i].setY2(*extendTo);
            }
        }

        *lastRow = m_rects.size();
        m_rects.reserve(*lastRow + numSpans);
        for (int i = 0; i < numSpans; ++i){
            m_rects.push_back(LRect<int>(spans[i].x1, y, spans[i].width(), 1, false));
        }

        if (spans[0].x1 < m_extents.m_x){
            m_extents.m_x = spans[0].x1;
        }

        if (spans[numSpans-1].x2 - 1 > m_extents.x2()){
            m_extents.setX2(spans[numSpans-1].x2 - 1);
        }

        *needsExtend = false;
    }
}


LRegion::const_iterator LRegion::begin() const noexcept
{
    return m_numRects == 1 ? &m_extents : m_rects.data();
}

LRegion::const_iterator LRegion::end() const noexcept
{
    return begin() + m_numRects;
}

bool LRegion::containsInner(const LRect<int> &r2) const
{
    const LRect<int> &r1 = m_innerRect;
    return r2.m_x >= r1.m_x && r2.x2() <= r1.x2()
           && r2.m_y >= r1.m_y && r2.y2() <= r1.y2();
}

bool LRegion::within(const LRect<int> &r1) const
{
    const LRect<int> &r2 = m_extents;
    return r2.m_x >= r1.m_x && r2.x2() <= r1.x2()
           && r2.m_y >= r1.m_y && r2.y2() <= r1.y2();
}

bool LRegion::rectInRegion(int rx, int ry, uint32_t rwidth, uint32_t rheight) const
{
    const LRect<int> *pbox;
    const LRect<int> *pboxEnd;
    LRect<int> rect(rx, ry, rx+rwidth, ry+rheight);
    LRect<int> *prect = &rect;
    int partIn, partOut;

    if (m_numRects == 0 || !extendCheck(m_extents, rect)){
        return false;
    }

    partOut = false;
    partIn = false;

    /* can stop when both partOut and partIn are true, or we reach prect->y2 */
    pbox = (m_numRects == 1) ? &m_extents : m_rects.data();
    pboxEnd = pbox + m_numRects;
    for (; pbox < pboxEnd; ++pbox) {
        if (pbox->y2() < ry){
            continue;
        }

        if (pbox->m_y > ry) {
            partOut = true;
            if (partIn || pbox->m_y > prect->y2()){
                break;
            }
            ry = pbox->m_y;
        }

        if (pbox->x2() < rx){
            continue;            /* not far enough over yet */
        }

        if (pbox->m_x > rx) {
            partOut = true;      /* missed part of rectangle to left */
            if (partIn){
                break;
            }
        }

        if (pbox->m_x <= prect->x2()) {
            partIn = true;      /* definitely overlap */
            if (partOut){
                break;
            }
        }

        if (pbox->x2() >= prect->x2()) {
            ry = pbox->y2() + 1;     /* finished with this band */
            if (ry > prect->y2()){
                break;
            }
            rx = prect->m_x;  /* reset x out to left again */
        } else {
            /*
             * Because boxes in a band are maximal width, if the first box
             * to overlap the rectangle doesn't completely cover it in that
             * band, the rectangle must be partially out, since some of it
             * will be uncovered in that band. partIn will have been set true
             * by now...
             */
            break;
        }
    }
    return partIn;
}

bool LRegion::pointInRegion(int x, int y) const
{
    int i;

    if (isEmpty()){
        return false;
    }
    if (!m_extents.contains(x, y)){
        return false;
    }
    if (m_numRects == 1){
        return m_extents.contains(x, y);
    }
    if (m_innerRect.contains(x, y)){
        return true;
    }
    for (i = 0; i < m_numRects; ++i) {
        if (m_rects[i].contains(x, y)){
            return true;
        }
    }
    return false;
}

bool LRegion::canAppend(const LRect<int>* r) const
{
    if(r->isValid()){
        const LRect<int>* myLast = (m_numRects == 1) ? &m_extents : &m_rects[m_numRects - 1];
        if (r->m_y >= myLast->y2()){
            return true;
        }
        if (r->m_y == myLast->m_y
            && r->y2() == myLast->y2()
            && r->m_x >= myLast->x2())
        {
            return true;
        }
    }
    return false;
}

bool LRegion::canPrepend(const LRect<int> *r) const
{
    if(r->isValid()){
        const LRect<int> *myFirst = (m_numRects == 1) ? &m_extents : &m_rects[0];
        if (r->y2() <= myFirst->m_y){ // not overlapping
            return true;
        }
        if (r->m_y == myFirst->m_y
            && r->y2() == myFirst->y2()
            && r->x2() <= myFirst->m_x)
        {
            return true;
        }
    }
    return false;
}

void LRegion::append(const LRect<int> *r)
{
    if(r->isValid()){
        LRect<int> *myLast = (m_numRects == 1 ? &m_extents : &m_rects[m_numRects - 1]);
        if (mergeFromRight(myLast, r)) {
            if (m_numRects > 1) {
                const LRect<int> *nextToTop = (m_numRects > 2 ? myLast - 2 : nullptr);
                if (mergeFromBelow(myLast - 1, myLast, nextToTop, nullptr)){
                    --m_numRects;
                }
            }
        } else if (mergeFromBelow(myLast, r, (m_numRects > 1 ? myLast - 1 : nullptr), nullptr)) {
            // nothing
        } else {
            vectorize();
            updateInnerRect(*r);

            if(m_rects.size() > m_numRects){
                m_rects[m_numRects] = *r;
                m_numRects++;
            }else{
                m_rects.push_back(*r);
                m_numRects = m_rects.size();
            }
        }
        m_extents.setCoords(std::min(m_extents.m_x, r->m_x),
                          std::min(m_extents.m_y, r->m_y),
                          std::max(m_extents.x2(), r->x2()),
                            std::max(m_extents.y2(), r->y2()));
    }
}

void LRegion::append(const LRegion *r)
{
    if (r->m_numRects == 1) {
        append(&(r->m_extents));
        return;
    }

    vectorize();

    LRect<int> *destRect = m_rects.data() + m_numRects;
    const LRect<int> *srcRect = r->m_rects.data();
    int numAppend = r->m_numRects;

    // try merging
    {
        const LRect<int> *rFirst = srcRect;
        LRect<int> *myLast = destRect - 1;
        const LRect<int> *nextToLast = (m_numRects > 1 ? myLast - 1 : nullptr);
        if (mergeFromRight(myLast, rFirst)) {
            ++srcRect;
            --numAppend;
            const LRect<int> *rNextToFirst = (numAppend > 1 ? rFirst + 2 : nullptr);
            if (mergeFromBelow(myLast, rFirst + 1, nextToLast, rNextToFirst)) {
                ++srcRect;
                --numAppend;
            }
            if (m_numRects  > 1) {
                nextToLast = (m_numRects > 2 ? myLast - 2 : nullptr);
                rNextToFirst = (numAppend > 0 ? srcRect : nullptr);
                if (mergeFromBelow(myLast - 1, myLast, nextToLast, rNextToFirst)) {
                    --destRect;
                    --m_numRects;
                }
            }
        } else if (mergeFromBelow(myLast, rFirst, nextToLast, rFirst + 1)) {
            ++srcRect;
            --numAppend;
        }
    }

    // append rectangles
    if (numAppend > 0) {
        const int newNumRects = m_numRects + numAppend;
        if (newNumRects > m_rects.size()) {
            m_rects.resize(newNumRects);
            destRect = m_rects.data() + m_numRects;
        }
        memcpy(destRect, srcRect, numAppend * sizeof(LRect<int>));
        m_numRects = newNumRects;
    }

    // update inner rectangle
    if (m_innerArea < r->m_innerArea) {
        m_innerArea = r->m_innerArea;
        m_innerRect = r->m_innerRect;
    }

    // update extents
    destRect = &m_extents;
    srcRect = &r->m_extents;
    m_extents.setCoords(std::min(destRect->m_x, srcRect->m_x),
                        std::min(destRect->m_y, srcRect->m_y),
                        std::max(destRect->x2(), srcRect->x2()),
                        std::max(destRect->y2(), srcRect->y2()));

}

void LRegion::prepend(const LRect<int> *r)
{
    if(r->isValid()){
        LRect<int> *myFirst = (m_numRects == 1 ? &m_extents : m_rects.data());
        if (mergeFromLeft(myFirst, r)) {
            if (m_numRects > 1) {
                const LRect<int> *nextToFirst = (m_numRects > 2 ? myFirst + 2 : nullptr);
                if (mergeFromAbove(myFirst + 1, myFirst, nextToFirst, nullptr)) {
                    --m_numRects;
                    memmove(m_rects.data(), m_rects.data() + 1,
                            m_numRects * sizeof(LRect<int>));
                }
            }
        } else if (mergeFromAbove(myFirst, r, (m_numRects > 1 ? myFirst + 1 : nullptr), nullptr)) {
            // nothing
        } else {
            vectorize();
            ++m_numRects;
            updateInnerRect(*r);
            m_rects.insert(m_rects.begin(), 1, *r);
        }
        m_extents.setCoords(std::min(m_extents.m_x, r->m_x),
                          std::min(m_extents.m_y, r->m_y),
                          std::max(m_extents.x2(), r->x2()),
                            std::max(m_extents.y2(), r->y2()));
    }
}

void LRegion::prepend(const LRegion *r)
{
    if (r->m_numRects == 1) {
        prepend(&(r->m_extents));
        return;
    }

    vectorize();

    int numPrepend = r->m_numRects;
    int numSkip = 0;

    // try merging
    {
        LRect<int> *myFirst = m_rects.data();
        const LRect<int> *nextToFirst = (m_numRects > 1 ? myFirst + 1 : nullptr);
        const LRect<int> *rLast = r->m_rects.data() + r->m_numRects - 1;
        const LRect<int> *rNextToLast = (r->m_numRects > 1 ? rLast - 1 : nullptr);
        if (mergeFromLeft(myFirst, rLast)) {
            --numPrepend;
            --rLast;
            rNextToLast = (numPrepend > 1 ? rLast - 1 : nullptr);
            if (mergeFromAbove(myFirst, rLast, nextToFirst, rNextToLast)) {
                --numPrepend;
                --rLast;
            }
            if (m_numRects  > 1) {
                nextToFirst = (m_numRects > 2? myFirst + 2 : nullptr);
                rNextToLast = (numPrepend > 0 ? rLast : nullptr);
                if (mergeFromAbove(myFirst + 1, myFirst, nextToFirst, rNextToLast)) {
                    --m_numRects;
                    ++numSkip;
                }
            }
        } else if (mergeFromAbove(myFirst, rLast, nextToFirst, rNextToLast)) {
            --numPrepend;
        }
    }

    if (numPrepend > 0) {
        const int newNumRects = m_numRects + numPrepend;
        if (newNumRects > m_rects.size()){
            m_rects.resize(newNumRects);
        }

        // move existing rectangles
        memmove(m_rects.data() + numPrepend, m_rects.data() + numSkip,
                m_numRects * sizeof(LRect<int>));

        // prepend new rectangles
        memcpy(m_rects.data(), r->m_rects.data(), numPrepend * sizeof(LRect<int>));

        m_numRects = newNumRects;
    }

    // update inner rectangle
    if (m_innerArea < r->m_innerArea) {
        m_innerArea = r->m_innerArea;
        m_innerRect = r->m_innerRect;
    }

    // update extents
    m_extents.setCoords(std::min(m_extents.m_x, r->m_extents.m_x),
                        std::min(m_extents.m_y, r->m_extents.m_y),
                        std::max(m_extents.x2(), r->m_extents.x2()),
                        std::max(m_extents.y2(), r->m_extents.y2()));

}

bool LRegion::mergeFromRight(LRect<int> *left, const LRect<int> *right)
{
    if (canMergeFromRight(left, right)) {
        left->setX2(right->x2(), false);
        updateInnerRect(*left);
        return true;
    }
    return false;
}

bool LRegion::mergeFromLeft(LRect<int> *right, const LRect<int> *left)
{
    if (canMergeFromLeft(right, left)) {
        right->setX1(left->m_x, false);
        updateInnerRect(*right);
        return true;
    }
    return false;
}

bool LRegion::mergeFromBelow(LRect<int> *top,
                             const LRect<int> *bottom,
                             const LRect<int> *nextToTop,
                             const LRect<int> *nextToBottom)
{
    if (canMergeFromBelow(top, bottom, nextToTop, nextToBottom)) {
        top->setY2(bottom->y2(), false);
        updateInnerRect(*top);
        return true;
    }
    return false;
}

bool LRegion::mergeFromAbove(LRect<int> *bottom,
                             const LRect<int> *top,
                             const LRect<int> *nextToBottom,
                             const LRect<int> *nextToTop)
{
    if (canMergeFromBelow(top, bottom, nextToTop, nextToBottom)) {
        bottom->setY1(top->m_y, false);
        updateInnerRect(*bottom);
        return true;
    }
    return false;
}

void LRegion::intersect0(const LRect<int> &r)
{
    //const LRect<int> r = rect;
    m_extents = LRect<int>();
    m_innerRect = LRect<int>();
    m_innerArea = -1;

    LRect<int> *dest = m_rects.data();
    const LRect<int> *src = dest;
    int n = m_numRects;
    m_numRects = 0;
    while (n--) {
        *dest = rectIntersectNormalized(*src++, r);
        if (dest->isEmpty()){
            continue;
        }

        if (m_numRects == 0) {
            m_extents = *dest;
        } else {
            m_extents.m_x = std::min(m_extents.m_x, dest->m_x);
            // hw: extents.top() will never change after initialization
            m_extents.setX2(std::max(m_extents.x2(), dest->x2()));
            m_extents.setY2(std::max(m_extents.y2(), dest->y2()));

            const LRect<int> *nextToLast = (m_numRects > 1 ? dest - 2 : nullptr);

            // mergeFromBelow inlined and optimized
            if (canMergeFromBelow(dest - 1, dest, nextToLast, nullptr)) {
                if (!n || src->m_y != dest->m_y || src->m_x > r.x2()) {
                    LRect<int> *prev = dest - 1;
                    prev->setY2(dest->y2());
                    updateInnerRect(*prev);
                    continue;
                }
            }
        }
        updateInnerRect(*dest);
        ++dest;
        ++m_numRects;
    }
    m_rects.resize(m_numRects);
}

/*-
 *-----------------------------------------------------------------------
 * miUnionNonO --
 *      Handle a non-overlapping band for the union operation. Just
 *      Adds the rectangles into the region. Doesn't have to check for
 *      subsumption or anything.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      dest.numRects is incremented and the final rectangles overwritten
 *      with the rectangles we're passed.
 *
 *-----------------------------------------------------------------------
 */
void LRegion::miUnionNonO(LRegion *dest, const LRect<int> *r, const LRect<int> *rEnd, int y1, int y2)
{
    assert(y1 < y2);
    int num = std::abs(rEnd-r);
    if ((dest->m_numRects + num) > dest->m_rects.size()){
        dest->m_rects.resize(dest->m_numRects + num);
    }
    LRect<int>* pNextRect = dest->m_rects.data() + dest->m_numRects;

    while (r != rEnd) {
        assert(r->m_x < r->x2());
        pNextRect->setCoords(r->m_x, y1, r->x2(), y2);
        dest->m_numRects++;
        ++pNextRect;
        ++r;
    }
}


/*-
 *-----------------------------------------------------------------------
 * miUnionO --
 *      Handle an overlapping band for the union operation. Picks the
 *      left-most rectangle each time and merges it into the region.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Rectangles are overwritten in dest.rects and dest.numRects will
 *      be changed.
 *
 *-----------------------------------------------------------------------
 */
void LRegion::miUnionO(LRegion *dest,
                       const LRect<int> *r1,
                       const LRect<int> *r1End, const
                       LRect<int> *r2,
                       const LRect<int> *r2End,
                       int y1,
                       int y2)
{
    LRect<int> *pNextRect;
    int numMax = std::abs(r1End-r1) + std::abs(r2End-r2);
    if ((dest->m_numRects + numMax) > dest->m_rects.size()){
        dest->m_rects.resize(dest->m_numRects + numMax);
    }

    pNextRect = dest->m_rects.data() + dest->m_numRects;

#define MERGERECT(r)             \
    if ((dest->m_numRects != 0) &&  \
        (pNextRect[-1].m_y == y1) &&  \
        (pNextRect[-1].y2() == y2) &&  \
        (pNextRect[-1].x2() >= r->m_x)) { \
        if (pNextRect[-1].x2() < r->x2()) { \
            pNextRect[-1].setX2(r->x2());  \
            dest->updateInnerRect(pNextRect[-1]); \
            assert(pNextRect[-1].m_x < pNextRect[-1].x2()); \
        }  \
    } else { \
        assert(r->m_x < r->x2()); \
        pNextRect->setCoords(r->m_x, y1, r->x2(), y2); \
        dest->updateInnerRect(*pNextRect); \
        dest->m_numRects++;  \
        pNextRect++;  \
    }  \
    r++;

    assert(y1 < y2);
    while (r1 != r1End && r2 != r2End) {
        if (r1->m_x < r2->m_x) {
            MERGERECT(r1)
        } else {
            MERGERECT(r2)
        }
    }

    if (r1 != r1End) {
        do {
            MERGERECT(r1)
        } while (r1 != r1End);
    } else {
        while (r2 != r2End) {
            MERGERECT(r2)
        }
    }
#undef MERGERECT
}

/*-
 *-----------------------------------------------------------------------
 * miCoalesce --
 *      Attempt to merge the boxes in the current band with those in the
 *      previous one. Used only by miRegionOp.
 *
 * Results:
 *      The new index for the previous band.
 *
 * Side Effects:
 *      If coalescing takes place:
 *          - rectangles in the previous band will have their y2 fields
 *            altered.
 *          - dest.numRects will be decreased.
 *
 *-----------------------------------------------------------------------
 */
int LRegion::miCoalesce(LRegion *dest, int prevStart, int curStart)
{
    LRect<int> *pPrevBox;   /* Current box in previous band */
    LRect<int> *pCurBox;    /* Current box in current band */
    LRect<int> *pRegEnd;    /* End of region */
    int curNumRects;    /* Number of rectangles in current band */
    int prevNumRects;   /* Number of rectangles in previous band */
    int bandY1;         /* Y1 coordinate for current band */
    LRect<int> *rData = dest->m_rects.data();

    pRegEnd = rData + dest->m_numRects;

    pPrevBox = rData + prevStart;
    prevNumRects = curStart - prevStart;

    /*
     * Figure out how many rectangles are in the current band. Have to do
     * this because multiple bands could have been added in miRegionOp
     * at the end when one region has been exhausted.
     */
    pCurBox = rData + curStart;
    bandY1 = pCurBox->m_y;
    for (curNumRects = 0; pCurBox != pRegEnd && pCurBox->m_y == bandY1; ++curNumRects) {
        ++pCurBox;
    }

    if (pCurBox != pRegEnd) {
        /*
         * If more than one band was added, we have to find the start
         * of the last band added so the next coalescing job can start
         * at the right place... (given when multiple bands are added,
         * this may be pointless -- see above).
         */
        --pRegEnd;
        while ((pRegEnd - 1)->m_y == pRegEnd->m_y){
            --pRegEnd;
        }
        curStart = pRegEnd - rData;
        pRegEnd = rData + dest->m_numRects;
    }

    if (curNumRects == prevNumRects && curNumRects != 0) {
        pCurBox -= curNumRects;
        /*
         * The bands may only be coalesced if the bottom of the previous
         * matches the top scanline of the current.
         */
        if (pPrevBox->y2() == pCurBox->m_y) {
            /*
             * Make sure the bands have boxes in the same places. This
             * assumes that boxes have been added in such a way that they
             * cover the most area possible. I.e. two boxes in a band must
             * have some horizontal space between them.
             */
            do {
                if (pPrevBox->m_x != pCurBox->m_x || pPrevBox->x2() != pCurBox->x2()) {
                    // The bands don't line up so they can't be coalesced.
                    return curStart;
                }
                ++pPrevBox;
                ++pCurBox;
                --prevNumRects;
            } while (prevNumRects != 0);

            dest->m_numRects -= curNumRects;
            pCurBox -= curNumRects;
            pPrevBox -= curNumRects;

            /*
             * The bands may be merged, so set the bottom y of each box
             * in the previous band to that of the corresponding box in
             * the current band.
             */
            do {
                pPrevBox->setY2(pCurBox->y2());
                dest->updateInnerRect(*pPrevBox);
                ++pPrevBox;
                ++pCurBox;
                curNumRects -= 1;
            } while (curNumRects != 0);

            /*
             * If only one band was added to the region, we have to backup
             * curStart to the start of the previous band.
             *
             * If more than one band was added to the region, copy the
             * other bands down. The assumption here is that the other bands
             * came from the same region as the current one and no further
             * coalescing can be done on them since it's all been done
             * already... curStart is already in the right place.
             */
            if (pCurBox == pRegEnd) {
                curStart = prevStart;
            } else {
                do {
                    *pPrevBox++ = *pCurBox++;
                    dest->updateInnerRect(*pPrevBox);
                } while (pCurBox != pRegEnd);
            }
        }
    }
    return curStart;
}

void LRegion::miRegionOp(LRegion *dest,
                         const LRegion *reg1,
                         const LRegion *reg2,
                         OverlapFunc overlapFunc,
                         NonOverlapFunc nonOverlap1Func,
                         NonOverlapFunc nonOverlap2Func)
{
    const LRect<int> *r1;         // Pointer into first region
    const LRect<int> *r2;         // Pointer into 2d region
    const LRect<int> *r1End;               // End of 1st region
    const LRect<int> *r2End;               // End of 2d region
    int ybot;          // Bottom of intersection
    int ytop;          // Top of intersection
    int prevBand;               // Index of start of previous band in dest
    int curBand;                // Index of start of current band in dest
    const LRect<int> *r1BandEnd;  // End of current band in r1
    const LRect<int> *r2BandEnd;  // End of current band in r2
    int top;                    // Top of non-overlapping band
    int bot;                    // Bottom of non-overlapping band

    /*
     * Initialization:
     *  set r1, r2, r1End and r2End appropriately, preserve the important
     * parts of the destination region until the end in case it's one of
     * the two source regions, then mark the "new" region empty, allocating
     * another array of rectangles for it to use.
     */
    if (reg1->m_numRects == 1){
        r1 = &(reg1->m_extents);
    }else{
        r1 = reg1->m_rects.data();
    }
    if (reg2->m_numRects == 1){
        r2 = &reg2->m_extents;
    }else{
        r2 = reg2->m_rects.data();
    }

    r1End = r1 + reg1->m_numRects;
    r2End = r2 + reg2->m_numRects;

    dest->vectorize();

    dest->m_numRects = 0;

    /*
     * Allocate a reasonable number of rectangles for the new region. The idea
     * is to allocate enough so the individual functions don't need to
     * reallocate and copy the array, which is time consuming, yet we don't
     * have to worry about using too much memory. I hope to be able to
     * nuke the realloc() at the end of this function eventually.
     */
    dest->m_rects.resize(std::max(reg1->m_numRects,reg2->m_numRects) * 2);

    /*
     * Initialize ybot and ytop.
     * In the upcoming loop, ybot and ytop serve different functions depending
     * on whether the band being handled is an overlapping or non-overlapping
     * band.
     *  In the case of a non-overlapping band (only one of the regions
     * has points in the band), ybot is the bottom of the most recent
     * intersection and thus clips the top of the rectangles in that band.
     * ytop is the top of the next intersection between the two regions and
     * serves to clip the bottom of the rectangles in the current band.
     *  For an overlapping band (where the two regions intersect), ytop clips
     * the top of the rectangles of both regions and ybot clips the bottoms.
     */
    if (reg1->m_extents.m_y < reg2->m_extents.m_y){
        ybot = reg1->m_extents.m_y;
    }
    else{
        ybot = reg2->m_extents.m_y;
    }

    /*
     * prevBand serves to mark the start of the previous band so rectangles
     * can be coalesced into larger rectangles. qv. miCoalesce, above.
     * In the beginning, there is no previous band, so prevBand == curBand
     * (curBand is set later on, of course, but the first band will always
     * start at index 0). prevBand and curBand must be indices because of
     * the possible expansion, and resultant moving, of the new region's
     * array of rectangles.
     */
    prevBand = 0;

    do {
        curBand = dest->m_numRects;

        /*
         * This algorithm proceeds one source-band (as opposed to a
         * destination band, which is determined by where the two regions
         * intersect) at a time. r1BandEnd and r2BandEnd serve to mark the
         * rectangle after the last one in the current band for their
         * respective regions.
         */
        r1BandEnd = r1;
        while ((r1BandEnd != r1End) && (r1BandEnd->m_y == r1->m_y || r1BandEnd->isEmpty()) ){
            ++r1BandEnd;
        }

        r2BandEnd = r2;
        while (r2BandEnd != r2End && (r2BandEnd->m_y == r2->m_y || r2BandEnd->isEmpty())){
            ++r2BandEnd;
        }

        /*
         * First handle the band that doesn't intersect, if any.
         *
         * Note that attention is restricted to one band in the
         * non-intersecting region at once, so if a region has n
         * bands between the current position and the next place it overlaps
         * the other, this entire loop will be passed through n times.
         */
        if (r1->m_y < r2->m_y) {
            top = std::max(r1->m_y, ybot);
            bot = std::min(r1->y2(), r2->m_y);

            if (nonOverlap1Func != nullptr && bot > top){
                (*nonOverlap1Func)(dest, r1, r1BandEnd, top, bot);
            }
            ytop = r2->m_y;
        } else if (r2->m_y < r1->m_y) {
            top = std::max(r2->m_y, ybot);
            bot = std::min(r2->y2(), r1->m_y);

            if (nonOverlap2Func != nullptr && bot > top){
                (*nonOverlap2Func)(dest, r2, r2BandEnd, top, bot);
            }
            ytop = r1->m_y;
        } else {
            ytop = r1->m_y;
        }

        /*
         * If any rectangles got added to the region, try and coalesce them
         * with rectangles from the previous band. Note we could just do
         * this test in miCoalesce, but some machines incur a not
         * inconsiderable cost for function calls, so...
         */
        if (dest->m_numRects != curBand)
            prevBand = miCoalesce(dest, prevBand, curBand);

        /*
         * Now see if we've hit an intersecting band. The two bands only
         * intersect if ybot >= ytop
         */
        ybot = std::min(r1->y2(), r2->y2());
        curBand = dest->m_numRects;
        if (ybot > ytop){
            (*overlapFunc)(dest, r1, r1BandEnd, r2, r2BandEnd, ytop, ybot);
        }

        if (dest->m_numRects != curBand){
            prevBand = miCoalesce(dest, prevBand, curBand);
        }

        /*
         * If we've finished with a band (y2 == ybot) we skip forward
         * in the region to the next band.
         */
        if (r1->y2() == ybot){
            r1 = r1BandEnd;
        }
        if (r2->y2() == ybot){
            r2 = r2BandEnd;
        }
    } while (r1 != r1End && r2 != r2End);

    /*
     * Deal with whichever region still has rectangles left.
     */
    curBand = dest->m_numRects;
    if (r1 != r1End) {
        if (nonOverlap1Func != nullptr) {
            do {
                r1BandEnd = r1;
                while (r1BandEnd < r1End && (r1BandEnd->m_y == r1->m_y || r1BandEnd->isEmpty())){
                    ++r1BandEnd;
                }
                (*nonOverlap1Func)(dest, r1, r1BandEnd, std::max(r1->m_y, ybot), r1->y2());
                r1 = r1BandEnd;
            } while (r1 != r1End);
        }
    } else if ((r2 != r2End) && (nonOverlap2Func != nullptr)) {
        do {
            r2BandEnd = r2;
            while (r2BandEnd < r2End && (r2BandEnd->m_y == r2->m_y || r2BandEnd->isEmpty())){
                ++r2BandEnd;
            }
            (*nonOverlap2Func)(dest, r2, r2BandEnd, std::max(r2->m_y, ybot), r2->y2());
            r2 = r2BandEnd;
        } while (r2 != r2End);
    }

    if (dest->m_numRects != curBand){
        (void)miCoalesce(dest, prevBand, curBand);
    }

    /*
     * A bit of cleanup. To keep regions from growing without bound,
     * we shrink the array of rectangles to match the new number of
     * rectangles in the region.
     *
     * Only do this stuff if the number of rectangles allocated is more than
     * twice the number of rectangles in the region (a simple optimization).
     */
    if (std::max(4, dest->m_numRects) < (dest->m_rects.size() >> 1)){
        dest->m_rects.resize(dest->m_numRects);
    }
}
/*-
 *-----------------------------------------------------------------------
 * miSetExtents --
 *      Reset the extents and innerRect of a region to what they should be.
 *      Called by miSubtract and miIntersect b/c they can't figure it out
 *      along the way or do so easily, as miUnion can.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      The region's 'extents' and 'innerRect' structure is overwritten.
 *
 *-----------------------------------------------------------------------
 */
void LRegion::miSetExtents(LRegion *dest)
{
    const LRect<int> *pBox, *pBoxEnd;
    LRect<int> *pExtents;

    dest->m_innerRect.setCoords(0, 0, 0, 0);
    dest->m_innerArea = -1;
    if (dest->m_numRects == 0) {
        dest->m_extents.setCoords(0, 0, 0, 0);
        return;
    }

    pExtents = &(dest->m_extents);
    if (dest->m_rects.empty()){
        pBox = &(dest->m_extents);
    }
    else{
        pBox = dest->m_rects.data();
    }
    pBoxEnd = pBox + dest->m_numRects - 1;

    /*
     * Since pBox is the first rectangle in the region, it must have the
     * smallest y1 and since pBoxEnd is the last rectangle in the region,
     * it must have the largest y2, because of banding. Initialize x1 and
     * x2 from  pBox and pBoxEnd, resp., as good things to initialize them
     * to...
     */
    pExtents->m_x = pBox->m_x;
    pExtents->m_y = pBox->m_y;
    pExtents->setX2(pBoxEnd->x2());
    pExtents->setY2(pBoxEnd->y2());

    while (pBox <= pBoxEnd) {
        if (pBox->m_x < pExtents->m_x){
            pExtents->m_x= pBox->m_x;
        }
        if (pBox->x2() > pExtents->x2()){
            pExtents->setX2(pBox->x2());
        }
        dest->updateInnerRect(*pBox);
        ++pBox;
    }
}

/*======================================================================
 *          Region Intersection
 *====================================================================*/
/*-
 *-----------------------------------------------------------------------
 * miIntersectO --
 *      Handle an overlapping band for miIntersect.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Rectangles may be added to the region.
 *
 *-----------------------------------------------------------------------
 */
void LRegion::miIntersectO(LRegion *dest,
                           const LRect<int> *r1,
                           const LRect<int> *r1End,
                           const LRect<int> *r2,
                           const LRect<int> *r2End,
                           int y1,
                           int y2)
{
    int x1;
    int x2;

    int numMax = std::abs(r1End-r1) + std::abs(r2End-r2);
    if ((dest->m_numRects + numMax) > dest->m_rects.size()){
        dest->m_rects.resize(dest->m_numRects + numMax);
    }
    LRect<int>* pNextRect = dest->m_rects.data() + dest->m_numRects;

    while (r1 != r1End && r2 != r2End) {
        x1 = std::max(r1->m_x, r2->m_x);
        x2 = std::min(r1->x2(), r2->x2());

        /*
         * If there's any overlap between the two rectangles, add that
         * overlap to the new region.
         * There's no need to check for subsumption because the only way
         * such a need could arise is if some region has two rectangles
         * right next to each other. Since that should never happen...
         */
        if (x1 < x2) {
            assert(y1 < y2);
            //MEMCHECK(dest, pNextRect, dest.rects)
            pNextRect->setCoords(x1, y1, x2, y2);
            ++dest->m_numRects;
            ++pNextRect;
        }

        /*
         * Need to advance the pointers. Shift the one that extends
         * to the right the least, since the other still has a chance to
         * overlap with that region's next rectangle, if you see what I mean.
         */
        if (r1->x2() < r2->x2()) {
            ++r1;
        } else if (r2->x2() < r1->x2()) {
            ++r2;
        } else {
            ++r1;
            ++r2;
        }
    }
}

void LRegion::unionRegion(const LRegion *reg1, const LRegion *reg2, LRegion *dest)
{
    assert(!reg1->isEmpty() && !reg2->isEmpty());
    assert(!reg1->m_innerRect.contains(reg2->m_extents));
    assert(!reg2->m_innerRect.contains(reg1->m_extents));
    assert(!equalRegion(reg1, reg2));
    assert(!reg1->canAppend(reg2));
    assert(!reg2->canAppend(reg1));

    if (reg1->m_innerArea > reg2->m_innerArea) {
        dest->m_innerArea = reg1->m_innerArea;
        dest->m_innerRect = reg1->m_innerRect;
    } else {
        dest->m_innerArea = reg2->m_innerArea;
        dest->m_innerRect = reg2->m_innerRect;
    }
    miRegionOp(dest, reg1, reg2, miUnionO, miUnionNonO, miUnionNonO);

    dest->m_extents.setCoords(std::min(reg1->m_extents.m_x, reg2->m_extents.m_x),
                               std::min(reg1->m_extents.m_y, reg2->m_extents.m_y),
                               std::max(reg1->m_extents.x2(), reg2->m_extents.x2()),
                              std::max(reg1->m_extents.y2(), reg2->m_extents.y2()));
}

/*======================================================================
 *        Region Subtraction
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miSubtractNonO --
 *      Deal with non-overlapping band for subtraction. Any parts from
 *      region 2 we discard. Anything from region 1 we add to the region.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      dest may be affected.
 *
 *-----------------------------------------------------------------------
 */
void LRegion::miSubtractNonO1(LRegion *dest, const LRect<int> *r, const LRect<int> *rEnd, int y1, int y2)
{
    miUnionNonO(dest, r, rEnd, y1, y2);
}

/*-
 *-----------------------------------------------------------------------
 * miSubtractO --
 *      Overlapping band subtraction. x1 is the left-most point not yet
 *      checked.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      dest may have rectangles added to it.
 *
 *-----------------------------------------------------------------------
 */
void LRegion::miSubtractO(LRegion *dest,
                          const LRect<int> *r1,
                          const LRect<int> *r1End,
                          const LRect<int> *r2,
                          const LRect<int> *r2End,
                          int y1,
                          int y2)
{
//    assert(y1 < y2);

    int numMax = std::abs(r1End-r1) + std::abs(r2End-r2);
    if ((dest->m_numRects + numMax) > (int)(dest->m_rects.size())){
        dest->m_rects.resize(dest->m_numRects + numMax);
    }

    int x1 = r1->m_x;
    LRect<int> *pNextRect = dest->m_rects.data() + dest->m_numRects;

    while (r1 != r1End && r2 != r2End) {
        if (r2->x2() <= x1) {
            /*
             * Subtrahend missed the boat: go to next subtrahend.
             */
            ++r2;
        } else if (r2->m_x <= x1) {
            /*
             * Subtrahend precedes minuend: nuke left edge of minuend.
             */
            x1 = r2->x2();
            if (x1 >= r1->x2()) {
                /*
                 * Minuend completely covered: advance to next minuend and
                 * reset left fence to edge of new minuend.
                 */
                ++r1;
                if (r1 != r1End){
                    x1 = r1->m_x;
                }
            } else {
                // Subtrahend now used up since it doesn't extend beyond minuend
                ++r2;
            }
        } else if (r2->m_x < r1->x2()) {
            /*
             * Left part of subtrahend covers part of minuend: add uncovered
             * part of minuend to region and skip to next subtrahend.
             */
            assert(x1 < r2->m_x);
            //MEMCHECK(dest, pNextRect, dest.rects)
            pNextRect->setCoords(x1, y1, r2->m_x, y2);
            dest->m_numRects++;
            ++pNextRect;

            x1 = r2->x2();
            if (x1 >= r1->x2()) {
                /*
                 * Minuend used up: advance to new...
                 */
                ++r1;
                if (r1 != r1End){
                    x1 = r1->m_x;
                }
            } else {
                // Subtrahend used up
                ++r2;
            }
        } else {
            /*
             * Minuend used up: add any remaining piece before advancing.
             */
            if (r1->x2() > x1) {
                //MEMCHECK(dest, pNextRect, dest.rects)
                pNextRect->setCoords(x1, y1, r1->x2(), y2);
                dest->m_numRects++;
                ++pNextRect;
            }
            ++r1;
            if (r1 != r1End){
                x1 = r1->m_x;
            }
        }
    }

    /*
     * Add remaining minuend rectangles to region.
     */
    while (r1 != r1End) {
        assert(x1 < r1->x2());
        //MEMCHECK(dest, pNextRect, dest.rects)
        pNextRect->setCoords(x1, y1, r1->x2(), y2);
        dest->m_numRects++;
        ++pNextRect;

        ++r1;
        if (r1 != r1End){
            x1 = r1->m_x;
        }
    }
}

void LRegion::subtractRegion(const LRegion *regM, const LRegion *regS, LRegion *dest)
{
//    assert(!regM->isEmpty());
//    assert(!regS->isEmpty());
//    assert(extendCheck(regM->m_extents, regS->m_extents));
//    assert(!regS->contains(*regM));
//    assert(!equalRegion(regM, regS));

    miRegionOp(dest, regM, regS, miSubtractO, miSubtractNonO1, nullptr);

    /*
     * Can't alter dest's extents before we call miRegionOp because
     * it might be one of the source regions and miRegionOp depends
     * on the extents of those regions being the unaltered. Besides, this
     * way there's no checking against rectangles that will be nuked
     * due to coalescing, so we have to examine fewer rectangles.
     */
    miSetExtents(dest);
}

void LRegion::exorRegion(const LRegion *sra, const LRegion *srb, LRegion *dest)
{
    assert(!sra->isEmpty() && !srb->isEmpty());
    assert(extendCheck(sra->m_extents, srb->m_extents));
    assert(!equalRegion(sra, srb));

    LRegion tra, trb;

    if (!srb->containsInner(sra)){
        subtractRegion(sra, srb, &tra);
    }
    if (!sra->containsInner(srb)){
        subtractRegion(srb, sra, &trb);
    }

    assert(trb.isEmpty() || !tra.containsInner(&trb));
    assert(tra.isEmpty() || !trb.containsInner(&tra));

    if (tra.isEmpty()) {
        *dest = trb;
    } else if (trb.isEmpty()) {
        *dest = tra;
    } else if (tra.canAppend(&trb)) {
        *dest = tra;
        dest->append(&trb);
    } else if (trb.canAppend(&tra)) {
        *dest = trb;
        dest->append(&tra);
    } else {
        unionRegion(&tra, &trb, dest);
    }
}

void LRegion::offsetRegion(LRegion *region, int x, int y)
{
    for(LRect<int>& box : region ->m_rects){
        box.translate(x, y);
    }

    region->m_extents.translate(x, y);
    region->m_innerRect.translate(x, y);
}


LRegion& LRegion::unite(const std::shared_ptr<LRegion> r)
{
    if(r == nullptr){
        return *this;
    }

    if (isEmpty()){
        return *this = *r;
    }
    if (r->isEmpty()){
        return *this;
    }
    if (this == r.get()){
        return *this;
    }

    if (containsInner(r.get())) {
        return *this;
    } else if (r->containsInner(this)) {
        return *this = *r;
    } else if (canAppend(r.get())) {
        append(r.get());
        return *this;
    } else if (canPrepend(r.get())) {
        prepend(r.get());
        return *this;
    } else if (equalRegion(this, r.get())) {
        return *this;
    } else {
        LRegion t1(*this);
        unionRegion(&t1, r.get(), this);
        return *this;
    }
}

LRegion& LRegion::unite(const LRect<int> &r)
{
    if (isEmpty()){
        return *this = r;
    }

    if (r.isEmpty()){
        return *this;
    }

    if (containsInner(r)) {
        return *this;
    } else if (within(r)) {
        return *this = r;
    } else if (canAppend(&r)) {
        append(&r);
        return *this;
    } else if (canPrepend(&r)) {
        prepend(&r);
        return *this;
    } else if (m_numRects == 1 && m_extents == r) {
        return *this;
    } else {
        LRegion t1(*this);
        LRegion t2(r);
        unionRegion(&t1, &t2, this);
        return *this;
    }
}

LRegion &LRegion::intersect(const std::shared_ptr<LRegion> r)
{
    if (isEmpty()){
        return *this;
    }

    if(r == nullptr){
        clear();
        return *this;
    }

    if(r->isEmpty()
        || !extendCheck(m_extents, r->m_extents)){
        clear();
        return *this;
    }

    /* this is fully contained in r */
    if (r->containsInner(this)){
        return *this;
    }

    /* r is fully contained in this */
    if (containsInner(r.get())){
        *this = *r;
        return *this;
    }

    if (r->m_numRects == 1 && m_numRects == 1) {
        const LRect<int> rect = rectIntersectNormalized(r->m_extents, m_extents);
        *this = rect;
        return *this;
    } else if (r->m_numRects == 1) {
        intersect0(r->m_extents);
        return *this;
    } else if (m_numRects == 1) {
        LRegion result(*r);
        result.intersect0(m_extents);
        *this = result;
        return *this;
    }

    LRegion t(*this);
    miRegionOp(this, &t, r.get(), miIntersectO, nullptr, nullptr);

    /*
     * Can't alter dest's extents before we call miRegionOp because
     * it might be one of the source regions and miRegionOp depends
     * on the extents of those regions being the same. Besides, this
     * way there's no checking against rectangles that will be nuked
     * due to coalescing, so we have to examine fewer rectangles.
     */
    miSetExtents(this);
    return *this;
}

LRegion &LRegion::intersect(const LRect<int> &r)
{
    if (isEmpty()){
        return *this;
    }

    if(r.isEmpty()
        || !extendCheck(m_extents, r)){
        clear();
        return *this;
    }

    /* this is fully contained in r */
    if (within(r)){
        return *this;
    }

    /* r is fully contained in this */
    if (containsInner(r)){
        *this = r;
        return *this;
    }

    if (m_numRects == 1) {
        const LRect<int> rect = rectIntersectNormalized(m_extents, r.normalized());
        *this = rect;
        return *this;
    }

    intersect0(r);
    return *this;

}

LRegion &LRegion::subtract(const std::shared_ptr<LRegion> r)
{
    if(r == nullptr){
        return *this;
    }

    if (isEmpty() || r->isEmpty()){
        return *this;
    }
    if (r->containsInner(this)){
        clear();
        return *this;
    }

    if (!extendCheck(m_extents, r->m_extents)){
        return *this;
    }

    if (this == r.get() || equalRegion(this, r.get())){
        clear();
        return *this;
    }

    LRegion t(*this);
    subtractRegion(&t, r.get(), this);
    return *this;
}

LRegion &LRegion::exor(const std::shared_ptr<LRegion> r)
{
    if(r == nullptr){
        return *this;
    }

    if (isEmpty()) {
        *this = *r;
        return *this;
    } else if (r->isEmpty()) {
        return *this;
    } else if (!extendCheck(m_extents, r->m_extents)) {
        unite(r);
        return *this;
    } else if (this == r.get() || equalRegion(this, r.get())) {
        clear();
        return *this;
    } else {
        LRegion t(*this);
        exorRegion(&t, r.get(), this);
        return *this;
    }
}

bool LRegion::contains(const LPoint<int> &p) const
{
    return pointInRegion(p.m_x, p.m_y);
}

bool LRegion::contains(const LRect<int> &r) const
{
    return rectInRegion(r.m_x, r.m_y, r.width(), r.height());
}

bool LRegion::equalRegion(const LRegion *r1, const LRegion *r2)
{
    if (r1->m_numRects != r2->m_numRects) {
        return false;
    } else if (r1->m_numRects == 0) {
        return true;
    } else if (r1->m_extents != r2->m_extents) {
        return false;
    } else if (r1->m_numRects == 1 && r2->m_numRects == 1) {
        return true; // equality tested in previous if-statement
    } else {
        const LRect<int> *rr1 = (r1->m_numRects == 1) ? &r1->m_extents : r1->m_rects.data();
        const LRect<int> *rr2 = (r2->m_numRects == 1) ? &r2->m_extents : r2->m_rects.data();
        for (int i = 0; i < r1->m_numRects; ++i, ++rr1, ++rr2) {
            if (*rr1 != *rr2){
                return false;
            }
        }
    }

    return true;
}

bool LRegion::regionStrictContains(const LRegion *region, const LRect<int> &rect)
{
    if(region == nullptr){
        return false;
    }
    if (region->isEmpty() || !rect.isValid()){
        return false;
    }
    const LRect<int> r1 = region->m_innerRect;
    return (rect.m_x >= r1.m_x && rect.x2() <= r1.x2()
            && rect.m_y >= r1.m_y && rect.y2() <= r1.y2());
}

std::shared_ptr<LPainterPath> LRegion::regionToPath(const LRegion *region)
{
    std::shared_ptr<LPainterPath> result = std::make_shared<LPainterPath>();
    if (region->rectCount() == 1) {
        result->addRect(region->boundingRect().toRectF());
        return result;
    }

    auto rect = region->begin();
    const auto end = region->end();

    std::vector<LSegment> segments;
    segments.resize(4 * (end - rect));

    int lastRowSegmentCount = 0;
    LSegment *lastRowSegments = nullptr;

    int lastSegment = 0;
    int lastY = 0;
    while (rect != end) {
        const int y = rect[0].m_y;
        int count = 0;
        while (&rect[count] != end && rect[count].m_y == y){
            ++count;
        }

        for (int i = 0; i < count; ++i) {
            int offset = lastSegment + i;
            segments[offset] = LSegment(rect[i].topLeft());
            segments[offset += count] = LSegment(rect[i].topRight() + LPoint<int>(1, 0));
            segments[offset += count] = LSegment(rect[i].bottomRight() + LPoint<int>(1, 1));
            segments[offset += count] = LSegment(rect[i].bottomLeft() + LPoint<int>(0, 1));

            offset = lastSegment + i;
            for (int j = 0; j < 4; ++j){
                segments[offset + j * count].connect(segments[offset + ((j + 1) % 4) * count]);
            }
        }

        if (lastRowSegments && lastY == y){
            mergeLSegments(lastRowSegments, lastRowSegmentCount, &segments[lastSegment], count);
        }

        lastRowSegments = &segments[lastSegment + 2 * count];
        lastRowSegmentCount = count;
        lastSegment += 4 * count;
        lastY = y + rect[0].height();
        rect += count;
    }

    for (int i = 0; i < lastSegment; ++i) {
        LSegment *segment = &segments[i];
        if (!segment->added){
            addLSegmentsToPath(segment, result);
        }
    }

    return result;

}

void LRegion::translate(int dx, int dy)
{
    if ((dx == 0 && dy == 0) || isEmpty()){
        return;
    }

    offsetRegion(this, dx, dy);
}

LRect<int> LRegion::boundingRect() const noexcept
{
    if (isEmpty()){
        return LRect<int>();
    }
    return m_extents;
}

void LRegion::setRects(const LRect<int> *rects, int num)
{
    clear();
    if (rects == nullptr || num == 0 || (num == 1 && rects->isEmpty())){
        return;
    }

    m_numRects = num;
    if (m_numRects == 1) {
        m_extents = *rects;
        m_innerRect = *rects;
        m_innerArea = m_innerRect.m_width*m_innerRect.m_height;
    } else {
        m_rects.resize(m_numRects);

        int left = INT_MAX, right = INT_MIN, top = INT_MAX,  bottom = INT_MIN;
        for (int i = 0; i < m_numRects; ++i) {
            const LRect<int> &rect = rects[i];
            m_rects[i] = rect;
            left = std::min(rect.m_x, left);
            right = std::max(rect.x2(), right);
            top = std::min(rect.m_y, top);
            bottom = std::max(rect.y2(), bottom);
            updateInnerRect(rect);
        }
        m_extents = LRect<int>(left, top, right, bottom, true);
    }
}

void LRegion::vectorize()
{
    if (m_numRects == 1) {
        if (m_rects.size()==0){
            m_rects.resize(1);
        }
        m_rects[0] = m_extents;
    }
}

bool LRegion::isIntersects(const std::shared_ptr<LRegion> region) const
{
    if(region == nullptr){
        return false;
    }

    if (isEmpty() || region->isEmpty()){
        return false;
    }

    if (!boundingRect().isIntersects(region->boundingRect())){
        return false;
    }
    if (rectCount() == 1 && region->rectCount() == 1){
        return true;
    }

    for (const LRect<int> &myRect : *this){
        for (const LRect<int> &otherRect : *region){
            if (myRect.isIntersects(otherRect)){
                return true;
            }
        }
    }
    return false;
}

bool LRegion::isIntersects(const LRect<int> rect) const
{
    if (isEmpty() || rect.isNull()){
        return false;
    }

    const LRect<int> r = rect.normalized();
    if (!m_extents.isIntersects(r)){
        return false;
    }
    if (m_numRects == 1){
        return true;
    }

    for (const LRect<int> &t : *this) {
        if (r.isIntersects(t)){
            return true;
        }
    }
    return false;
}

LK_NAMESPACE_END
