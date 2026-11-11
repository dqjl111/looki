#include "limage.h"
#include "lkgui/paint/ldrawhelper.h"
LK_NAMESPACE_BEGIN

struct LImageScaleInfo {
    int *xpoints{nullptr};
    const uint32_t **ypoints{nullptr};
    int *xapoints{nullptr};
    int *yapoints{nullptr};
    int xup_yup{0};
    int sh = 0;
    int sw = 0;

    ~LImageScaleInfo(){
        if (xpoints) {
            delete[] xpoints;
            xpoints = nullptr;
        }

        if(ypoints){
            delete[] ypoints;
            ypoints = nullptr;
        }

        if(xapoints){
            delete[] xapoints;
            xapoints = nullptr;
        }

        if(yapoints){
            delete[] yapoints;
            yapoints = nullptr;
        }
    }
};


static const unsigned int** lImageCalcYPoints(const unsigned int *src, int sw, int sh, int dh){
    const unsigned int **p;
    int j = 0, rv = 0;
    int64_t val, inc;

    if (dh < 0) {
        dh = -dh;
        rv = 1;
    }
    p = new const unsigned int* [dh+1];

    int up = std::abs(dh) >= sh;
    val = up ? 0x8000 * sh / dh - 0x8000 : 0;
    inc = (((int64_t)sh) << 16) / dh;
    for (int i = 0; i < dh; i++) {
        p[j++] = src + std::max(0LL, val >> 16) * sw;
        val += inc;
    }
    if (rv) {
        for (int i = dh / 2; --i >= 0; ) {
            const unsigned int *tmp = p[i];
            p[i] = p[dh - i - 1];
            p[dh - i - 1] = tmp;
        }
    }
    return(p);
}
static int* lImageCalcXPoints(int sw, int dw){
    int *p, j = 0, rv = 0;
    int64_t val, inc;

    if (dw < 0) {
        dw = -dw;
        rv = 1;
    }
    p = new int[dw+1];

    int up = std::abs(dw) >= sw;
    val = up ? 0x8000 * sw / dw - 0x8000 : 0;
    inc = (((int64_t)sw) << 16) / dw;
    for (int i = 0; i < dw; i++) {
        p[j++] = std::max(0LL, val >> 16);
        val += inc;
    }

    if (rv) {
        for (int i = dw / 2; --i >= 0; ) {
            int tmp = p[i];
            p[i] = p[dw - i - 1];
            p[dw - i - 1] = tmp;
        }
    }
    return p;
}
static int* lImageCalcApoints(int s, int d, int up){
    int *p, j = 0, rv = 0;

    if (d < 0) {
        rv = 1;
        d = -d;
    }
    p = new int[d];

    if (up) {
        /* scaling up */
        int64_t val = 0x8000 * s / d - 0x8000;
        int64_t inc = (((int64_t)s) << 16) / d;
        for (int i = 0; i < d; i++) {
            int pos = val >> 16;
            if (pos < 0){
                p[j++] = 0;
            }
            else if (pos >= (s - 1)){
                p[j++] = 0;
            }
            else{
                p[j++] = (val >> 8) - ((val >> 8) & 0xffffff00);
            }
            val += inc;
        }
    } else {
        /* scaling down */
        int64_t val = 0;
        int64_t inc = (((int64_t)s) << 16) / d;
        int Cp = (((d << 14) + s - 1) / s);
        for (int i = 0; i < d; i++) {
            int ap = ((0x10000 - (val & 0xffff)) * Cp) >> 16;
            p[j] = ap | (Cp << 16);
            j++;
            val += inc;
        }
    }
    if (rv) {
        int tmp;
        for (int i = d / 2; --i >= 0; ) {
            tmp = p[i];
            p[i] = p[d - i - 1];
            p[d - i - 1] = tmp;
        }
    }
    return p;
}

