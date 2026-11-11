#ifndef LSTYLE_H
#define LSTYLE_H
#include "lkcore/livar.h"
#include "lkgui/lrect.h"
#include "lkcore/lvariant.h"
#include "lkgui/image/ltexture.h"
#include "lkgui/lpalette.h"
#include "lkwidgets/draw/lstyleopt.h"
#include "lkwidgets/core/lsizepolicy.h"
#include <map>

LK_NAMESPACE_BEGIN
class LFrame;
class LWidget;
class LPainter;
class LActionBase;
class LFrameData;
class LLabelData;
class LSliderData;
class LPalette;
class LSpinBoxData;

class LKWIDGETSDEF LStyle : public LIVar
{
public:
    LStyle();

    virtual void drawFrame(std::shared_ptr<LWidget> w, LPainter* p, const LFrameData& f, bool fill =false);
    virtual void drawFrame(const LRect<int>& fRect,
                           LPainter* p,
                           std::shared_ptr<LPalette> palette,
                           LPalette::ColorRole foregroundRole=LPalette::WindowText,
                           int shape=0, int shadow=0, int lineWidth=0, int midLineWidth=0,
                           std::shared_ptr<LBrush> fill = nullptr);

    LRect<double> labelIconRect(const LLabelData& f, LRect<int>& fRect,
                                bool isImageVertical = false, const LWidgetStyle* ele = nullptr) const;
    LPoint<double> labelIconPos(const LLabelData& f, const LRect<double>& iconRect) const;

    virtual void drawLabel(std::shared_ptr<LWidget> w,
                           LPainter* p,
                           const LLabelData& f,
                           LDrawMark mark);

    virtual void drawLabel(const LLabelData& f,
                           LRect<int> fRect,
                           LPainter* p,
                           bool isEnable,
                           LDrawMark mark,
                           const LWidgetStyle* ele = nullptr);

    virtual void drawMenuItem(std::shared_ptr<LWidget> w,
                              std::shared_ptr<LActionBase> a,
                              LPainter* p,
                              const LStyleOpt& opt,
                              int maxIconWidth = 0,
                              int tabWidth = 0);

    virtual void drawScroll(std::shared_ptr<LWidget> w,
                            const LSliderData& f,
                            LPainter* p,
                            const LStyleOptComplex& opt);

    virtual void drawScroll(const LSliderData& f,
                            LPainter* p,
                            const LStyleOptComplex& opt);

    virtual void drawPrimitive(std::shared_ptr<LWidget> w,
                              const LRect<int>& fRect,
                              LPainter* p,
                              LUiElementType type);

    virtual void drawPrimitive(const LRect<int>& fRect,
                               LPainter* p,
                               LUiElementType type,
                               uint16_t state,
                               std::shared_ptr<LPalette> palette);

    LUiSubCtrl hitTestSlider(LSliderData& f,
                           const LStyleOptComplex& opt,
                           const LPoint<int> &pt,
                           uint8_t tickPosition =0,
                           const LWidgetStyle* ele = nullptr) const;


    int sliderCtrlThickness(const LOrientation orient,
                            const LRect<int>& rect,
                            const uint8_t tickPosition,
                            const LWidgetStyle* ele = nullptr) const;

    int sliderTickOffset(const LOrientation orient,
                         const LRect<int>& rect,
                         const uint8_t tickPosition,
                         int ctrlThickness=-1,
                         const LWidgetStyle* ele = nullptr) const;

    int sliderSpaceAvailable(const LOrientation orient, const LRect<int>& rect, const LWidgetStyle* ele = nullptr) const;

    LRect<int> sliderGrooveRect(const LOrientation orient,
                                const LStyleOptComplex& rect,
                                const uint8_t tickPosition,
                                int ctrlThickness=-1,
                                int tickOffset=-1) const;

    LRect<int> sliderHandleRect(const LSliderData& f,
                                const LStyleOptComplex& opt,
                                const uint8_t tickPosition,
                                int ctrlThickness=-1,
                                int tickOffset=-1) const;

    LRect<int> sliderSubCtrlRect(const LSliderData& f,
                                 const LStyleOptComplex& opt,
                                 LUiSubCtrl sc,
                                 const uint8_t tickPosition,
                                 int ctrlThickness=-1,
                                 int tickOffset=-1) const;

