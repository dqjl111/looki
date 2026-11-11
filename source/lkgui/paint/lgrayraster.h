#ifndef LGRAYRASTER_H
#define LGRAYRASTER_H

#include "lkgui/paint/ldrawhelper.h"
#include "lkcore/3rd/fpmfixed.hpp"

LK_NAMESPACE_BEGIN

typedef signed int  LK_FT_Pos;
typedef struct  LK_FT_Vector_
{
LK_FT_Pos  x;
LK_FT_Pos  y;
} LK_FT_Vector;

typedef struct  LK_FT_BBox_
{
LK_FT_Pos  xMin, yMin;
LK_FT_Pos  xMax, yMax;
} LK_FT_BBox;


typedef enum  LK_FT_Pixel_Mode_
{
LK_FT_PIXEL_MODE_NONE = 0,
LK_FT_PIXEL_MODE_MONO,
LK_FT_PIXEL_MODE_GRAY,
LK_FT_PIXEL_MODE_GRAY2,
LK_FT_PIXEL_MODE_GRAY4,
LK_FT_PIXEL_MODE_LCD,
LK_FT_PIXEL_MODE_LCD_V,
LK_FT_PIXEL_MODE_MAX      /* do not remove */

} LK_FT_Pixel_Mode;

#define LK_ft_pixel_mode_none   LK_FT_PIXEL_MODE_NONE
#define LK_ft_pixel_mode_mono   LK_FT_PIXEL_MODE_MONO
#define LK_ft_pixel_mode_grays  LK_FT_PIXEL_MODE_GRAY
#define LK_ft_pixel_mode_pal2   LK_FT_PIXEL_MODE_GRAY2
#define LK_ft_pixel_mode_pal4   LK_FT_PIXEL_MODE_GRAY4



typedef struct  LK_FT_Outline_
{
    int n_contours;      /* number of contours in glyph        */
    int n_points;        /* number of points in the glyph      */
    LK_FT_Vector* points;/* the outline's points with 24d8     */
    char* tags;          /* the points flags                   */
    int* contours;       /* the contour end points             */
    int flags;           /* outline masks                      */
} LK_FT_Outline;


#define LK_FT_OUTLINE_NONE             0x0
#define LK_FT_OUTLINE_OWNER            0x1
#define LK_FT_OUTLINE_EVEN_ODD_FILL    0x2
#define LK_FT_OUTLINE_REVERSE_FILL     0x4
#define LK_FT_OUTLINE_IGNORE_DROPOUTS  0x8

#define LK_FT_OUTLINE_HIGH_PRECISION   0x100
#define LK_FT_OUTLINE_SINGLE_PASS      0x200

#define LK_ft_outline_none             LK_FT_OUTLINE_NONE
#define LK_ft_outline_owner            LK_FT_OUTLINE_OWNER
#define LK_ft_outline_even_odd_fill    LK_FT_OUTLINE_EVEN_ODD_FILL
#define LK_ft_outline_reverse_fill     LK_FT_OUTLINE_REVERSE_FILL
#define LK_ft_outline_ignore_dropouts  LK_FT_OUTLINE_IGNORE_DROPOUTS
#define LK_ft_outline_high_precision   LK_FT_OUTLINE_HIGH_PRECISION
#define LK_ft_outline_single_pass      LK_FT_OUTLINE_SINGLE_PASS

  /* */

#define LK_FT_CURVE_TAG( flag )  ( flag & 3 )

#define LK_FT_CURVE_TAG_ON           1
#define LK_FT_CURVE_TAG_CONIC        0
#define LK_FT_CURVE_TAG_CUBIC        2

#define LK_FT_CURVE_TAG_TOUCH_X      8  /* reserved for the TrueType hinter */
#define LK_FT_CURVE_TAG_TOUCH_Y     16  /* reserved for the TrueType hinter */

#define LK_FT_CURVE_TAG_TOUCH_BOTH  ( LK_FT_CURVE_TAG_TOUCH_X | \
                                   LK_FT_CURVE_TAG_TOUCH_Y )

#define  LK_FT_Curve_Tag_On       LK_FT_CURVE_TAG_ON
#define  LK_FT_Curve_Tag_Conic    LK_FT_CURVE_TAG_CONIC
#define  LK_FT_Curve_Tag_Cubic    LK_FT_CURVE_TAG_CUBIC
#define  LK_FT_Curve_Tag_Touch_X  LK_FT_CURVE_TAG_TOUCH_X
#define  LK_FT_Curve_Tag_Touch_Y  LK_FT_CURVE_TAG_TOUCH_Y


typedef int
(*LK_FT_Outline_MoveToFunc)( LK_FT_Vector*  to,
                        void*       user );

#define LK_FT_Outline_MoveTo_Func  LK_FT_Outline_MoveToFunc


typedef int
(*LK_FT_Outline_LineToFunc)( LK_FT_Vector*  to,
                        void*       user );

#define  LK_FT_Outline_LineTo_Func  LK_FT_Outline_LineToFunc


typedef int
(*LK_FT_Outline_ConicToFunc)( LK_FT_Vector*  control,
                         LK_FT_Vector*  to,
                         void*       user );

#define  LK_FT_Outline_ConicTo_Func  LK_FT_Outline_ConicToFunc


typedef int
(*LK_FT_Outline_CubicToFunc)( LK_FT_Vector*  control1,
                         LK_FT_Vector*  control2,
                         LK_FT_Vector*  to,
                         void*       user );

#define  LK_FT_Outline_CubicTo_Func  LK_FT_Outline_CubicToFunc

typedef struct  LK_FT_Outline_Funcs_
{
LK_FT_Outline_MoveToFunc move_to;
LK_FT_Outline_LineToFunc line_to;
LK_FT_Outline_ConicToFunc  conic_to;
LK_FT_Outline_CubicToFunc  cubic_to;
int shift;
LK_FT_Pos delta;
}LK_FT_Outline_Funcs;



#define LK_FT_RASTER_FLAG_DEFAULT  0x0
#define LK_FT_RASTER_FLAG_AA       0x1
#define LK_FT_RASTER_FLAG_CLIP     0x2

  /* deprecated */
#define LK_ft_raster_flag_default  LK_FT_RASTER_FLAG_DEFAULT
#define LK_ft_raster_flag_aa       LK_FT_RASTER_FLAG_AA
#define LK_ft_raster_flag_clip     LK_FT_RASTER_FLAG_CLIP


typedef struct  LK_FT_Raster_Params_
{
void*                   source;
int                     flags;
ProcessSpans            gray_spans;
void*                   user;
LK_FT_BBox              clip_box;
int                     skip_spans;
} LK_FT_Raster_Params;


/* Minimum buffer size for raster object, that accounts
   for TWorker and TCell sizes.*/
#define MINIMUM_FT_POOL_SIZE 8192

struct TRaster;
class LGrayRaster{
public:
    LGrayRaster();
    ~LGrayRaster();


    void init();

    void reset(uint8_t* poolBase, uint32_t poolSize );
    int render(LK_FT_Raster_Params*  params );
    int renderedSpans();
private:
    TRaster*  tRaster = nullptr;
};


LK_NAMESPACE_END
#endif /* LGRAYRASTER_H */

/* END */
