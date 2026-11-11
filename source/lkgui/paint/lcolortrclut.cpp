#include "lcolortrclut.h"
#include "lcolortransfertable.h"
LK_NAMESPACE_BEGIN

LColorTrcLut::LColorTrcLut()
{

}


std::shared_ptr<LColorTrcLut> LColorTrcLut::fromGamma(double gamma)
{
    std::shared_ptr<LColorTrcLut>  cp = std::make_shared<LColorTrcLut>();

    for (int i = 0; i <= (255 * 16); ++i) {
        cp->m_toLinear[i] = uint16_t(std::lround(std::pow(i / double(255 * 16), gamma) * (255 * 256)));
        cp->m_fromLinear[i] = uint16_t(std::lround(std::pow(i / double(255 * 16), double(1) / gamma) * (255 * 256)));
    }

    return cp;

}

std::shared_ptr<LColorTrcLut> LColorTrcLut::fromTransferFunction(const LColorTransferFunction &fun)
{
    std::shared_ptr<LColorTrcLut>  cp = std::make_shared<LColorTrcLut>();
    LColorTransferFunction inv = fun.inverted();

    for (int i = 0; i <= (255 * 16); ++i) {
        cp->m_toLinear[i] = uint16_t(std::lround(fun.apply(i / double(255 * 16)) * (255 * 256)));
        cp->m_fromLinear[i] = uint16_t(std::lround(inv.apply(i / double(255 * 16)) * (255 * 256)));
    }

    return cp;
}

std::shared_ptr<LColorTrcLut> LColorTrcLut::fromTransferTable(const LColorTransferTable &table)
{
    std::shared_ptr<LColorTrcLut>  cp = std::make_shared<LColorTrcLut>();

    float minInverse = 0.0f;
    for (int i = 0; i <= (255 * 16); ++i) {
        cp->m_toLinear[i] = uint16_t(lBound(0, (int)std::lround(table.apply(i / double(255 * 16)) * (255 * 256)), 65280));
        minInverse = table.applyInverse(i / double(255 * 16), minInverse);
        cp->m_fromLinear[i] = uint16_t(lBound(0, (int)std::lround(minInverse * (255 * 256)), 65280));
    }

    return cp;
}

LK_NAMESPACE_END
