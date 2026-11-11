#include "lsvgtanimatedproperty.h"
#include "lkcore/ldebug.h"
#include "lkgui/paint/ltransform2d.h"
#include <iterator>

LK_NAMESPACE_BEGIN

typedef std::map<std::string, LSvgAbstractAnimatedProperty::Type> AnimatableHashType;
static AnimatableHashType animatableProperties;

static void initHash()
{
    animatableProperties["fill"] = LSvgAbstractAnimatedProperty::Color;
    animatableProperties["stroke"] = LSvgAbstractAnimatedProperty::Color;
    animatableProperties["transform"] = LSvgAbstractAnimatedProperty::Transform;
}

inline static double llerp(double a, double b, double t)
{
    return a + (b - a) * t;
}

static LPoint<double> pointInterpolator(LPoint<double> v1, LPoint<double> v2, double t)
{
    double x = llerp(v1.m_x, v2.m_x, t);
    double y = llerp(v1.m_y, v2.m_y, t);

    return LPoint<double>(x, y);
}


LSvgAbstractAnimatedProperty::LSvgAbstractAnimatedProperty(const std::string &name, Type type)
    : m_propertyName(name)
    , m_type(type)
{
}

LSvgAbstractAnimatedProperty::~LSvgAbstractAnimatedProperty()
{
}

void LSvgAbstractAnimatedProperty::setKeyFrames(const std::list<double> &keyFrames)
{
    m_keyFrames = keyFrames;
}

void LSvgAbstractAnimatedProperty::appendKeyFrame(double keyFrame)
{
    m_keyFrames.push_back(keyFrame);
}

void LSvgAbstractAnimatedProperty::setPropertyName(const std::string &name)
{
    m_propertyName = name;
}


std::shared_ptr<LSvgAbstractAnimatedProperty>  LSvgAbstractAnimatedProperty::createAnimatedProperty(const std::string &name)
{
    if (animatableProperties.empty()){
        initHash();
    }

    AnimatableHashType::const_iterator iter = animatableProperties.find(name);
    if(iter == animatableProperties.cend()){
        lWarn() << "Property : " << name << " is not animatable";
    }

    LSvgAbstractAnimatedProperty::Type type = iter->second;
    std::shared_ptr<LSvgAbstractAnimatedProperty> prop = nullptr;

    switch (type) {
    case LSvgAbstractAnimatedProperty::Color:{
        prop = std::make_shared<LSvgAnimatedPropertyColor>(name);
        break;
    }
    case LSvgAbstractAnimatedProperty::Transform:{
        prop = std::make_shared<LSvgAnimatedPropertyTransform>(name);
        break;
    }
    default:
        break;
    }

    return prop;
}

LSvgAnimatedPropertyColor::LSvgAnimatedPropertyColor(const std::string &name)
    : LSvgAbstractAnimatedProperty(name, LSvgAbstractAnimatedProperty::Color)
{
}

void LSvgAnimatedPropertyColor::setColors(const std::list<LColor> &colors)
{
    m_colors = colors;
}

void LSvgAnimatedPropertyColor::appendColor(const LColor &color)
{
    m_colors.push_back(color);
}

void LSvgAnimatedPropertyColor::interpolate(uint32_t index, double t)
{
    auto it = m_colors.cbegin();
    std::advance(it, index-1);

    if(it != m_colors.end()){
        LColor c1 = *it;
        ++it;
        if(it != m_colors.end()){
            LColor c2 = *it;
            int alpha  = llerp(c1.m_alpha, c2.m_alpha, t);
            int red    = llerp(c1.m_red, c2.m_red, t);
            int green  = llerp(c1.m_green, c2.m_green, t);
            int blue   = llerp(c1.m_blue, c2.m_blue, t);

            m_interpolatedValue = LColor(red, green, blue, alpha).toUInt();
        }
    }
}

LSvgAnimatedPropertyTransform::LSvgAnimatedPropertyTransform(const std::string &name)
    : LSvgAbstractAnimatedProperty(name, LSvgAbstractAnimatedProperty::Transform)
{

}

void LSvgAnimatedPropertyTransform::setTranslations(const std::list<LPoint<double>> &translations)
{
    m_translations = translations;
}

void LSvgAnimatedPropertyTransform::appendTranslation(const LPoint<double> &translation)
{
    m_translations.push_back(translation);
}

void LSvgAnimatedPropertyTransform::setScales(const std::list<LPoint<double>> &scales)
{
    m_scales = scales;
}

void LSvgAnimatedPropertyTransform::appendScale(const LPoint<double> &scale)
{
    m_scales.push_back(scale);
}