static std::shared_ptr<LImageScaleInfo> lImageCalcScaleInfo(const std::shared_ptr<LImage> img,
                                            int sw, int sh,
                                            int dw, int dh, char aa)
{
    int scw, sch;

    scw = dw * uint64_t(img->width()) / sw;
    sch = dh * uint64_t(img->height()) / sh;

    std::shared_ptr<LImageScaleInfo>  isi = std::make_shared<LImageScaleInfo>();
    isi->sh = sh;
    isi->sw = sw;

    isi->xup_yup = (std::abs(dw) >= sw) + ((std::abs(dh) >= sh) << 1);

    isi->xpoints = lImageCalcXPoints(img->width(), scw);
    if (isi->xpoints == nullptr){
        return nullptr;
    }
    isi->ypoints = lImageCalcYPoints((const unsigned int *)img->scanLine(0),
                                     img->bytesPerLine() / 4, img->height(), sch);
    if (isi->ypoints == nullptr){
        return nullptr;
    }
    if (aa) {
        isi->xapoints = lImageCalcApoints(img->width(), scw, isi->xup_yup & 1);
        if (isi->xapoints == nullptr){
            return nullptr;
        }
        isi->yapoints = lImageCalcApoints(img->height(), sch, isi->xup_yup & 2);
        if (isi->yapoints == nullptr){
            return nullptr;
        }
    }
    return isi;
}

inline static void lImageScaleAARGB_helper(const unsigned int *pix, int xyap, int Cxy, int step, int &r, int &g, int &b)
{
    r = lRed(*pix)   * xyap;
    g = lGreen(*pix) * xyap;
    b = lBlue(*pix)  * xyap;
    int j;
    for (j = (1 << 14) - xyap; j > Cxy; j -= Cxy) {
        pix += step;
        r += lRed(*pix)   * Cxy;
        g += lGreen(*pix) * Cxy;
        b += lBlue(*pix)  * Cxy;
    }
    pix += step;
    r += lRed(*pix)   * j;
    g += lGreen(*pix) * j;
    b += lBlue(*pix)  * j;
}


inline static void lImageScaleAARGBA_helper(const unsigned int *pix, int xyap, int Cxy, int step, int &r, int &g, int &b, int &a)
{
    r = lRed(*pix)   * xyap;
    g = lGreen(*pix) * xyap;
    b = lBlue(*pix)  * xyap;
    a = lAlpha(*pix) * xyap;
    int j;
    for (j = (1 << 14) - xyap; j > Cxy; j -= Cxy) {
        pix += step;
        r += lRed(*pix)   * Cxy;
        g += lGreen(*pix) * Cxy;
        b += lBlue(*pix)  * Cxy;
        a += lAlpha(*pix) * Cxy;
    }
    pix += step;
    r += lRed(*pix)   * j;
    g += lGreen(*pix) * j;
    b += lBlue(*pix)  * j;
    a += lAlpha(*pix) * j;
}


static void lImageScaleAARGBA_up_xy(std::shared_ptr<LImageScaleInfo> isi,
                                    unsigned int *dest,
                                    int dw, int dh, int dow, int sow)
{
    const unsigned int **ypoints = isi->ypoints;
    int *xpoints = isi->xpoints;
    int *xapoints = isi->xapoints;
    int *yapoints = isi->yapoints;

    /* go through every scanline in the output buffer */
    auto scaleSection = [&] (int yStart, int yEnd) {
        for (int y = yStart; y < yEnd; ++y) {
            /* calculate the source line we'll scan from */
            const unsigned int *sptr = ypoints[y];
            unsigned int *dptr = dest + (y * dow);
            const int yap = yapoints[y];
            if (yap > 0) {
                for (int x = 0; x < dw; x++) {
                    const unsigned int *pix = sptr + xpoints[x];
                    const int xap = xapoints[x];
                    if (xap > 0){
                        *dptr = draw::interpolate4Pixels(pix, pix + sow, xap, yap);
                    }
                    else{
                        *dptr = draw::interpolatePixel256(pix[0], 256 - yap, pix[sow], yap);
                    }
                    dptr++;
                }
            } else {
                for (int x = 0; x < dw; x++) {
                    const unsigned int *pix = sptr + xpoints[x];
                    const int xap = xapoints[x];
                    if (xap > 0){
                        *dptr = draw::interpolatePixel256(pix[0], 256 - xap, pix[1], xap);
                    }
                    else{
                        *dptr = pix[0];
                    }
                    dptr++;
                }
            }
        }
    };
    scaleSection(0, dh);
}

