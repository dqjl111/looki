#ifndef LSVGCSSHANDLER_H
#define LSVGCSSHANDLER_H

#include "lsvghelper.h"
#include "lkgui/text/lcssparser.h"

LK_NAMESPACE_BEGIN

class LSvgCssAnimation;
class LKSVGDEF LSvgCssHandler
{
public:
    LSvgCssHandler() = default;

    std::shared_ptr<LSvgCssAnimation> createAnimation(const std::string &name);
    void collectAnimations(const std::shared_ptr<lcss::StyleSheet> sheet);

private:
    // void updateColorProperty(const QCss::Declaration &decl, QSvgAnimatedPropertyColor *property);
    // void updateTransformProperty(const QCss::Declaration &decl, QSvgAnimatedPropertyTransform *property);

private:
    // std::unordered_map<std::string, QCss::AnimationRule> m_animations;
    std::unordered_map<std::string, std::shared_ptr<LSvgCssAnimation>> m_cachedAnimations;
};


LK_NAMESPACE_END

#endif // LSVGCSSHANDLER_H
