#ifndef LSTYLEOPT_H
#define LSTYLEOPT_H
#include "lkwidgets/draw/lwidgetstyle.h"

LK_NAMESPACE_BEGIN
class LPalette;


class LKWIDGETSDEF LStyleOpt
{
public:
    LStyleOpt();

    LFlags<LUiState> state = LUiState::None;
    LLayoutDirection direction = LLayoutDirection::LeftToRight;
    LRect<int> rect;
    std::shared_ptr<LPalette> palette = nullptr;
    LPalette::ColorRole foregroundRole = LPalette::NoRole;
    LPalette::ColorRole backgroundRole = LPalette::NoRole;
    const LWidgetStyle* elementStyle = nullptr;
};


class LKWIDGETSDEF LStyleOptComplex : public LStyleOpt
{
public:
    LStyleOptComplex();
    LUiSubCtrl subCtrls;
    LUiSubCtrl activeSubCtrls;
};


class LKWIDGETSDEF LStyleOptGroupBox : public LStyleOptComplex
{
public:
    LStyleOptGroupBox();
    std::shared_ptr<LFont> font;
    LFlags<LAlignment> titleAlignment;
};
LK_NAMESPACE_END

#endif // LSTYLEOPT_H