static void lImageScaleAARGB_up_x_down_y(std::shared_ptr<LImageScaleInfo> isi, unsigned int *dest,
                                            int dw, int dh, int dow, int sow)
{
    const unsigned int **ypoints = isi->ypoints;
    int *xpoints = isi->xpoints;
    int *xapoints = isi->xapoints;
    int *yapoints = isi->yapoints;

    /* go through every scanline in the output buffer */
    auto scaleSection = [&] (int yStart, int yEnd) {
        for (int y = yStart; y < yEnd; ++y) {
            int Cy = yapoints[y] >> 16;
            int yap = yapoints[y] & 0xffff;

            unsigned int *dptr = dest + (y * dow);
            for (int x = 0; x < dw; x++) {
                const unsigned int *sptr = ypoints[y] + xpoints[x];
                int r, g, b;
                lImageScaleAARGB_helper(sptr, yap, Cy, sow, r, g, b);

                int xap = xapoints[x];
                if (xap > 0) {
                    int rr, bb, gg;
                    lImageScaleAARGB_helper(sptr + 1, yap, Cy, sow, rr, gg, bb);

                    r = r * (256 - xap);
                    g = g * (256 - xap);
                    b = b * (256 - xap);
                    r = (r + (rr * xap)) >> 8;
                    g = (g + (gg * xap)) >> 8;
                    b = (b + (bb * xap)) >> 8;
                }
                *dptr++ = LColor::argbToUInt(r >> 14, g >> 14, b >> 14);
            }
        }
    };
    scaleSection(0, dh);
}


static void lImageScaleAARGBA_up_x_down_y(std::shared_ptr<LImageScaleInfo> isi,
                                          unsigned int *dest,
                                          int dw, int dh, int dow, int sow)
{
    const unsigned int **ypoints = isi->ypoints;
    int *xpoints = isi->xpoints;
    int *xapoints = isi->xapoints;
    int *yapoints = isi->yapoints;

    /* go through every scanline in the output buffer */
    auto scaleSection = [&] (int yStart, int yEnd) {
        for (int y = yStart; y < yEnd; ++y) {
            int Cy = yapoints[y] >> 16;
            int yap = yapoints[y] & 0xffff;

            unsigned int *dptr = dest + (y * dow);
            for (int x = 0; x < dw; x++) {
                const unsigned int *sptr = ypoints[y] + xpoints[x];
                int r, g, b, a;
                lImageScaleAARGBA_helper(sptr, yap, Cy, sow, r, g, b, a);

                int xap = xapoints[x];
                if (xap > 0) {
                    int rr, gg, bb, aa;
                    lImageScaleAARGBA_helper(sptr + 1, yap, Cy, sow, rr, gg, bb, aa);

                    r = r * (256 - xap);
                    g = g * (256 - xap);
                    b = b * (256 - xap);
                    a = a * (256 - xap);
                    r = (r + (rr * xap)) >> 8;
                    g = (g + (gg * xap)) >> 8;
                    b = (b + (bb * xap)) >> 8;
                    a = (a + (aa * xap)) >> 8;
                }
                *dptr++ = LColor::argbToUInt(r >> 14, g >> 14, b >> 14, a >> 14);
            }
        }
    };

    scaleSection(0, dh);
}

static void lImageScaleAARGB_down_x_up_y(std::shared_ptr<LImageScaleInfo> isi,
                                         unsigned int *dest,
                                         int dw, int dh, int dow, int sow)
{
    const unsigned int **ypoints = isi->ypoints;
    int *xpoints = isi->xpoints;
    int *xapoints = isi->xapoints;
    int *yapoints = isi->yapoints;

    /* go through every scanline in the output buffer */
    auto scaleSection = [&] (int yStart, int yEnd) {
        for (int y = yStart; y < yEnd; ++y) {
            unsigned int *dptr = dest + (y * dow);
            for (int x = 0; x < dw; x++) {
                int Cx = xapoints[x] >> 16;
                int xap = xapoints[x] & 0xffff;

                const unsigned int *sptr = ypoints[y] + xpoints[x];
                int r, g, b;
                lImageScaleAARGB_helper(sptr, xap, Cx, 1, r, g, b);

                int yap = yapoints[y];
                if (yap > 0) {
                    int rr, bb, gg;
                    lImageScaleAARGB_helper(sptr + sow, xap, Cx, 1, rr, gg, bb);

                    r = r * (256 - yap);
                    g = g * (256 - yap);
                    b = b * (256 - yap);
                    r = (r + (rr * yap)) >> 8;
                    g = (g + (gg * yap)) >> 8;
                    b = (b + (bb * yap)) >> 8;
                }
                *dptr++ = LColor::argbToUInt(r >> 14, g >> 14, b >> 14);
            }
        }
    };
    scaleSection(0, dh);
}



