#ifndef LTEXTURE_H
#define LTEXTURE_H

#include "lkgui/image/lpixmap.h"
#include "lkgui/paint/lgradient.h"

LK_NAMESPACE_BEGIN
class LKGUIDEF LTexturePixmaps : public LIVar
{
public:
    LTexturePixmaps(){}

    std::string m_path;

    std::shared_ptr<LPixmap> m_normal;
    std::shared_ptr<LPixmap> m_disabled;
    std::shared_ptr<LPixmap> m_select;
    std::shared_ptr<LGradient> m_gradient;
};

class LWindow;
class LPainter;
class LKGUIDEF LTexture : public LIVar
{
public:
    LTexture();
    LTexture(std::shared_ptr<LPixmap> pix, uint16_t partId=0, uint16_t rowNum=1, uint16_t colNum=1);
    LTexture(std::shared_ptr<LImage> img, uint16_t partId=0, uint16_t rowNum=1, uint16_t colNum=1);
    LTexture(const std::string& path, uint16_t partId=0, uint16_t rowNum=1, uint16_t colNum=1);
    LTexture(std::shared_ptr<LTexturePixmaps> pixmaps, uint16_t partId=0, uint16_t rowNum=1, uint16_t colNum=1);


    enum Type{
        Normal,
        Disable,
        Select
    };

    inline bool isEmpty() const{
        return m_pixmaps == nullptr ||
               ( (m_pixmaps->m_normal == nullptr || m_rect.isEmpty())
                  && (m_pixmaps->m_gradient == nullptr || m_pixmaps->m_gradient->m_type == LGradient::Type::No) );
    }

    inline const LRect<int>& rect() const{
        return m_rect;
    }

    void setRect(const LRect<int>& in);

    inline LSize<int> size() const{
        return LSize<int>(m_rect.width(), m_rect.height());
    }

    inline int width() const{
        return m_rect.width();
    }

    inline int height() const{
        return m_rect.height();
    }

    uint16_t inline partId() const{
        return m_partId;
    }

    uint16_t inline rowNum() const{
        return m_rowNum;
    }

    uint16_t inline colNum() const{
        return m_colNum;
    }

    void setPixmapSize(const LSize<int>& s);

    void setPixmap(std::shared_ptr<LPixmap> pix, uint16_t partId=0, uint16_t rowNum=1, uint16_t colNum=1);
    void setImage(std::shared_ptr<LImage> image, uint16_t partId=0, uint16_t rowNum=1, uint16_t colNum=1);
    void setImage(const std::string& path, uint16_t partId=0, uint16_t rowNum=1, uint16_t colNum=1);
    void setTexturePixmaps(std::shared_ptr<LTexturePixmaps> pixmaps,
                           uint16_t partId=0,
                           uint16_t rowNum=1,
                           uint16_t colNum=1);

    void switchPart(uint16_t partId, uint16_t rowNum=0, uint16_t colNum=0);
    std::shared_ptr<LTexture> switchFromThis(uint16_t partId, uint16_t rowNum=0, uint16_t colNum=0);
    std::shared_ptr<LTexture> switchSizeFromThis(const LSize<int>& s);

    void toNext();
    void toPrevious();

    inline bool isLast(){
        return m_partId >= m_rowNum*m_colNum;
    }

    inline bool isFirst(){
        return m_partId = 0;
    }

    std::shared_ptr<LPixmap> toPixmap();
    bool isPartValid();
    bool isSplitWithPart();


    static void removePixmaps(const std::string& path);
    static void removePixmaps(const std::string& path, const LSize<int>& s);

    static std::shared_ptr<LTexture> getTexture(const std::string& path,
                                                uint16_t partId = 0,
                                                uint16_t rowNum=0,
                                                uint16_t colNum=0);

    static std::shared_ptr<LTexture> getTexture(const std::string& path,
                                                const LSize<int>& s,
                                                uint16_t partId = 0,
                                                uint16_t rowNum=0,
                                                uint16_t colNum=0);

    static std::shared_ptr<LTexturePixmaps> getPixmaps(const std::string& path);
    static std::shared_ptr<LTexturePixmaps> getPixmaps(const std::string& path, const LSize<int>& s);

    static bool registPixmaps(const std::string& path, std::shared_ptr<LTexturePixmaps> pixmaps);
    static bool registPixmaps(const std::string& path, const LSize<int>& s, std::shared_ptr<LTexturePixmaps> pixmaps);


    std::shared_ptr<LPixmap> getPixmap(Type type, bool checkRect = false) const;
    std::shared_ptr<LPixmap> getPixmap(const LSize<int> &s,
                                       Type mode = Normal,
                                       bool checkRect = false) const;
    std::shared_ptr<LPixmap> getPixmap(std::shared_ptr<LWindow> window,
                                       const LSize<int> &s,
                                       Type mode = Normal,
                                       bool checkRect = false) const;
    std::shared_ptr<LPixmap> getPixmap(std::shared_ptr<LPaintDevice> dev,
                                       const LSize<int> &s,
                                       Type mode = Normal,
                                       bool checkRect = false) const;
    std::shared_ptr<LPixmap> getPixmap(double devicePixelRatio,
                                       const LSize<int> &s,
                                       Type mode = Normal,
                                       bool checkRect = false) const;



    std::shared_ptr<LPixmap> getNormal(bool checkRect = false) const;
    std::shared_ptr<LPixmap> getDisabled(bool checkRect = false) const;
    std::shared_ptr<LPixmap> getSelect(bool checkRect = false) const;


    double devicePixelRatio() const;
    void setDevicePixelRatio(double scaleFactor);

    LSize<int> actualSize(const LSize<int> &size) const;
    LSize<int> actualSize(std::shared_ptr<LWindow> window, const LSize<int> &size) const;

    bool isWholePixmap(Type mode = Normal);
    std::string registPath(){
        return m_pixmaps!= nullptr ? m_pixmaps->m_path : "";
    }

    void paint(LPainter *painter,
               const LRect<int> &rect,
               LFlags<LAlignment> alignment = LAlignment::Center,
               Type mode = Normal) const;

    const inline bool hasGradient() const{
        return m_pixmaps!= nullptr ? m_pixmaps->m_gradient != nullptr : false;
    }

    std::shared_ptr<LGradient> getGradient() const{
        return m_pixmaps->m_gradient;
    }

    inline const bool isGradientUse() const{
        return (m_pixmaps == nullptr)
               && (m_pixmaps->m_normal == nullptr || m_rect.isEmpty())
               && (m_pixmaps->m_gradient != nullptr && m_pixmaps->m_gradient->m_type != LGradient::Type::No);
    }
protected:
    std::shared_ptr<LTexturePixmaps> m_pixmaps;

    LRect<int> m_rect;
    uint16_t m_partId = 0;
    uint16_t m_rowNum = 1;
    uint16_t m_colNum = 1;

    void initRect();

    static std::unordered_map<std::string, std::shared_ptr<LTexturePixmaps> > m_cachedPixmaps;
    static std::unordered_map<std::string, std::shared_ptr<LTexturePixmaps> > m_cachedSizedPixmaps;

    static std::string sizedKey(const std::string& path, const LSize<int>& s){
        return std::to_string(s.m_x) + "," + std::to_string(s.m_y) + ":" + path;
    }

    static std::shared_ptr<LGradient> parseGradient(const std::string& s);

};

LK_NAMESPACE_END
#endif // LTEXTURE_H
