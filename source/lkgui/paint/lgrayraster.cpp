#undef  LK_FT_COMPONENT
#define LK_FT_COMPONENT  trace_smooth


/* Auxiliary macros for token concatenation. */
#define LK_FT_ERR_XCAT( x, y )  x ## y
#define LK_FT_ERR_CAT( x, y )   LK_FT_ERR_XCAT( x, y )

#define LK_FT_BEGIN_STMNT  do {
#define LK_FT_END_STMNT    } while ( 0 )

#define LK_FT_MAX( a, b )  ( (a) > (b) ? (a) : (b) )
#define LK_FT_ABS( a )     ( (a) < 0 ? -(a) : (a) )


/*
 *  Approximate sqrt(x*x+y*y) using the `alpha max plus beta min'
 *  algorithm.  We use alpha = 1, beta = 3/8, giving us results with a
 *  largest error less than 7% compared to the exact value.
 */
#define LK_FT_HYPOT( x, y )                 \
    ( x = LK_FT_ABS( x ),             \
      y = LK_FT_ABS( y ),             \
      x > y ? x + ( 3 * y >> 3 )   \
      : y + ( 3 * x >> 3 ) )

#define ErrRaster_MemoryOverflow   -4

#if defined(VXWORKS)
#  include <vxWorksCommon.h>    /* needed for setjmp.h */
#endif
#include <string.h>             /* for LK_ft_memcpy() */
#include <setjmp.h>
#include <limits.h>

#define LK_FT_UINT_MAX  UINT_MAX

#define LK_ft_memset   memset

#define LK_ft_setjmp   setjmp
#define LK_ft_longjmp  longjmp
#define LK_ft_jmp_buf  jmp_buf

#include <stddef.h>
typedef ptrdiff_t  LK_FT_PtrDist;

#define ErrRaster_Invalid_Mode      -2
#define ErrRaster_Invalid_Outline   -1
#define ErrRaster_Invalid_Argument  -3
#define ErrRaster_Memory_Overflow   -4
#define ErrRaster_OutOfMemory       -6

#define LK_FT_BEGIN_HEADER
#define LK_FT_END_HEADER

#include "lgrayraster.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

LK_NAMESPACE_BEGIN


#define LK_FT_UNUSED( x )  (void) x
#define LK_FT_THROW( e )   LK_FT_ERR_CAT( ErrRaster_, e )

#ifndef LK_FT_MEM_SET
#define LK_FT_MEM_SET( d, s, c )  LK_ft_memset( d, s, c )
#endif

#ifndef LK_FT_MEM_ZERO
#define LK_FT_MEM_ZERO( dest, count )  LK_FT_MEM_SET( dest, 0, count )
#endif


/* must be at least 6 bits! */
#define PIXEL_BITS  8

#define ONE_PIXEL       ( 1L << PIXEL_BITS )
#define TRUNC( x )      (TCoord)( (x) >> PIXEL_BITS )
#define FRACT( x )      (TCoord)( (x) & ( ONE_PIXEL - 1 ) )

/* Compute `dividend / divisor' and return both its quotient and     */
/* remainder, cast to a specific type.  This macro also ensures that */
/* the remainder is always positive.                                 */
#define LK_FT_DIV_MOD( type, dividend, divisor, quotient, remainder ) \
    LK_FT_BEGIN_STMNT                                                   \
    (quotient)  = (type)( (dividend) / (divisor) );                \
    (remainder) = (type)( (dividend) % (divisor) );                \
    if ( (remainder) < 0 )                                         \
    {                                                              \
        (quotient)--;                                              \
        (remainder) += (type)(divisor);                            \
    }                                                              \
    LK_FT_END_STMNT

/* These macros speed up repetitive divisions by replacing them */
/* with multiplications and right shifts.                       */
#define LK_FT_UDIVPREP( b )                                       \
    long  b ## _r = (long)( ULONG_MAX >> PIXEL_BITS ) / ( b )
