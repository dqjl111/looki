#ifndef LSVGGRAPHICS_H
#define LSVGGRAPHICS_H

#include "lkgraph/svg/lsvgnode.h"

LK_NAMESPACE_BEGIN

class LTextCharFormat;

class LKSVGDEF LSvgDummyNode : public LSvgNode
{
public:
    void drawCommand(LPainter *, LSvgExtraStates &) override;
    Type type() const override;
};

class LKSVGDEF LSvgEllipse : public LSvgNode
{
public:
    LSvgEllipse(std::shared_ptr<LSvgNode> parent, const LRect<double> &rect);
    bool separateFillStroke(const LSvgExtraStates &s) const override;
    void drawCommand(LPainter *p, LSvgExtraStates &states) override;
    Type type() const override;
    LRect<double> internalFastBounds(LPainter *p, LSvgExtraStates &states) const override;
    LRect<double> internalBounds(LPainter *p, LSvgExtraStates &states) const override;
    LRect<double> decoratedInternalBounds(LPainter *p, LSvgExtraStates &states) const override;
    LRect<double> rect() const { return m_bounds; }
private:
    LRect<double> m_bounds;
};

class LKSVGDEF LSvgCircle : public LSvgEllipse
{
public:
    LSvgCircle(std::shared_ptr<LSvgNode> parent, const LRect<double> &rect) : LSvgEllipse(parent, rect) { }
    Type type() const override;
};

class LKSVGDEF LSvgImage : public LSvgNode
{
public:
    LSvgImage(std::shared_ptr<LSvgNode> parent,
              const std::shared_ptr<LImage> image,
              const std::string &filename,
              const LRect<double> &bounds);
    void drawCommand(LPainter *p, LSvgExtraStates &states) override;
    Type type() const override;
    LRect<double> internalBounds(LPainter *p, LSvgExtraStates &states) const override;

    LRect<double> rect() const { return m_bounds; }
    const std::shared_ptr<LImage> image() const { return m_image; }
    std::string filename() const { return m_filename; }
private:
    std::string m_filename;
    std::shared_ptr<LImage> m_image;
    LRect<double> m_bounds;
};

class LKSVGDEF LSvgLine : public LSvgNode
{
public:
    LSvgLine(std::shared_ptr<LSvgNode> parent, const LLine<double> &line);
    void drawCommand(LPainter *p, LSvgExtraStates &states) override;
    Type type() const override;
    LRect<double> internalFastBounds(LPainter *p, LSvgExtraStates &states) const override;
    LRect<double> internalBounds(LPainter *p, LSvgExtraStates &states) const override;
    LRect<double> decoratedInternalBounds(LPainter *p, LSvgExtraStates &states) const override;
    bool requiresGroupRendering() const override;
    LLine<double> line() const { return m_line; }
private:
    LRect<double> internalBounds(LPainter *p, LSvgExtraStates &states, BoundsMode mode) const;
    LLine<double> m_line;
};

class LKSVGDEF LSvgPath : public LSvgNode
{
public:
    LSvgPath(std::shared_ptr<LSvgNode> parent, const std::shared_ptr<LPainterPath> path);
    bool separateFillStroke(const LSvgExtraStates &s) const override;
    void drawCommand(LPainter *p, LSvgExtraStates &states) override;
    Type type() const override;
    LRect<double> internalFastBounds(LPainter *p, LSvgExtraStates &states) const override;
    LRect<double> internalBounds(LPainter *p, LSvgExtraStates &states) const override;
    LRect<double> decoratedInternalBounds(LPainter *p, LSvgExtraStates &states) const override;
    bool requiresGroupRendering() const override;
    std::shared_ptr<LPainterPath> path() const { return m_path; }
private:
    std::shared_ptr<LPainterPath> m_path;
};

class LKSVGDEF LSvgPolygon : public LSvgNode
{
public:
    LSvgPolygon(std::shared_ptr<LSvgNode> parent, const LPolygon<double> &poly);
    bool separateFillStroke(const LSvgExtraStates &s) const override;
    void drawCommand(LPainter *p, LSvgExtraStates &states) override;
    Type type() const override;
    LRect<double> internalFastBounds(LPainter *p, LSvgExtraStates &states) const override;
    LRect<double> internalBounds(LPainter *p, LSvgExtraStates &states) const override;
    LRect<double> decoratedInternalBounds(LPainter *p, LSvgExtraStates &states) const override;
    bool requiresGroupRendering() const override;
    const LPolygon<double> &polygon() const { return m_poly; }
private:
    LRect<double> internalBounds(LPainter *p, LSvgExtraStates &states, BoundsMode mode) const;
    LPolygon<double> m_poly;
};

class LKSVGDEF LSvgPolyline : public LSvgNode
{
public:
    LSvgPolyline(std::shared_ptr<LSvgNode> parent, const LPolygon<double> &poly);
    bool separateFillStroke(const LSvgExtraStates &s) const override;
    void drawCommand(LPainter *p, LSvgExtraStates &states) override;
    Type type() const override;
    LRect<double> internalFastBounds(LPainter *p, LSvgExtraStates &states) const override;
    LRect<double> internalBounds(LPainter *p, LSvgExtraStates &states) const override;
    LRect<double> decoratedInternalBounds(LPainter *p, LSvgExtraStates &states) const override;
    bool requiresGroupRendering() const override;
    const LPolygon<double> &polygon() const { return m_poly; }
private:
    LRect<double> internalBounds(LPainter *p, LSvgExtraStates &states, BoundsMode mode) const;
    LPolygon<double> m_poly;
};