static void lImageScaleAARGBA_down_x_up_y(std::shared_ptr<LImageScaleInfo> isi,
                                          unsigned int *dest,
                                          int dw, int dh, int dow, int sow)
{
    const unsigned int **ypoints = isi->ypoints;
    int *xpoints = isi->xpoints;
    int *xapoints = isi->xapoints;
    int *yapoints = isi->yapoints;

    /* go through every scanline in the output buffer */
    auto scaleSection = [&] (int yStart, int yEnd) {
        for (int y = yStart; y < yEnd; ++y) {
            unsigned int *dptr = dest + (y * dow);
            for (int x = 0; x < dw; x++) {
                int Cx = xapoints[x] >> 16;
                int xap = xapoints[x] & 0xffff;

                const unsigned int *sptr = ypoints[y] + xpoints[x];
                int r, g, b, a;
                lImageScaleAARGBA_helper(sptr, xap, Cx, 1, r, g, b, a);

                int yap = yapoints[y];
                if (yap > 0) {
                    int rr, gg, bb, aa;
                    lImageScaleAARGBA_helper(sptr + sow, xap, Cx, 1, rr, gg, bb, aa);

                    r = r * (256 - yap);
                    g = g * (256 - yap);
                    b = b * (256 - yap);
                    a = a * (256 - yap);
                    r = (r + (rr * yap)) >> 8;
                    g = (g + (gg * yap)) >> 8;
                    b = (b + (bb * yap)) >> 8;
                    a = (a + (aa * yap)) >> 8;
                }
                *dptr = LColor::argbToUInt(r >> 14, g >> 14, b >> 14, a >> 14);
                dptr++;
            }
        }
    };
    scaleSection(0, dh);
}

static void lImageScaleAARGB_down_xy(std::shared_ptr<LImageScaleInfo> isi,
                                     unsigned int *dest,
                                     int dw, int dh, int dow, int sow)
{
    const unsigned int **ypoints = isi->ypoints;
    int *xpoints = isi->xpoints;
    int *xapoints = isi->xapoints;
    int *yapoints = isi->yapoints;

    auto scaleSection = [&] (int yStart, int yEnd) {
        for (int y = yStart; y < yEnd; ++y) {
            int Cy = yapoints[y] >> 16;
            int yap = yapoints[y] & 0xffff;

            unsigned int *dptr = dest + (y * dow);
            for (int x = 0; x < dw; x++) {
                int Cx = xapoints[x] >> 16;
                int xap = xapoints[x] & 0xffff;

                const unsigned int *sptr = ypoints[y] + xpoints[x];
                int rx, gx, bx;
                lImageScaleAARGB_helper(sptr, xap, Cx, 1, rx, gx, bx);

                int r = (rx >> 4) * yap;
                int g = (gx >> 4) * yap;
                int b = (bx >> 4) * yap;

                int j;
                for (j = (1 << 14) - yap; j > Cy; j -= Cy) {
                    sptr += sow;
                    lImageScaleAARGB_helper(sptr, xap, Cx, 1, rx, gx, bx);

                    r += (rx >> 4) * Cy;
                    g += (gx >> 4) * Cy;
                    b += (bx >> 4) * Cy;
                }
                sptr += sow;
                lImageScaleAARGB_helper(sptr, xap, Cx, 1, rx, gx, bx);

                r += (rx >> 4) * j;
                g += (gx >> 4) * j;
                b += (bx >> 4) * j;

                *dptr = LColor::argbToUInt(r >> 24, g >> 24, b >> 24);
                dptr++;
            }
        }
    };
    scaleSection(0, dh);
}

