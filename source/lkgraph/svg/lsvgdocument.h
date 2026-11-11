#ifndef LSVGDOCUMENT_H
#define LSVGDOCUMENT_H
#include "lkgraph/svg/lsvgstructure.h"
#include "lkgraph/svg/lsvganimator.h"

LK_NAMESPACE_BEGIN

class LKSVGDEF LSvgDocument : public LSvgStructure
{
public:
    LSvgDocument(LFlags<LSvgOption> options = {}, LSvgAnimatorType type = LSvgAnimatorType::Automatic);
    ~LSvgDocument();

    static std::shared_ptr<LSvgDocument> load(const std::string &file, LFlags<LSvgOption> options = {},
                                              LSvgAnimatorType type = LSvgAnimatorType::Automatic);
    static std::shared_ptr<LSvgDocument> load(const std::wstring &file, LFlags<LSvgOption> options = {},
                                              LSvgAnimatorType type = LSvgAnimatorType::Automatic);
    static std::shared_ptr<LSvgDocument> loadContent(const std::string &content, LFlags<LSvgOption> options = {},
                                                     LSvgAnimatorType type = LSvgAnimatorType::Automatic);

    Type type() const override;

    inline LSize<int> size() const;
    void setWidth(int len, bool percent);
    void setHeight(int len, bool percent);
    inline int width() const;
    inline int height() const;
    inline bool widthPercent() const;
    inline bool heightPercent() const;

    inline bool preserveAspectRatio() const;
    void setPreserveAspectRatio(bool on);

    inline const LRect<double>& viewBox() const;
    void setViewBox(const LRect<double> &rect);

    LFlags<LSvgOption> options() const;

    void drawCommand(LPainter *, LSvgExtraStates &) override;

    void draw(LPainter *p);
    void draw(LPainter *p, const LRect<double> &bounds);
    void draw(LPainter *p, const std::string &id,
              const LRect<double> &bounds=LRect<double>());

    LTransform2D transformForElement(const std::string &id) const;
    LRect<double> boundsOnElement(const std::string &id) const;
    bool   elementExists(const std::string &id) const;

    void addSvgFont(std::shared_ptr<LSvgFont> );
    std::shared_ptr<LSvgFont> svgFont(const std::string &family) const;
    void addNamedNode(const std::string &id, std::shared_ptr<LSvgNode>node);
    std::shared_ptr<LSvgNode>namedNode(const std::string &id) const;
    void addNamedStyle(const std::string &id, std::shared_ptr<LSvgPaintStyleProperty> style);
    std::shared_ptr<LSvgPaintStyleProperty> namedStyle(const std::string &id) const;

    void restartAnimation();
    inline int64_t currentElapsed() const;
    bool animated() const;
    void setAnimated(bool a);
    inline int64_t animationDuration() const;
    int currentFrame() const;
    void setCurrentFrame(int);
    void setFramesPerSecond(int num);

    std::shared_ptr<LSvgAbstractAnimator> animator() const;

private:
    void mapSourceToTarget(LPainter *p, const LRect<double> &targetRect, const LRect<double> &sourceRect = LRect<double>());
private:
    LSize<int>  m_size;
    bool   m_widthPercent;
    bool   m_heightPercent;

    mutable bool m_implicitViewBox = true;
    mutable LRect<double> m_viewBox;
    bool m_preserveAspectRatio = false;

    std::unordered_map<std::string, std::shared_ptr<LSvgFont>> m_fonts;
    std::unordered_map<std::string, std::shared_ptr<LSvgNode>> m_namedNodes;
    std::unordered_map<std::string, std::shared_ptr<LSvgPaintStyleProperty> > m_namedStyles;

    bool  m_animated;
    int   m_fps;

    LSvgExtraStates m_states;

    const LFlags<LSvgOption> m_options;
    std::shared_ptr<LSvgAbstractAnimator> m_animator;
};


inline LSize<int> LSvgDocument::size() const
{
    if (m_size.isEmpty()){
        return viewBox().size().toSize();
    }
    if (m_widthPercent || m_heightPercent) {
        const int width = m_widthPercent ? std::lround(0.01 * m_size.m_x * viewBox().m_width) : m_size.m_x;
        const int height = m_heightPercent ? std::lround(0.01 * m_size.m_y * viewBox().m_height) : m_size.m_y;
        return LSize<int>(width, height);
    }
    return m_size;
}

inline int LSvgDocument::width() const
{
    return size().width();
}

inline int LSvgDocument::height() const
{
    return size().height();
}

inline bool LSvgDocument::widthPercent() const
{
    return m_widthPercent;
}

inline bool LSvgDocument::heightPercent() const
{
    return m_heightPercent;
}

inline const LRect<double>& LSvgDocument::viewBox() const
{
    if (m_viewBox.isNull()) {
        m_viewBox = bounds();
        m_implicitViewBox = true;
    }

    return m_viewBox;
}

inline bool LSvgDocument::preserveAspectRatio() const
{
    return m_preserveAspectRatio;
}

inline int64_t LSvgDocument::currentElapsed() const
{
    return m_animator->currentElapsed();
}

inline int64_t LSvgDocument::animationDuration() const
{
    return m_animator->animationDuration();
}



LK_NAMESPACE_END

#endif // LSVGDOCUMENT_H