    int scrollMaxLen(const LSliderData& f, const LStyleOptComplex& opt, int sbextent =-1) const;
    int scrollSliderLen(const LSliderData& f, const LStyleOptComplex& opt, int sbextent=-1, int maxLen=-1) const;
    int scrollSliderStart(const LSliderData& f,
                          const LStyleOptComplex& opt,
                          int sbextent=-1,
                          int maxLen=-1,
                          int sliderLen = -1) const;

    LRect<int> scrollSubCtrlRect(const LSliderData& f,
                                 const LStyleOptComplex& opt,
                                 LUiSubCtrl sc,
                                 int sbextent=-1,
                                 int maxLen=-1,
                                 int sliderLen = -1,
                                 int sliderStart = -1) const;

    LRect<int> spinBoxSubCtrlRect(const LStyleOptComplex& opt,
                                   LUiSubCtrl sc,
                                   int editButtonLen,
                                   bool isSpinButtonSameSide = true);

    LRect<int> comboEditSubCtrlRect(const LStyleOptComplex& opt,
                                   LUiSubCtrl sc,
                                   int editButtonLen);

    LRect<int> groupBoxSubCtrlRect(const LStyleOptGroupBox& opt, LUiSubCtrl sc, const std::wstring& title);

    LUiSubCtrl hitTestSpinBox(const LStyleOptComplex& opt, bool isButtonSameSide, const LPoint<int> &pt);
    LUiSubCtrl hitTestComboEdit(const LStyleOptComplex& opt,const LPoint<int> &pt);
    LUiSubCtrl hitTestGroupBox(const LStyleOptGroupBox& opt,const LPoint<int> &pt, const std::wstring& title);

    LUiSubCtrl hitTestScroll(LSliderData& f,
                             const LStyleOptComplex& opt,
                             const LPoint<int> &pt) const;

    static LUiElementType scrollSubCtrlUiElement(uint8_t scId, LOrientation orient);
    static LUiElementType spinBoxSubCtrlUiElement(uint8_t scId);
    static LUiElementType comboEditSubCtrlUiElement(uint8_t scId);

    static int sliderPositionFromValue(int min, int max, int val, int space,
                                       bool upsideDown = false);
    static int sliderValueFromPosition(int min, int max, int pos, int space,
                                       bool upsideDown = false);


    std::shared_ptr<LPainterPath> markPath(const LRect<double>& rect, LDrawMark mark);
    void drawMark(LPainter* p, const LRect<double>& rect, LDrawMark mark,
                  const LWidgetStyle* ele = nullptr, uint16_t state= (uint16_t)LUiState::None);
    uint16_t markState(LDrawMark mark);
    LUiElementType markElementType(LDrawMark mark);


    double boundWidth(LSizePolicy::ControlType type,
                     LDirection dir = LDirection::ToLeft,
                     int flags = LRenderRule::All,
                     const LWidgetStyle* ele = nullptr);

    LMargins<double> boundWidths(LSizePolicy::ControlType type, int flags = LRenderRule::All,const LWidgetStyle* ele = nullptr);

    uint16_t get(uint16_t LStylePixelMetric::* k, const LWidgetStyle* ele = nullptr) const{
        if(ele != nullptr){
            bool hasValue = false;
            uint16_t ret = ele->get(k, &hasValue);
            if(hasValue){
                return ret;
            }
        }
        return m_pixMetric.*k;
    }


    template<typename T>
    T get(T LMathMLStyle::* k) const{
        return m_mathMLStyle.*k;
    }


    template<typename T>
    void set(T LStylePixelMetric::* k, uint16_t v){
        m_pixMetric.*k = v;
    }

    template<typename T>
    void set(T LMathMLStyle::* k, T v){
        m_mathMLStyle.*k = v;
    }

    inline bool getBit(LStyleBit bit){
        return m_styleBits.test((size_t)bit);
    }

    inline void setBit(LStyleBit bit, bool v){
        m_styleBits.set((size_t)bit, v);
    }
protected:
    std::bitset<(int)LStyleBit::Num> m_styleBits;
    LStylePixelMetric m_pixMetric;
    LStyleBoundMetric m_boundMetric;
    LMathMLStyle m_mathMLStyle;
};

LK_NAMESPACE_END

#endif // LSTYLE_H
