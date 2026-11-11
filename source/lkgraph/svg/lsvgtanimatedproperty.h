#ifndef LSVGTANIMATEDPROPERTY_H
#define LSVGTANIMATEDPROPERTY_H

#include "lkgui/paint/lcolor.h"
#include "lkgui/lpoint.h"

LK_NAMESPACE_BEGIN

class LKSVGDEF LSvgAbstractAnimatedProperty
{
public:
    enum Type
    {
        Int,
        Float,
        Color,
        Transform,
    };

    LSvgAbstractAnimatedProperty(const std::string &name, Type type);
    virtual ~LSvgAbstractAnimatedProperty();

    void setKeyFrames(const std::list<double> &keyFrames);
    void appendKeyFrame(double keyFrame);
    const std::list<double>& keyFrames() const{
        return m_keyFrames;
    }
    void setPropertyName(const std::string &name);
    inline const std::string& propertyName() const{
        return m_propertyName;
    }
    inline Type type() const{
        return m_type;
    }
    inline const LVariant& interpolatedValue() const{
        return m_interpolatedValue;
    }
    virtual void interpolate(uint32_t index, double t) = 0;

    static std::shared_ptr<LSvgAbstractAnimatedProperty> createAnimatedProperty(const std::string &name);
protected:
    std::list<double> m_keyFrames;
    LVariant m_interpolatedValue;

private:
    std::string m_propertyName;
    Type m_type;
};

class LKSVGDEF LSvgAnimatedPropertyColor : public LSvgAbstractAnimatedProperty
{
public:
    LSvgAnimatedPropertyColor(const std::string &name);

    void setColors(const std::list<LColor> &colors);
    void appendColor(const LColor &color);
    inline const std::list<LColor>& colors() const
    {
        return m_colors;
    }
    void interpolate(uint32_t index, double t) override;
private:
    std::list<LColor> m_colors;
};

class LKSVGDEF LSvgAnimatedPropertyTransform : public LSvgAbstractAnimatedProperty
{
public:
    LSvgAnimatedPropertyTransform(const std::string &name);

    void setTranslations(const std::list<LPoint<double>> &translations);
    void appendTranslation(const LPoint<double> &translation);
    inline const std::list<LPoint<double>>& translations() const{
        return m_translations;
    }

    void setScales(const std::list<LPoint<double>> &scales);
    void appendScale(const LPoint<double> &scale);
    inline const std::list<LPoint<double>>& scales() const
    {
        return m_scales;
    }

    void setRotations(const std::list<double> &rotations);
    void appendRotation(double rotation);
    inline const std::list<double>& rotations() const{
        return m_rotations;
    }

    void setCentersOfRotation(const std::list<LPoint<double>> &centersOfRotations);
    void appendCenterOfRotation(const LPoint<double> &centerOfRotation);
    inline const std::list<LPoint<double>>& centersOfRotations() const
    {
        return m_centersOfRotation;
    }
    void setSkews(const std::list<LPoint<double>> &skews);
    void appendSkew(const LPoint<double> &skew);
    inline const std::list<LPoint<double>>& skews() const
    {
        return m_skews;
    }


    void interpolate(uint32_t index, double t) override;

    double interpolatedRotation(uint32_t index, double t) const;
    LPoint<double> interpolatedCenterOfRotation(uint32_t index, double t) const;
    LPoint<double> interpolatedScale(uint32_t index, double t) const;
    LPoint<double> interpolatedTranslation(uint32_t index, double t) const;
    LPoint<double> interpolatedSkew(uint32_t index, double t) const;

private:
    std::list<LPoint<double>> m_translations;
    std::list<LPoint<double>> m_scales;
    std::list<double> m_rotations;
    std::list<LPoint<double>> m_centersOfRotation;
    std::list<LPoint<double>> m_skews;
};


LK_NAMESPACE_END

#endif // LSVGTANIMATEDPROPERTY_H