void LSvgAnimatedPropertyTransform::setRotations(const std::list<double> &rotations)
{
    m_rotations = rotations;
}

void LSvgAnimatedPropertyTransform::appendRotation(double rotation)
{
    m_rotations.push_back(rotation);
}


void LSvgAnimatedPropertyTransform::setCentersOfRotation(const std::list<LPoint<double>> &centersOfRotations)
{
    m_centersOfRotation = centersOfRotations;
}

void LSvgAnimatedPropertyTransform::appendCenterOfRotation(const LPoint<double> &centerOfRotation)
{
    m_centersOfRotation.push_back(centerOfRotation);
}

void LSvgAnimatedPropertyTransform::setSkews(const std::list<LPoint<double>> &skews)
{
    m_skews = skews;
}

void LSvgAnimatedPropertyTransform::appendSkew(const LPoint<double> &skew)
{
    m_skews.push_back(skew);
}

double LSvgAnimatedPropertyTransform::interpolatedRotation(uint32_t index, double t) const
{
    auto it = m_rotations.cbegin();
    std::advance(it, index-1);
    if(it != m_rotations.end()){
        double r1 = *it;
        ++it;
        if(it != m_rotations.end()){
            double r2 = *it;
            return llerp(r1, r2, t);
        }
    }
    return 0;
}

LPoint<double> LSvgAnimatedPropertyTransform::interpolatedCenterOfRotation(uint32_t index, double t) const
{
    auto it = m_centersOfRotation.cbegin();
    std::advance(it, index-1);
    if(it != m_centersOfRotation.end()){
        LPoint<double> cor1 = *it;
        ++it;
        if(it != m_centersOfRotation.end()){
            LPoint<double> cor2 = *it;
            return pointInterpolator(cor1, cor2, t);
        }
    }
    return LPoint<double>();
}

LPoint<double> LSvgAnimatedPropertyTransform::interpolatedSkew(uint32_t index, double t) const
{
    auto it = m_skews.cbegin();
    std::advance(it, index-1);
    if(it != m_skews.end()){
        LPoint<double> cor1 = *it;
        ++it;
        if(it != m_skews.end()){
            LPoint<double> cor2 = *it;
            return pointInterpolator(cor1, cor2, t);
        }
    }
    return LPoint<double>();
}

LPoint<double> LSvgAnimatedPropertyTransform::interpolatedTranslation(uint32_t index, double t) const
{
    auto it = m_translations.cbegin();
    std::advance(it, index-1);
    if(it != m_translations.end()){
        LPoint<double> cor1 = *it;
        ++it;
        if(it != m_translations.end()){
            LPoint<double> cor2 = *it;
            return pointInterpolator(cor1, cor2, t);
        }
    }
    return LPoint<double>();
}

LPoint<double> LSvgAnimatedPropertyTransform::interpolatedScale(uint32_t index, double t) const
{
    auto it = m_scales.cbegin();
    std::advance(it, index-1);
    if(it != m_scales.end()){
        LPoint<double> cor1 = *it;
        ++it;
        if(it != m_scales.end()){
            LPoint<double> cor2 = *it;
            return pointInterpolator(cor1, cor2, t);
        }
    }
    return LPoint<double>();
}

void LSvgAnimatedPropertyTransform::interpolate(uint32_t index, double t)
{
    if (index >= (uint32_t)m_keyFrames.size()) {
        lWarn() << "Invalid index for key frames";
        return;
    }

    LTransform2D transform = LTransform2D();

    if (m_skews.size() == m_keyFrames.size()) {
        const LPoint<double> skew = interpolatedSkew(index, t);
        transform.shear(std::tan(lDegreesToRadians(skew.m_x)), std::tan(lDegreesToRadians(skew.m_y)));
    }

    if (m_scales.size() == m_keyFrames.size()) {
        const LPoint<double> scale = interpolatedScale(index, t);
        transform.scale(scale.m_x, scale.m_y);
    }

    if (m_rotations.size() == m_keyFrames.size() &&
        m_centersOfRotation.size() == m_keyFrames.size()) {
        const double rotation = interpolatedRotation(index, t);
        const LPoint<double> cor = interpolatedCenterOfRotation(index, t);

        transform.translate(cor.m_x, cor.m_y);
        transform.rotate(rotation);
        transform.translate(-cor.m_x, -cor.m_y);
    }

    if (m_translations.size() == m_keyFrames.size()) {
        const LPoint<double> translation = interpolatedTranslation(index, t);
        transform.translate(translation.m_x, translation.m_y);
    }

    m_interpolatedValue = transform.toCbor();
}

LK_NAMESPACE_END
