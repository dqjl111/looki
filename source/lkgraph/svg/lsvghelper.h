#ifndef LSVGHELPER_H
#define LSVGHELPER_H

#include "lkgui/lrect.h"

LK_NAMESPACE_BEGIN

enum class LSvgUnitTypes : uint8_t {
    unknown,
    objectBoundingBox,
    userSpaceOnUse
};

enum class LSvgAnimatorType : uint8_t {
    Controlled,
    Automatic,
};

enum class LSvgOption : uint16_t {
    NoOption           = 0x00,
    Tiny12FeaturesOnly = 0x01,
    AssumeTrustedSource = 0x02,
    // reserved for non-animation options: 0x04
    // reserved for non-animation options: 0x08
    DisableSMILAnimations = 0x10,
    DisableCSSAnimations = 0x20,
    // reserved for potentially other animations: 0x40
    // reserved for potentially other animations: 0x80
    DisableAnimations = 0xf0,
    // next value for non-animations: 0x0100
};

L_EN_FLAGS(LSvgOption)

enum class LSvgPreserveAspectRatio : uint8_t {
    None =  0b000000,
    xMin =  0b000001,
    xMid =  0b000010,
    xMax =  0b000011,
    yMin =  0b000100,
    yMid =  0b001000,
    yMax =  0b001100,
    meet =  0b010000,
    slice = 0b100000,
    xMask = xMin | xMid | xMax,
    yMask = yMin | yMid | yMax,
    xyMask = xMask | yMask,
    meetSliceMask = meet | slice
};

L_EN_FLAGS(LSvgPreserveAspectRatio)



enum LSvgLengthType {
    percent,
    px,
    pc,
    pt,
    mm,
    cm,
    in,
    other
};



class LKSVGDEF LSvgRectInfo
{
public:
    LSvgRectInfo(const LRect<double> &r = LRect<double>(),
              LSvgUnitTypes unitX = LSvgUnitTypes::userSpaceOnUse,
              LSvgUnitTypes unitY = LSvgUnitTypes::userSpaceOnUse,
              LSvgUnitTypes unitW = LSvgUnitTypes::userSpaceOnUse,
              LSvgUnitTypes unitH = LSvgUnitTypes::userSpaceOnUse)
        : m_rect(r)
        , m_unitX(unitX)
        , m_unitY(unitY)
        , m_unitW(unitW)
        , m_unitH(unitH)
    {}

    LPoint<double> translationRelativeToBoundingBox(const LRect<double> &boundingBox) const {
        LPoint<double> result;

        if (m_unitX == LSvgUnitTypes::objectBoundingBox){
            result.m_x = m_rect.m_x * boundingBox.width();
        }else{
            result.m_x = m_rect.m_x;
        }
        if (m_unitY == LSvgUnitTypes::objectBoundingBox){
            result.m_y = m_rect.m_y * boundingBox.height();
        }else{
            result.m_y = m_rect.m_y;
        }
        return result;
    }

    LRect<double> resolveRelativeLengths(const LRect<double> &localRect) const {
        LRect<double> result;
        if (m_unitX == LSvgUnitTypes::objectBoundingBox){
            result.m_x = localRect.m_x + m_rect.m_x * localRect.width();
        }else{
            result.m_x = m_rect.m_x;
        }
        if (m_unitY == LSvgUnitTypes::objectBoundingBox){
            result.m_y = localRect.m_y + m_rect.m_y * localRect.height();
        }else{
            result.m_y = m_rect.m_y;
        }
        if (m_unitW == LSvgUnitTypes::objectBoundingBox){
            result.setWidth(localRect.width() * m_rect.width());
        }else{
            result.setWidth(m_rect.width());
        }
        if (m_unitH == LSvgUnitTypes::objectBoundingBox){
            result.setHeight(localRect.height() * m_rect.height());
        }else{
            result.setHeight(m_rect.height());
        }
        return result;
    }

    LRect<double> resolveRelativeLengths(const LRect<double> &localRect, LSvgUnitTypes units) const {
        LRect<double> result;
        if (units == LSvgUnitTypes::objectBoundingBox ||
            m_unitX == LSvgUnitTypes::objectBoundingBox){
            result.m_x = localRect.m_x + m_rect.m_x * localRect.width();
        }else{
            result.m_x = m_rect.m_x;
        }
        if (units == LSvgUnitTypes::objectBoundingBox ||
            m_unitY == LSvgUnitTypes::objectBoundingBox){
            result.m_y = localRect.m_y + m_rect.m_y * localRect.height();
        }else{
            result.m_y = m_rect.m_y;
        }
        if (units == LSvgUnitTypes::objectBoundingBox ||
            m_unitW == LSvgUnitTypes::objectBoundingBox){
            result.setWidth(localRect.width() * m_rect.width());
        }else{
            result.setWidth(m_rect.width());
        }
        if (units == LSvgUnitTypes::objectBoundingBox ||
            m_unitH == LSvgUnitTypes::objectBoundingBox){
            result.setHeight(localRect.height() * m_rect.height());
        }else{
            result.setHeight(m_rect.height());
        }

        return result;
    }



    LRect<double> m_rect;
    LSvgUnitTypes m_unitX, m_unitY, m_unitW, m_unitH;
};

LK_NAMESPACE_END

#endif