static void lImageScaleAARGBA_down_xy(std::shared_ptr<LImageScaleInfo> isi,
                                      unsigned int *dest,
                                      int dw, int dh, int dow, int sow)
{
    const unsigned int **ypoints = isi->ypoints;
    int *xpoints = isi->xpoints;
    int *xapoints = isi->xapoints;
    int *yapoints = isi->yapoints;

    auto scaleSection = [&] (int yStart, int yEnd) {
        for (int y = yStart; y < yEnd; ++y) {
            int Cy = (yapoints[y]) >> 16;
            int yap = (yapoints[y]) & 0xffff;

            unsigned int *dptr = dest + (y * dow);
            for (int x = 0; x < dw; x++) {
                int Cx = xapoints[x] >> 16;
                int xap = xapoints[x] & 0xffff;

                const unsigned int *sptr = ypoints[y] + xpoints[x];
                int rx, gx, bx, ax;
                lImageScaleAARGBA_helper(sptr, xap, Cx, 1, rx, gx, bx, ax);

                int r = ((rx>>4) * yap);
                int g = ((gx>>4) * yap);
                int b = ((bx>>4) * yap);
                int a = ((ax>>4) * yap);

                int j;
                for (j = (1 << 14) - yap; j > Cy; j -= Cy) {
                    sptr += sow;
                    lImageScaleAARGBA_helper(sptr, xap, Cx, 1, rx, gx, bx, ax);
                    r += ((rx>>4) * Cy);
                    g += ((gx>>4) * Cy);
                    b += ((bx>>4) * Cy);
                    a += ((ax>>4) * Cy);
                }
                sptr += sow;
                lImageScaleAARGBA_helper(sptr, xap, Cx, 1, rx, gx, bx, ax);

                r += ((rx>>4) * j);
                g += ((gx>>4) * j);
                b += ((bx>>4) * j);
                a += ((ax>>4) * j);

                *dptr = LColor::argbToUInt(r >> 24, g >> 24, b >> 24, a >> 24);
                dptr++;
            }
        }
    };
    scaleSection(0, dh);
}

/* scale by area sampling - IGNORE the ALPHA byte*/
static void lImageScaleAARGB(std::shared_ptr<LImageScaleInfo> isi,
                             unsigned int *dest,
                             int dw, int dh,
                             int dow, int sow)
{
    /* scaling up both ways */
    if (isi->xup_yup == 3) {
        lImageScaleAARGBA_up_xy(isi, dest, dw, dh, dow, sow);
    }
    /* if we're scaling down vertically */
    else if (isi->xup_yup == 1) {
        lImageScaleAARGB_up_x_down_y(isi, dest, dw, dh, dow, sow);
    }
    /* if we're scaling down horizontally */
    else if (isi->xup_yup == 2) {
        lImageScaleAARGB_down_x_up_y(isi, dest, dw, dh, dow, sow);
    }
    /* if we're scaling down horizontally & vertically */
    else {
        lImageScaleAARGB_down_xy(isi, dest, dw, dh, dow, sow);
    }
}


/* scale by area sampling - with alpha */
static void lImageScaleAARGBA(std::shared_ptr<LImageScaleInfo> isi,
                              unsigned int *dest,
                              int dw, int dh,
                              int dow, int sow)
{
    /* scaling up both ways */
    if (isi->xup_yup == 3) {
        lImageScaleAARGBA_up_xy(isi, dest, dw, dh, dow, sow);
    }
    /* if we're scaling down vertically */
    else if (isi->xup_yup == 1) {
        lImageScaleAARGBA_up_x_down_y(isi, dest, dw, dh, dow, sow);
    }
    /* if we're scaling down horizontally */
    else if (isi->xup_yup == 2) {
        lImageScaleAARGBA_down_x_up_y(isi, dest, dw, dh, dow, sow);
    }
    /* if we're scaling down horizontally & vertically */
    else {
        lImageScaleAARGBA_down_xy(isi, dest, dw, dh, dow, sow);
    }
}

std::shared_ptr<LImage> LImage::lSmoothScaleImage(const std::shared_ptr<LImage> src, int dw, int dh)
{
    if (src == nullptr || dw <= 0 || dh <= 0){
        return nullptr;
    }

    int w = src->m_width;
    int h = src->m_height;
    std::shared_ptr<LImageScaleInfo> scaleinfo =
        lImageCalcScaleInfo(src, w, h, dw, dh, true);

    if (scaleinfo == nullptr){
        return nullptr;
    }

    std::shared_ptr<LImage> buffer = std::make_shared<LImage>(dw, dh, src->m_format);
    if (src->hasAlphaChannel()){
        lImageScaleAARGBA(scaleinfo, (unsigned int *)buffer->scanLine(0),
                          dw, dh, dw, src->m_bytesPerLine / 4);
    }
    else{
        lImageScaleAARGB(scaleinfo, (unsigned int *)buffer->scanLine(0),
                         dw, dh, dw, src->m_bytesPerLine / 4);
    }
    return buffer;
}

LK_NAMESPACE_END
