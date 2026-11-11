#ifndef LSVGFILTER_H
#define LSVGFILTER_H

#include "lkgraph/svg/lsvgstructure.h"
#include <array>

LK_NAMESPACE_BEGIN

class LKSVGDEF LSvgFeFilterPrimitive : public LSvgStructure
{
public:
    LSvgFeFilterPrimitive(std::shared_ptr<LSvgNode> parent, const std::string &input,
                          const std::string &result, const LSvgRectInfo &rect);
    void drawCommand(LPainter *, LSvgExtraStates &) override {}
    bool shouldDrawNode(LPainter *, LSvgExtraStates &) const override;
    LRect<double> internalFastBounds(LPainter *, LSvgExtraStates &) const override { return LRect<double>(); }
    LRect<double> internalBounds(LPainter *, LSvgExtraStates &) const override { return LRect<double>(); }
    LRect<double> localSubRegion(const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                          LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const;
    LRect<double> globalSubRegion(LPainter *p,
                           const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                           LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const;
    void clipToTransformedBounds(std::shared_ptr<LImage> buffer, LPainter *p, const LRect<double> &localRect) const;
    virtual std::shared_ptr<LImage>  apply(const std::map<std::string, std::shared_ptr<LImage> > &sources,
                         LPainter *p, const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                         LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const = 0;
    virtual bool requiresSourceAlpha() const;
    std::string input() const {
        return m_input;
    }
    std::string result() const {
        return m_result;
    }

    static std::shared_ptr<LSvgFeFilterPrimitive> castToFilterPrimitive(std::shared_ptr<LSvgNode> node);

protected:
    std::string m_input;
    std::string m_result;
    LSvgRectInfo m_rect;


};

class LKSVGDEF LSvgFeColorMatrix : public LSvgFeFilterPrimitive
{
public:
    enum class ColorShiftType : uint8_t {
        Matrix,
        Saturate,
        HueRotate,
        LuminanceToAlpha
    };

    typedef std::array<std::array<double, 5>, 5> Matrix;
    LSvgFeColorMatrix(std::shared_ptr<LSvgNode> parent, const std::string &input, const std::string &result,
                      const LSvgRectInfo &rect, ColorShiftType type, const Matrix &matrix);
    Type type() const override;
    std::shared_ptr<LImage>  apply(const std::map<std::string, std::shared_ptr<LImage> > &sources,
                 LPainter *p, const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                 LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const override;

    static void fill(Matrix& m, double v);
    static void setIdentity(Matrix& m);
private:
    Matrix m_matrix;
};

class LKSVGDEF LSvgFeGaussianBlur : public LSvgFeFilterPrimitive
{
public:
    enum class EdgeMode : uint8_t {
        Duplicate,
        Wrap,
        None
    };

    LSvgFeGaussianBlur(std::shared_ptr<LSvgNode> parent, const std::string &input, const std::string &result,
                       const LSvgRectInfo &rect, double stdDeviationX, double stdDeviationY,
                       EdgeMode edgemode);
    Type type() const override;
    std::shared_ptr<LImage>  apply(const std::map<std::string, std::shared_ptr<LImage> > &sources,
                 LPainter *p, const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                 LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const override;
private:
    double m_stdDeviationX;
    double m_stdDeviationY;
    EdgeMode m_edgemode; // TODO: Unused. Start using it when there's a reference implementation.
};

class LKSVGDEF LSvgFeOffset : public LSvgFeFilterPrimitive
{
public:
    LSvgFeOffset(std::shared_ptr<LSvgNode> parent, const std::string &input, const std::string &result,
                 const LSvgRectInfo &rect, double dx, double dy);
    Type type() const override;
    std::shared_ptr<LImage>  apply(const std::map<std::string, std::shared_ptr<LImage> > &sources,
                 LPainter *p, const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                 LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const override;
private:
    double m_dx;
    double m_dy;
};

class LKSVGDEF LSvgFeMerge : public LSvgFeFilterPrimitive
{
public:
    LSvgFeMerge(std::shared_ptr<LSvgNode> parent, const std::string &input,
                const std::string &result, const LSvgRectInfo &rect);
    Type type() const override;
    std::shared_ptr<LImage>  apply(const std::map<std::string, std::shared_ptr<LImage> > &sources,
                 LPainter *p, const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                 LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const override;
    bool requiresSourceAlpha() const override;
};

class LKSVGDEF LSvgFeMergeNode : public LSvgFeFilterPrimitive
{
public:
    LSvgFeMergeNode(std::shared_ptr<LSvgNode> parent, const std::string &input,
                    const std::string &result, const LSvgRectInfo &rect);
    Type type() const override;
    std::shared_ptr<LImage>  apply(const std::map<std::string, std::shared_ptr<LImage> > &sources,
                 LPainter *p, const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                 LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const override;
};

class LKSVGDEF LSvgFeComposite : public LSvgFeFilterPrimitive
{
public:
    enum class Operator : uint8_t {
        Over,
        In,
        Out,
        Atop,
        Xor,
        Lighter,
        Arithmetic
    };
    LSvgFeComposite(std::shared_ptr<LSvgNode> parent, const std::string &input, const std::string &result,
                    const LSvgRectInfo &rect, const std::string &input2, Operator op, const std::array<double, 4> &k);
    Type type() const override;
    std::shared_ptr<LImage>  apply(const std::map<std::string, std::shared_ptr<LImage> > &sources,
                 LPainter *p, const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                 LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const override;
    bool requiresSourceAlpha() const override;
private:
    std::string m_input2;
    Operator m_operator;
    std::array<double, 4> m_k;
};

class LKSVGDEF LSvgFeFlood : public LSvgFeFilterPrimitive
{
public:
    LSvgFeFlood(std::shared_ptr<LSvgNode> parent, const std::string &input, const std::string &result,
                const LSvgRectInfo &rect, const LColor &color);
    Type type() const override;
    std::shared_ptr<LImage>  apply(const std::map<std::string, std::shared_ptr<LImage> > &sources,
                 LPainter *p, const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                 LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const override;
private:
    LColor m_color;
};

class LKSVGDEF LSvgFeBlend : public LSvgFeFilterPrimitive
{
public:
    enum class Mode : uint8_t {
        Normal,
        Multiply,
        Screen,
        Darken,
        Lighten
    };
    LSvgFeBlend(std::shared_ptr<LSvgNode> parent, const std::string &input, const std::string &result,
                const LSvgRectInfo &rect, const std::string &input2, Mode mode);
    Type type() const override;
    std::shared_ptr<LImage>  apply(const std::map<std::string, std::shared_ptr<LImage> > &sources,
                 LPainter *p, const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                 LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const override;
    bool requiresSourceAlpha() const override;
private:
    std::string m_input2;
    Mode m_mode;

};

class LKSVGDEF LSvgFeUnsupported : public LSvgFeFilterPrimitive
{
public:
    LSvgFeUnsupported(std::shared_ptr<LSvgNode> parent, const std::string &input,
                      const std::string &result, const LSvgRectInfo &rect);
    Type type() const override;
    std::shared_ptr<LImage>  apply(const std::map<std::string, std::shared_ptr<LImage> > &sources,
                 LPainter *p, const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                 LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const override;
};


LK_NAMESPACE_END
#endif // LSVGFILTER_H
