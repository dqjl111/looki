#ifndef LBRUSH_H
#define LBRUSH_H
#include "lkgui/paint/ltransform2d.h"
#include "lkgui/paint/lcolor.h"
#include "lkgui/paint/lgradient.h"
#include "lkgui/image/limage.h"
#include "lkgui/image/lpixmap.h"

LK_NAMESPACE_BEGIN
class LKGUIDEF LBrush : public LIVar
{
public:
    enum Style { // brush style
        NoPattern,
        SolidPattern,
        TexturePattern,
        GradientPattern
    };

    LBrush(LBrush* b = nullptr);
    LBrush(const LColor &color, Style bs=SolidPattern);
    LBrush(const std::shared_ptr<LImage> image);
    LBrush(const std::shared_ptr<LPixmap> pm);
    LBrush(const std::shared_ptr<LGradient> g);

    std::shared_ptr<LBrush> backBrush();

    LBrush& operator=(const LBrush &b);

    bool operator==(const LBrush &b) const;
    inline bool operator!=(const LBrush &b) const { return !(operator==(b)); }

    bool isOpaque() const;

    std::shared_ptr<LPixmap> texture() const;
    std::shared_ptr<LImage> textureImage() const;
    std::shared_ptr<LGradient> gradient() const;

    void setPixmap(std::shared_ptr<LPixmap> pm);
    void setImage(std::shared_ptr<LImage> img);
    void setGradient(std::shared_ptr<LGradient> g);

    Style m_style = NoPattern;
    LColor m_color;
    LTransform2D m_transform;

protected:
    std::shared_ptr<LGradient> m_gradient = nullptr;
    std::shared_ptr<LPixmap> m_pixmap = nullptr;
    std::shared_ptr<LImage> m_image = nullptr;
    bool m_hasPixmapTexture = false;
};

LK_NAMESPACE_END

#endif // LBRUSH_H