#define LK_FT_UDIV( a, b )                                        \
      ( ( (unsigned long)( a ) * (unsigned long)( b ## _r ) ) >>   \
       ( sizeof( long ) * CHAR_BIT - PIXEL_BITS ) )


typedef long   TCoord;   /* integer scanline/pixel coordinate */
typedef long   TPos;     /* sub-pixel coordinate              */
typedef long   TArea ;   /* cell areas, coordinate products   */

/* maximal number of gray spans in a call to the span callback */
#define LK_FT_MAX_GRAY_SPANS  256


typedef struct TCell_*  PCell;

typedef struct  TCell_
{
    int    x;
    int    cover;
    TArea  area;
    PCell  next;

} TCell;


typedef struct  TWorker_
{
    TCoord  ex, ey;
    TPos    min_ex, max_ex;
    TPos    min_ey, max_ey;
    TPos    count_ex, count_ey;

    TArea   area;
    int     cover;
    int     invalid;

    PCell   cells;
    LK_FT_PtrDist     max_cells;
    LK_FT_PtrDist     num_cells;

    TPos    x,  y;

    LK_FT_Outline  outline;
    LK_FT_BBox     clip_box;

    LSpan     gray_spans[LK_FT_MAX_GRAY_SPANS];
    int       num_gray_spans;

    ProcessSpans  render_span;
    void* render_span_data;

    int  band_size;
    int  band_shoot;

    LK_ft_jmp_buf  jump_buffer;

    void*       buffer;
    long        buffer_size;

    PCell*     ycells;
    TPos       ycount;

    int        skip_spans;
} TWorker, *PWorker;


struct TRaster
{
    void*    buffer;
    long     buffer_size;
    long     buffer_allocated_size;
    int      band_size;
    void*    memory;
    PWorker  worker;
};

/*************************************************************************/
/*                                                                       */
/* Initialize the cells table.                                           */
/*                                                                       */
static void gray_init_cells( TWorker* worker, void*  buffer, long   byte_size )
{
    worker->buffer      = buffer;
    worker->buffer_size = byte_size;

    worker->ycells      = (PCell*) buffer;
    worker->cells       = NULL;
    worker->max_cells   = 0;
    worker->num_cells   = 0;
    worker->area        = 0;
    worker->cover       = 0;
    worker->invalid     = 1;
}


/*************************************************************************/
/*                                                                       */
/* Compute the outline bounding box.                                     */
/*                                                                       */
static void gray_compute_cbox( TWorker* worker )
{
    LK_FT_Outline*  outline = &(worker->outline);
    LK_FT_Vector*   vec     = outline->points;
    LK_FT_Vector*   limit   = vec + outline->n_points;


    if ( outline->n_points <= 0 )
    {
        worker->min_ex = worker->max_ex = 0;
        worker->min_ey = worker->max_ey = 0;
        return;
    }

    worker->min_ex = worker->max_ex = vec->x;
    worker->min_ey = worker->max_ey = vec->y;

    vec++;

    for ( ; vec < limit; vec++ )
    {
        TPos  x = vec->x;
        TPos  y = vec->y;


        if ( x < worker->min_ex ) worker->min_ex = x;
        if ( x > worker->max_ex ) worker->max_ex = x;
        if ( y < worker->min_ey ) worker->min_ey = y;
        if ( y > worker->max_ey ) worker->max_ey = y;
    }

    /* truncate the bounding box to integer pixels */
    worker->min_ex = worker->min_ex >> 8;
    worker->min_ey = worker->min_ey >> 8;
    worker->max_ex = ( worker->max_ex + 255 ) >> 8;
    worker->max_ey = ( worker->max_ey + 255 ) >> 8;
}


/*************************************************************************/
/*                                                                       */
/* Record the current cell in the table.                                 */
/*                                                                       */
static PCell gray_find_cell( TWorker* worker )
{
    PCell  *pcell, cell;
    TPos    x = worker->ex;

    if ( x > worker->count_ex ){
        x = worker->count_ex;
    }

    pcell = &(worker->ycells[worker->ey]);
    for (;;)
    {
        cell = *pcell;
        if ( cell == NULL || cell->x > x ){
            break;
        }

        if ( cell->x == x ){
            goto Exit;
        }
        pcell = &cell->next;
    }

    if ( worker->num_cells >= worker->max_cells ){
        LK_ft_longjmp( worker->jump_buffer, 1 );
    }

    cell        = worker->cells + worker->num_cells++;
    cell->x     = x;
    cell->area  = 0;
    cell->cover = 0;

    cell->next  = *pcell;
    *pcell      = cell;

Exit:
    return cell;
}


static void gray_record_cell( TWorker* worker )
{
    if ( worker->area | worker->cover )
    {
        PCell  cell = gray_find_cell( worker );

        cell->area  += worker->area;
        cell->cover += worker->cover;
    }
}


/*************************************************************************/
/*                                                                       */
/* Set the current cell to a new position.                               */
/*                                                                       */
static void gray_set_cell( TWorker* worker, TCoord  ex, TCoord  ey )
{
    /* Move the cell pointer to a new position.  We set the `invalid'      */
    /* flag to indicate that the cell isn't part of those we're interested */
    /* in during the render phase.  This means that:                       */
    /*                                                                     */
    /* . the new vertical position must be within min_ey..max_ey-1.        */
    /* . the new horizontal position must be strictly less than max_ex     */
    /*                                                                     */
    /* Note that if a cell is to the left of the clipping region, it is    */
    /* actually set to the (min_ex-1) horizontal position.                 */

    /* All cells that are on the left of the clipping region go to the */
    /* min_ex - 1 horizontal position.                                 */
    ey -= worker->min_ey;

    if ( ex > worker->max_ex ){
        ex = worker->max_ex;
    }

    ex -= worker->min_ex;
    if ( ex < 0 ){
        ex = -1;
    }

    /* are we moving to a different cell ? */
    if ( ex != worker->ex || ey != worker->ey )
    {
        /* record the current one if it is valid */
        if ( !worker->invalid ){
            gray_record_cell( worker );
        }

        worker->area  = 0;
        worker->cover = 0;
        worker->ex    = ex;
        worker->ey    = ey;
    }

    worker->invalid = ( (unsigned int)ey >= (unsigned int)worker->count_ey || ex >= worker->count_ex );
}


/*************************************************************************/
/*                                                                       */
/* Start a new contour at a given cell.                                  */
/*                                                                       */
static void gray_start_cell( TWorker* worker, TCoord  ex, TCoord  ey )
{
    if ( ex > worker->max_ex ){
        ex = (TCoord)( worker->max_ex );
    }

    if ( ex < worker->min_ex ){
        ex = (TCoord)( worker->min_ex - 1 );
    }

    worker->area    = 0;
    worker->cover   = 0;
    worker->ex      = ex - worker->min_ex;
    worker->ey      = ey - worker->min_ey;
    worker->invalid = 0;

    gray_set_cell( worker, ex, ey );
}


/*************************************************************************/
/*                                                                       */
/* Render a scanline as one or more cells.                               */
/*                                                                       */
static void gray_render_scanline( TWorker* worker, TCoord ey,TPos x1,TCoord  y1,TPos x2, TCoord y2 )
{
    TCoord  ex1, ex2, fx1, fx2, first, dy, delta, mod;
    TPos    p, dx;
    int     incr;


    ex1 = TRUNC( x1 );
    ex2 = TRUNC( x2 );

    /* trivial case.  Happens often */
    if ( y1 == y2 ){
        gray_set_cell( worker, ex2, ey );
        return;
    }

    fx1   = FRACT( x1 );
    fx2   = FRACT( x2 );

    /* everything is located in a single cell.  That is easy! */
    /*                                                        */
    if ( ex1 == ex2 ){
        goto End;
    }

    /* ok, we'll have to render a run of adjacent cells on the same */
    /* scanline...                                                  */
    /*                                                              */
    dx = x2 - x1;
    dy = y2 - y1;

    if ( dx > 0 ){
        p     = ( ONE_PIXEL - fx1 ) * dy;
        first = ONE_PIXEL;
        incr  = 1;
    } else {
        p     = fx1 * dy;
        first = 0;
        incr  = -1;
        dx    = -dx;
    }

    LK_FT_DIV_MOD( TCoord, p, dx, delta, mod );

    worker->area  += (TArea)( fx1 + first ) * delta;
    worker->cover += delta;
    y1        += delta;
    ex1       += incr;
    gray_set_cell( worker, ex1, ey );

    if ( ex1 != ex2 )
    {
        TCoord  lift, rem;


        p = ONE_PIXEL * dy;
        LK_FT_DIV_MOD( TCoord, p, dx, lift, rem );

        do
        {
            delta = lift;
            mod  += rem;
            if ( mod >= (TCoord)dx )
            {
                mod -= (TCoord)dx;
                delta++;
            }

            worker->area  += (TArea)( ONE_PIXEL * delta );
            worker->cover += delta;
            y1        += delta;
            ex1       += incr;
            gray_set_cell( worker, ex1, ey );
        } while ( ex1 != ex2 );
    }
    fx1 = ONE_PIXEL - first;

End:
    dy = y2 - y1;

    worker->area  += (TArea)( ( fx1 + fx2 ) * dy );
    worker->cover += dy;
}


/*************************************************************************/
/*                                                                       */
/* Render a given line as a series of scanlines.                         */
/*                                                                       */
static void gray_render_line( TWorker* worker, TPos  to_x, TPos  to_y )
{
    TCoord  ey1, ey2, fy1, fy2, first, delta, mod;
    TPos    p, dx, dy, x, x2;
    int     incr;

    ey1 = TRUNC( worker->y );
    ey2 = TRUNC( to_y );     /* if (ey2 >= worker->max_ey) ey2 = worker->max_ey-1; */

    /* perform vertical clipping */
    if ( ( ey1 >= worker->max_ey && ey2 >= worker->max_ey ) ||
        ( ey1 <  worker->min_ey && ey2 <  worker->min_ey ) )
        goto End;

    fy1 = FRACT( worker->y );
    fy2 = FRACT( to_y );

    /* everything is on a single scanline */
    if ( ey1 == ey2 )
    {
        gray_render_scanline( worker, ey1, worker->x, fy1, to_x, fy2 );
        goto End;
    }

    dx = to_x - worker->x;
    dy = to_y - worker->y;

    /* vertical line - avoid calling gray_render_scanline */
    if ( dx == 0 )
    {
        TCoord  ex     = TRUNC( worker->x );
        TCoord  two_fx = FRACT( worker->x ) << 1;
        TPos    area, max_ey1;


        if ( dy > 0)
        {
            first = ONE_PIXEL;
        }
        else
        {
            first = 0;
        }

        delta      = first - fy1;
        worker->area  += (TArea)two_fx * delta;
        worker->cover += delta;

        delta = first + first - ONE_PIXEL;
        area  = (TArea)two_fx * delta;
        max_ey1 = worker->count_ey + worker->min_ey;
        if (dy < 0) {
            if (ey1 > max_ey1) {
                ey1 = (max_ey1 > ey2) ? max_ey1 : ey2;
                gray_set_cell( &(*worker), ex, ey1 );
            } else {
                ey1--;
                gray_set_cell( &(*worker), ex, ey1 );
            }
            while ( ey1 > ey2 && ey1 >= worker->min_ey)
            {
                worker->area  += area;
                worker->cover += delta;
                ey1--;

                gray_set_cell( &(*worker), ex, ey1 );
            }
            if (ey1 != ey2) {
                ey1 = ey2;
                gray_set_cell( &(*worker), ex, ey1 );
            }
        } else {
            if (ey1 < worker->min_ey) {
                ey1 = (worker->min_ey < ey2) ? worker->min_ey : ey2;
                gray_set_cell( &(*worker), ex, ey1 );
            } else {
                ey1++;
                gray_set_cell( &(*worker), ex, ey1 );
            }
            while ( ey1 < ey2 && ey1 < max_ey1)
            {
                worker->area  += area;
                worker->cover += delta;
                ey1++;

                gray_set_cell( &(*worker), ex, ey1 );
            }
            if (ey1 != ey2) {
                ey1 = ey2;
                gray_set_cell( &(*worker), ex, ey1 );
            }
        }

        delta      = (int)( fy2 - ONE_PIXEL + first );
        worker->area  += (TArea)two_fx * delta;
        worker->cover += delta;

        goto End;
    }

    /* ok, we have to render several scanlines */
    if ( dy > 0)
    {
        p     = ( ONE_PIXEL - fy1 ) * dx;
        first = ONE_PIXEL;
        incr  = 1;
    }
    else
    {
        p     = fy1 * dx;
        first = 0;
        incr  = -1;
        dy    = -dy;
    }

    /* the fractional part of x-delta is mod/dy. It is essential to */
    /* keep track of its accumulation for accurate rendering.       */
    LK_FT_DIV_MOD( TCoord, p, dy, delta, mod );

    x = worker->x + delta;
    gray_render_scanline( worker, ey1, worker->x, fy1, x, (TCoord)first );

    ey1 += incr;
    gray_set_cell( worker, TRUNC( x ), ey1 );

    if ( ey1 != ey2 )
    {
        TCoord  lift, rem;


        p    = ONE_PIXEL * dx;
        LK_FT_DIV_MOD( TCoord, p, dy, lift, rem );

        do
        {
            delta = lift;
            mod  += rem;
            if ( mod >= (TCoord)dy )
            {
                mod -= (TCoord)dy;
                delta++;
            }

            x2 = x + delta;
            gray_render_scanline( worker, ey1,
                                 x, ONE_PIXEL - first,
                                 x2, first );
            x = x2;

            ey1 += incr;
            gray_set_cell( worker, TRUNC( x ), ey1 );
        } while ( ey1 != ey2 );
    }

    gray_render_scanline( worker, ey1,
                         x, ONE_PIXEL - first,
                         to_x, fy2 );

End:
    worker->x       = to_x;
    worker->y       = to_y;
}


static void gray_split_conic( LK_FT_Vector*  base )
{
    TPos  a, b;


    base[4].x = base[2].x;
    b = base[1].x;
    a = base[3].x = ( base[2].x + b ) / 2;
    b = base[1].x = ( base[0].x + b ) / 2;
    base[2].x = ( a + b ) / 2;

    base[4].y = base[2].y;
    b = base[1].y;
    a = base[3].y = ( base[2].y + b ) / 2;
    b = base[1].y = ( base[0].y + b ) / 2;
    base[2].y = ( a + b ) / 2;
}


static void gray_render_conic( TWorker* worker, const LK_FT_Vector*  control, const LK_FT_Vector*  to )
{
    LK_FT_Vector   bez_stack[16 * 2 + 1];  /* enough to accommodate bisections */
    LK_FT_Vector*  arc = bez_stack;
    TPos        dx, dy;
    int         draw, split;


    arc[0].x = to->x;
    arc[0].y = to->y;
    arc[1].x = control->x;
    arc[1].y = control->y;
    arc[2].x = worker->x;
    arc[2].y = worker->y;

    /* short-cut the arc that crosses the current band */
    if ( ( TRUNC( arc[0].y ) >= worker->max_ey &&
         TRUNC( arc[1].y ) >= worker->max_ey &&
         TRUNC( arc[2].y ) >= worker->max_ey ) ||
        ( TRUNC( arc[0].y ) <  worker->min_ey &&
         TRUNC( arc[1].y ) <  worker->min_ey &&
         TRUNC( arc[2].y ) <  worker->min_ey ) )
    {
        worker->x = arc[0].x;
        worker->y = arc[0].y;
        return;
    }

    dx = LK_FT_ABS( arc[2].x + arc[0].x - 2 * arc[1].x );
    dy = LK_FT_ABS( arc[2].y + arc[0].y - 2 * arc[1].y );
    if ( dx < dy ){
        dx = dy;
    }
    /* We can calculate the number of necessary bisections because  */
    /* each bisection predictably reduces deviation exactly 4-fold. */
    /* Even 32-bit deviation would vanish after 16 bisections.      */
    draw = 1;
    while ( dx > ONE_PIXEL / 4 )
    {
        dx >>= 2;
        draw <<= 1;
    }

    /* We use decrement counter to count the total number of segments */
    /* to draw starting from 2^level. Before each draw we split as    */
    /* many times as there are trailing zeros in the counter.         */
    do
    {
        split = 1;
        while ( ( draw & split ) == 0 )
        {
            gray_split_conic( arc );
            arc += 2;
            split <<= 1;
        }

        gray_render_line( worker, arc[0].x, arc[0].y );
        arc -= 2;

    } while ( --draw );
}


static void gray_split_cubic( LK_FT_Vector*  base )
{
    TPos  a, b, c, d;


    base[6].x = base[3].x;
    c = base[1].x;
    d = base[2].x;
    base[1].x = a = ( base[0].x + c ) / 2;
    base[5].x = b = ( base[3].x + d ) / 2;
    c = ( c + d ) / 2;
    base[2].x = a = ( a + c ) / 2;
    base[4].x = b = ( b + c ) / 2;
    base[3].x = ( a + b ) / 2;

    base[6].y = base[3].y;
    c = base[1].y;
    d = base[2].y;
    base[1].y = a = ( base[0].y + c ) / 2;
    base[5].y = b = ( base[3].y + d ) / 2;
    c = ( c + d ) / 2;
    base[2].y = a = ( a + c ) / 2;
    base[4].y = b = ( b + c ) / 2;
    base[3].y = ( a + b ) / 2;
}


static void gray_render_cubic( TWorker* worker, const LK_FT_Vector*  control1, const LK_FT_Vector*  control2,  const LK_FT_Vector*  to )
{
    LK_FT_Vector   bez_stack[16 * 3 + 1];  /* enough to accommodate bisections */
    LK_FT_Vector*  arc = bez_stack;
    TPos        dx, dy, dx_, dy_;
    TPos        dx1, dy1, dx2, dy2;
    TPos        L, s, s_limit;


    arc[0].x = to->x;
    arc[0].y = to->y;
    arc[1].x = control2->x;
    arc[1].y = control2->y;
    arc[2].x = control1->x;
    arc[2].y = control1->y;
    arc[3].x = worker->x;
    arc[3].y = worker->y;

    /* short-cut the arc that crosses the current band */
    if ( ( TRUNC( arc[0].y ) >= worker->max_ey &&
         TRUNC( arc[1].y ) >= worker->max_ey &&
         TRUNC( arc[2].y ) >= worker->max_ey &&
         TRUNC( arc[3].y ) >= worker->max_ey ) ||
        ( TRUNC( arc[0].y ) <  worker->min_ey &&
         TRUNC( arc[1].y ) <  worker->min_ey &&
         TRUNC( arc[2].y ) <  worker->min_ey &&
         TRUNC( arc[3].y ) <  worker->min_ey ) )
    {
        worker->x = arc[0].x;
        worker->y = arc[0].y;
        return;
    }

    for (;;)
    {
        /* Decide whether to split or draw. See `Rapid Termination          */
        /* Evaluation for Recursive Subdivision of Bezier Curves' by Thomas */
        /* F. Hain, at                                                      */
        /* http://www.cis.southalabama.edu/~hain/general/Publications/Bezier/Camera-ready%20CISST02%202.pdf */


        /* dx and dy are x and y components of the P0-P3 chord vector. */
        dx = dx_ = arc[3].x - arc[0].x;
        dy = dy_ = arc[3].y - arc[0].y;

        L = LK_FT_HYPOT( dx_, dy_ );

        /* Avoid possible arithmetic overflow below by splitting. */
        if ( L >= (1 << 23) )
            goto Split;

        /* Max deviation may be as much as (s/L) * 3/4 (if Hain's v = 1). */
        s_limit = L * (TPos)( ONE_PIXEL / 6 );

        /* s is L * the perpendicular distance from P1 to the line P0-P3. */
        dx1 = arc[1].x - arc[0].x;
        dy1 = arc[1].y - arc[0].y;
        s = LK_FT_ABS( dy * dx1 - dx * dy1 );

        if ( s > s_limit )
            goto Split;

        /* s is L * the perpendicular distance from P2 to the line P0-P3. */
        dx2 = arc[2].x - arc[0].x;
        dy2 = arc[2].y - arc[0].y;
        s = LK_FT_ABS( dy * dx2 - dx * dy2 );

        if ( s > s_limit )
            goto Split;

        /* Split super curvy segments where the off points are so far
     from the chord that the angles P0-P1-P3 or P0-P2-P3 become
     acute as detected by appropriate dot products. */
        if ( dx1 * ( dx1 - dx ) + dy1 * ( dy1 - dy ) > 0 ||
            dx2 * ( dx2 - dx ) + dy2 * ( dy2 - dy ) > 0 )
            goto Split;

        gray_render_line( worker, arc[0].x, arc[0].y );

        if ( arc == bez_stack )
            return;

        arc -= 3;
        continue;

    Split:
        gray_split_cubic( arc );
        arc += 3;
    }
}



static int gray_move_to( const LK_FT_Vector*  to, PWorker  worker )
{
    TPos  x, y;
    /* record current cell, if any */
    if ( !(worker->invalid) ){
        gray_record_cell( worker );
    }

    /* start to a new position */
    x = to->x;
    y = to->y;

    gray_start_cell( worker, TRUNC( x ), TRUNC( y ) );
    worker->x = x;
    worker->y = y;
    return 0;
}


static void gray_hline( TWorker* worker, TCoord  x,TCoord  y,TPos area,int acount )
{
    /* compute the coverage line's coverage, depending on the    */
    /* outline fill rule                                         */
    /*                                                           */
    /* the coverage percentage is area/(PIXEL_BITS*PIXEL_BITS*2) */
    /*                                                           */
    int coverage = (int)( area >> ( PIXEL_BITS * 2 + 1 - 8 ) );
    /* use range 0..256 */
    if ( coverage < 0 ){
        coverage = -coverage;
    }

    if ( worker->outline.flags & LK_FT_OUTLINE_EVEN_ODD_FILL )
    {
        coverage &= 511;

        if ( coverage > 256 ){
            coverage = 512 - coverage;
        }else if ( coverage == 256 ){
            coverage = 255;
        }
    }
    else
    {
        /* normal non-zero winding rule */
        if ( coverage >= 256 ){
            coverage = 255;
        }
    }

    y += (TCoord)worker->min_ey;
    x += (TCoord)worker->min_ex;

    /* LSpan.x is an int, so limit our coordinates appropriately */
    if ( x >= (1 << 23) ){
        x = (1 << 23) - 1;
    }

    /* LSpan.y is an int, so limit our coordinates appropriately */
    if ( y >= (1 << 23) ){
        y = (1 << 23) - 1;
    }

    if ( coverage )
    {
        LSpan*  span;
        int       count;
        int       skip;


        /* see whether we can add this span to the current list */
        count = worker->num_gray_spans;
        span  = worker->gray_spans + count - 1;
        if ( count > 0                          &&
            span->y == y                       &&
            span->x + span->len == x           &&
            span->coverage == coverage         )
        {
            span->len = span->len + acount;
            return;
        }

        if ( count >= LK_FT_MAX_GRAY_SPANS )
        {
            if ( worker->render_span && count > worker->skip_spans )
            {
                skip = worker->skip_spans > 0 ? worker->skip_spans : 0;
                worker->render_span( worker->num_gray_spans - skip,
                                worker->gray_spans + skip,
                                worker->render_span_data );
            }

            worker->skip_spans -= worker->num_gray_spans;
            worker->num_gray_spans = 0;

            span  = worker->gray_spans;
        }
        else{
            span++;
        }

        /* add a gray span to the current list */
        span->x        = x;
        span->len      = acount;
        span->y        = y;
        span->coverage = (unsigned char)coverage;

        worker->num_gray_spans++;
    }
}


static void gray_sweep( TWorker* worker)
{
    int  yindex;
    if ( worker->num_cells == 0 ){
        return;
    }

    for ( yindex = 0; yindex < worker->ycount; yindex++ )
    {
        PCell   cell  = worker->ycells[yindex];
        TCoord  cover = 0;
        TCoord  x     = 0;


        for ( ; cell != NULL; cell = cell->next )
        {
            TArea  area;


            if ( cell->x > x && cover != 0 ){
                gray_hline( worker, x, yindex, cover * ( ONE_PIXEL * 2 ), cell->x - x );
            }

            cover += cell->cover;
            area   = cover * ( ONE_PIXEL * 2 ) - cell->area;

            if ( area != 0 && cell->x >= 0 ){
                gray_hline( worker, cell->x, yindex, area, 1 );
            }

            x = cell->x + 1;
        }

        if ( worker->count_ex > x && cover != 0 ){
            gray_hline( worker, x, yindex, cover * ( ONE_PIXEL * 2 ),worker->count_ex - x );
        }
    }
}

/*************************************************************************/
/*                                                                       */
/*  The following function should only compile in stand_alone mode,      */
/*  i.e., when building this component without the rest of FreeType.     */
/*                                                                       */
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/* <Function>                                                            */
/*    LK_FT_Outline_Decompose                                               */
/*                                                                       */
/* <Description>                                                         */
/*    Walks over an outline's structure to decompose it into individual  */
/*    segments and Bezier arcs.  This function is also able to emit      */
/*    `move to' and `close to' operations to indicate the start and end  */
/*    of new contours in the outline.                                    */
/*                                                                       */
/* <Input>                                                               */
/*    outline        :: A pointer to the source target.                  */
/*                                                                       */
/*    user           :: A typeless pointer which is passed to each       */
/*                      emitter during the decomposition.  It can be     */
/*                      used to store the state during the               */
/*                      decomposition.                                   */
/*                                                                       */
/* <Return>                                                              */
/*    Error code.  0 means success.                                      */
/*                                                                       */
static int  LK_FT_Outline_Decompose( const LK_FT_Outline*  outline, TWorker* worker )
{
    LK_FT_Vector   v_last;
    LK_FT_Vector   v_control;
    LK_FT_Vector   v_start;

    LK_FT_Vector*  point;
    LK_FT_Vector*  limit;
    char*       tags;

    int   n;         /* index of contour in outline     */
    int   first;     /* index of first point in contour */
    int   error;
    char  tag;       /* current point's state           */

    if ( !outline ){
        return ErrRaster_Invalid_Outline;
    }

    first = 0;

    for ( n = 0; n < outline->n_contours; n++ )
    {
        int  last;  /* index of last point in contour */


        last  = outline->contours[n];
        if ( last < 0 ){
            goto Invalid_Outline;
        }
        limit = outline->points + last;

        v_start   = outline->points[first];
        v_last   = outline->points[last];
        v_control = v_start;

        point = outline->points + first;
        tags  = outline->tags  + first;
        tag   = LK_FT_CURVE_TAG( tags[0] );

        /* A contour cannot start with a cubic control point! */
        if ( tag == LK_FT_CURVE_TAG_CUBIC ){
            goto Invalid_Outline;
        }

        /* check first point to determine origin */
        if ( tag == LK_FT_CURVE_TAG_CONIC )
        {
            /* first point is conic control.  Yes, this happens. */
            if ( LK_FT_CURVE_TAG( outline->tags[last] ) == LK_FT_CURVE_TAG_ON )
            {
                /* start at last point if it is on the curve */
                v_start = v_last;
                limit--;
            }
            else
            {
                /* if both first and last points are conic,         */
                /* start at their middle and record its position    */
                /* for closure                                      */
                v_start.x = ( v_start.x + v_last.x ) / 2;
                v_start.y = ( v_start.y + v_last.y ) / 2;

                v_last = v_start;
            }
            point--;
            tags--;
        }

        error = gray_move_to( &v_start, worker );
        if ( error ){
            goto Exit;
        }

        while ( point < limit )
        {
            point++;
            tags++;

            tag = LK_FT_CURVE_TAG( tags[0] );
            switch ( tag )
            {
            case LK_FT_CURVE_TAG_ON:  /* emit a single line_to */
            {
                gray_render_line(worker, point->x, point->y);
                continue;
            }
            case LK_FT_CURVE_TAG_CONIC:  /* consume conic arcs */
            {
                v_control.x = point->x;
                v_control.y = point->y;

            Do_Conic:
                if ( point < limit )
                {
                    LK_FT_Vector  vec;
                    LK_FT_Vector  v_middle;
                    point++;
                    tags++;
                    tag = LK_FT_CURVE_TAG( tags[0] );

                    vec.x = point->x;
                    vec.y = point->y;

                    if ( tag == LK_FT_CURVE_TAG_ON )
                    {
                        gray_render_conic(worker, &v_control, &vec);
                        continue;
                    }

                    if ( tag != LK_FT_CURVE_TAG_CONIC ){
                        goto Invalid_Outline;
                    }

                    v_middle.x = ( v_control.x + vec.x ) / 2;
                    v_middle.y = ( v_control.y + vec.y ) / 2;


                    gray_render_conic(worker, &v_control, &v_middle);
                    v_control = vec;
                    goto Do_Conic;
                }

                gray_render_conic(worker, &v_control, &v_start);
                goto Close;
            }

            default:  /* LK_FT_CURVE_TAG_CUBIC */
            {
                LK_FT_Vector  vec1, vec2;
                if ( ((point + 1) > limit) || (LK_FT_CURVE_TAG( tags[1] ) != LK_FT_CURVE_TAG_CUBIC)){
                    goto Invalid_Outline;
                }

                point += 2;
                tags  += 2;

                vec1.x = point[-2].x;
                vec1.y = point[-2].y;

                vec2.x = point[-1].x;
                vec2.y = point[-1].y;

                if ( point <= limit )
                {
                    gray_render_cubic(worker, &vec1, &vec2, point);
                    continue;
                }

                gray_render_cubic(worker, &vec1, &vec2, &v_start);
                goto Close;
            }
            }
        }
        gray_render_line(worker, v_start.x, v_start.y);

    Close:
        first = last + 1;
    }

    return 0;

Exit:
    return error;

Invalid_Outline:
    return ErrRaster_Invalid_Outline;
}

typedef struct  TBand_
{
    TPos  min, max;

} TBand;

static int gray_convert_glyph_inner( TWorker* worker )
{
    volatile int  error = 0;

    if ( LK_ft_setjmp( worker->jump_buffer ) == 0 )
    {
        error = LK_FT_Outline_Decompose( &(worker->outline), worker);
        if ( !(worker->invalid) ){
            gray_record_cell( worker );
        }
    }
    else
    {
        error = ErrRaster_Memory_Overflow;
    }
    return error;
}


static int gray_convert_glyph( TWorker* worker )
{
    TBand            bands[40];
    TBand* volatile  band;
    int volatile     n, num_bands;
    TPos volatile    min, max, max_y;
    LK_FT_BBox*      clip;
    int              skip;

    worker->num_gray_spans = 0;

    /* Set up state in the raster object */
    gray_compute_cbox( worker );

    /* clip to target bitmap, exit if nothing to do */
    clip = &worker->clip_box;

    if ( worker->max_ex <= clip->xMin || worker->min_ex >= clip->xMax ||
        worker->max_ey <= clip->yMin || worker->min_ey >= clip->yMax ){
        return 0;
    }

    if ( worker->min_ex < clip->xMin ) worker->min_ex = clip->xMin;
    if ( worker->min_ey < clip->yMin ) worker->min_ey = clip->yMin;

    if ( worker->max_ex > clip->xMax ) worker->max_ex = clip->xMax;
    if ( worker->max_ey > clip->yMax ) worker->max_ey = clip->yMax;

    worker->count_ex = worker->max_ex - worker->min_ex;
    worker->count_ey = worker->max_ey - worker->min_ey;

    /* set up vertical bands */
    num_bands = (int)( ( worker->max_ey - worker->min_ey ) / worker->band_size );
    if ( num_bands == 0 )
        num_bands = 1;
    if ( num_bands >= 39 )
        num_bands = 39;

    worker->band_shoot = 0;

    min   = worker->min_ey;
    max_y = worker->max_ey;

    for ( n = 0; n < num_bands; n++, min = max )
    {
        max = min + worker->band_size;
        if ( n == num_bands - 1 || max > max_y )
            max = max_y;

        bands[0].min = min;
        bands[0].max = max;
        band         = bands;

        while ( band >= bands )
        {
            TPos  bottom, top, middle;
            int   error;

            {
                PCell  cells_max;
                int    yindex;
                int    cell_start, cell_end, cell_mod;


                worker->ycells = (PCell*)worker->buffer;
                worker->ycount = band->max - band->min;

                cell_start = sizeof ( PCell ) * worker->ycount;
                cell_mod   = cell_start % sizeof ( TCell );
                if ( cell_mod > 0 ){
                    cell_start += sizeof ( TCell ) - cell_mod;
                }

                cell_end  = worker->buffer_size;
                cell_end -= cell_end % sizeof( TCell );

                cells_max = (PCell)( (char*)worker->buffer + cell_end );
                worker->cells = (PCell)( (char*)worker->buffer + cell_start );
                if ( worker->cells >= cells_max ){
                    goto ReduceBands;
                }

                worker->max_cells = (int)(cells_max - worker->cells);
                if ( worker->max_cells < 2 ){
                    goto ReduceBands;
                }

                for ( yindex = 0; yindex < worker->ycount; yindex++ ){
                    worker->ycells[yindex] = NULL;
                }
            }

            worker->num_cells = 0;
            worker->invalid   = 1;
            worker->min_ey    = band->min;
            worker->max_ey    = band->max;
            worker->count_ey  = band->max - band->min;

            error = gray_convert_glyph_inner( worker );

            if ( !error )
            {
                gray_sweep( worker);
                band--;
                continue;
            }
            else if ( error != ErrRaster_Memory_Overflow )
                return 1;

        ReduceBands:
            /* render pool overflow; we will reduce the render band by half */
            bottom = band->min;
            top    = band->max;
            middle = bottom + ( ( top - bottom ) >> 1 );

            /* This is too complex for a single scanline; there must */
            /* be some problems.                                     */
            if ( middle == bottom )
            {
                return ErrRaster_OutOfMemory;
            }

            if ( bottom-top >= worker->band_size )
                worker->band_shoot++;

            band[1].min = bottom;
            band[1].max = middle;
            band[0].min = middle;
            band[0].max = top;
            band++;
        }
    }

    if ( worker->render_span && worker->num_gray_spans > worker->skip_spans )
    {
        skip = worker->skip_spans > 0 ? worker->skip_spans : 0;
        worker->render_span( worker->num_gray_spans - skip,
                        worker->gray_spans + skip,
                        worker->render_span_data );
    }

    worker->skip_spans -= worker->num_gray_spans;

    if ( worker->band_shoot > 8 && worker->band_size > 16 ){
        worker->band_size = worker->band_size / 2;
    }

    return 0;
}


LGrayRaster::LGrayRaster(){
    tRaster = (TRaster*)malloc(sizeof(TRaster));
}

LGrayRaster::~LGrayRaster(){
    if (tRaster ==nullptr) {
        free(tRaster);
        tRaster = nullptr;
    }
}

void LGrayRaster::init()
{
    if (tRaster !=nullptr) {
        std::memset(tRaster, 0, sizeof(TRaster));
    }
}

void LGrayRaster::reset(uint8_t*  poolBase, uint32_t poolSize ){
    if (tRaster !=nullptr) {
        if ( poolBase && ( poolSize >= MINIMUM_FT_POOL_SIZE ) )
        {
            PWorker  worker = (PWorker)poolBase;
            tRaster->worker      = worker;
            tRaster->buffer      = poolBase +
                           ( ( sizeof ( TWorker ) + sizeof ( TCell ) - 1 ) &
                            ~( sizeof ( TCell ) - 1 ) );
            tRaster->buffer_size = (long)( ( poolBase + poolSize )-(uint8_t*)tRaster->buffer ) & ~( sizeof ( TCell ) - 1 );
            tRaster->band_size   = (int)( tRaster->buffer_size /( sizeof ( TCell ) * 8 ) );
        }
        else if ( poolBase)
        { /* Case when there is a raster pool allocated, but it                */
          /* doesn't have the minimum size (and so memory will be reallocated) */
            tRaster->buffer = poolBase;
            tRaster->worker = NULL;
            tRaster->buffer_size = poolSize;
        }
        else
        {
            tRaster->buffer      = NULL;
            tRaster->buffer_size = 0;
            tRaster->worker      = NULL;
        }
        tRaster->buffer_allocated_size = poolSize;
    }
}


int LGrayRaster::render(LK_FT_Raster_Params*  params ){


    if ( tRaster==nullptr || !tRaster->buffer || !tRaster->buffer_size ){
        return ErrRaster_Invalid_Argument;
    }

    /* Should always be non-null, it is set by raster_reset() which is always */
    /* called with a non-null pool, and a pool_size >= MINIMUM_POOL_SIZE.     */
    assert(tRaster->worker);

    tRaster->worker->skip_spans = params->skip_spans;

    /* If raster object and raster buffer are allocated, but  */
    /* raster size isn't of the minimum size, indicate out of */
    /* memory.                                                */
    if (tRaster->buffer_allocated_size < MINIMUM_FT_POOL_SIZE ){
        return ErrRaster_OutOfMemory;
    }

    const LK_FT_Outline*  outline    = (const LK_FT_Outline*)params->source;
    if ( !outline ){
        return ErrRaster_Invalid_Outline;
    }

    /* return immediately if the outline is empty */
    if ( outline->n_points == 0 || outline->n_contours <= 0 ){
        return 0;
    }

    if ( !outline->contours || !outline->points ){
        return ErrRaster_Invalid_Outline;
    }

    if ( outline->n_points != outline->contours[outline->n_contours - 1] + 1 ){
        return ErrRaster_Invalid_Outline;
    }

    PWorker worker = tRaster->worker;

    /* this version does not support monochrome rendering */
    if ( !( params->flags & LK_FT_RASTER_FLAG_AA ) ){
        return ErrRaster_Invalid_Mode;
    }

    if ( params->flags & LK_FT_RASTER_FLAG_CLIP )
    {
        worker->clip_box = params->clip_box;
    }
    else
    {
        worker->clip_box.xMin = -(1 << 23);
        worker->clip_box.yMin = -(1 << 23);
        worker->clip_box.xMax =  (1 << 23) - 1;
        worker->clip_box.yMax =  (1 << 23) - 1;
    }

    gray_init_cells( worker, tRaster->buffer, tRaster->buffer_size );

    worker->outline   = *outline;
    worker->num_cells = 0;
    worker->invalid   = 1;
    worker->band_size = tRaster->band_size;

    worker->render_span      = (ProcessSpans)params->gray_spans;
    worker->render_span_data = params->user;
    return gray_convert_glyph( worker );
}

int LGrayRaster::renderedSpans()
{
    if ((tRaster !=nullptr) && (tRaster->worker != nullptr)) {
        return tRaster->worker->skip_spans > 0 ? 0 : -tRaster->worker->skip_spans;
    }
    return 0;
}

LK_NAMESPACE_END
/* END */