class LKSVGDEF LSvgRect : public LSvgNode
{
public:
    LSvgRect(std::shared_ptr<LSvgNode> paren, const LRect<double> &rect, double rx=0, double ry=0);
    Type type() const override;
    bool separateFillStroke(const LSvgExtraStates &s) const override;
    void drawCommand(LPainter *p, LSvgExtraStates &states) override;
    LRect<double> internalFastBounds(LPainter *p, LSvgExtraStates &states) const override;
    LRect<double> internalBounds(LPainter *p, LSvgExtraStates &states) const override;
    LRect<double> decoratedInternalBounds(LPainter *p, LSvgExtraStates &states) const override;
    LRect<double> rect() const { return m_rect; }
    LPoint<double> radius() const { return { m_rx, m_ry }; }
private:
    LRect<double> internalBounds(LPainter *p, LSvgExtraStates &states, BoundsMode mode) const;
    LRect<double> m_rect;
    double m_rx, m_ry;
};

class  LSvgTspan;

class LKSVGDEF LSvgText : public LSvgNode
{
public:
    enum WhitespaceMode
    {
        Default,
        Preserve
    };

    LSvgText(std::shared_ptr<LSvgNode> parent, const LPoint<double> &coord);
    void setTextArea(const LSize<double> &size);

    void drawCommand(LPainter *p, LSvgExtraStates &states) override;
    bool shouldDrawNode(LPainter *p, LSvgExtraStates &states) const override;
    Type type() const override;

    void addTspan(std::shared_ptr<LSvgTspan> tspan) {m_tspans.push_back(tspan);}
    const std::list<std::shared_ptr<LSvgTspan> > tspans() const { return m_tspans; }
    void addText(const std::string &text);
    void addLineBreak() {m_tspans.push_back(LINEBREAK);}
    void setWhitespaceMode(WhitespaceMode mode) {m_mode = mode;}

    LRect<double> internalFastBounds(LPainter *p, LSvgExtraStates &states) const override;
    LRect<double> internalBounds(LPainter *p, LSvgExtraStates &states) const override;

    LPoint<double> position() const { return m_coord; }
    LSize<double> size() const { return m_size; }
    WhitespaceMode whitespaceMode() const { return m_mode; }

private:
    void drawHelper(LPainter *p, LSvgExtraStates &states, LRect<double> *boundingRect = nullptr) const;

    static std::shared_ptr<LSvgTspan> LINEBREAK;

    LPoint<double> m_coord;

    // 'm_tspans' is also used to store characters outside tspans and line breaks.
    // If a 'm_tspan' item is null, it indicates a line break.
    std::list<std::shared_ptr<LSvgTspan> > m_tspans;

    Type m_type;
    LSize<double> m_size;
    WhitespaceMode m_mode;
};

class LKSVGDEF LSvgTspan : public LSvgNode
{
public:
    // tspans are also used to store normal text, so the 'isProperTspan' is used to separate text from tspan.
    LSvgTspan(std::shared_ptr<LSvgNode> parent, bool isProperTspan = true)
        : LSvgNode(parent), m_mode(LSvgText::Default), m_isTspan(isProperTspan)
    {
    }
    Type type() const override { return Tspan; }
    void drawCommand(LPainter *, LSvgExtraStates &) override { lWarn() << "Tspans should be drawn through LSvgText::draw()."; }
    void addText(const std::string &text) {m_text += text;}
    const std::string &text() const {return m_text;}
    bool isTspan() const {return m_isTspan;}
    void setWhitespaceMode(LSvgText::WhitespaceMode mode) {m_mode = mode;}
    LSvgText::WhitespaceMode whitespaceMode() const {return m_mode;}
private:
    std::string m_text;
    LSvgText::WhitespaceMode m_mode;
    bool m_isTspan;
};

class LSvgUse : public LSvgNode
{
public:
    LSvgUse(const LPoint<double> &start, std::shared_ptr<LSvgNode> parent, std::shared_ptr<LSvgNode> link);
    LSvgUse(const LPoint<double> &start, std::shared_ptr<LSvgNode> parent, const std::string &linkId);

    void drawCommand(LPainter *p, LSvgExtraStates &states) override;
    Type type() const override;
    LRect<double> internalBounds(LPainter *p, LSvgExtraStates &states) const override;
    LRect<double> decoratedInternalBounds(LPainter *p, LSvgExtraStates &states) const override;
    bool isResolved() const { return m_link != nullptr; }
    const std::string &linkId() const { return m_linkId; }
    void setLink(std::shared_ptr<LSvgNode> link) { m_link = link; }
    std::shared_ptr<LSvgNode> link() const { return m_link; }
    LPoint<double> start() const { return m_start; }
    bool isRecursing() const { return m_recursing; }

private:
    std::shared_ptr<LSvgNode> m_link;
    LPoint<double>   m_start;
    std::string   m_linkId;
    mutable bool m_recursing;
};

class LSvgVideo : public LSvgNode
{
public:
    void drawCommand(LPainter *, LSvgExtraStates &) override {}
    Type type() const override;
};


LK_NAMESPACE_END

#endif // LSVGGRAPHICS_H
