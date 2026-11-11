#include "lplatformtheme.h"

LK_NAMESPACE_BEGIN

LPlatformTheme::LPlatformTheme()
{

}

LVariant LPlatformTheme::defaultThemeHint(Hint hint)
{
    switch (hint) {
    case Hint::CursorFlashTime:{
        return LVariant(1000);
    }
    case Hint::KeyboardInputInterval:{
        return LVariant(400);
    }
    case Hint::KeyboardAutoRepeatRate:{
        return LVariant(30);
    }
    case Hint::MouseDoubleClickInterval:{
        return LVariant(400);
    }
    case Hint::StartDragDistance:{
        return LVariant(10);
    }
    case Hint::StartDragTime:{
        return LVariant(500);
    }
    case Hint::StartDragVelocity:{
        return LVariant(int(0)); // no limit
    }
    case Hint::UseFullScreenForPopup:{
#ifdef _WIN32
        return LVariant(true);
#else
        return LVariant(false);
#endif
    }
    case Hint::WindowAutoPlacement:{
        return LVariant(false);
    }
    case Hint::ShowShortcutsInContextMenus:{
        return LVariant(true);
    }
    case Hint::TextCursorWidth:{
        return LVariant(1);
    }
    case Hint::DropShadow:{
        return LVariant(false);
    }
    case Hint::MaximumScrollBarDragDistance:{
        return LVariant(-1);
    }
    case Hint::KeyboardScheme:{
#ifdef _WIN32
        return LVariant(int(KeyboardScheme::Windows));
#else
        return LVariant(int(KeyboardScheme::Kde));
#endif
    }
    case Hint::UiEffects:{
        return LVariant(int(0));
    }
    case Hint::DialogSnapToDefaultButton:
    case Hint::ContextMenuOnMouseRelease:{
        return LVariant(false);
    }
    case Hint::MousePressAndHoldInterval:{
        return LVariant(800);
    }
    case Hint::MouseDoubleClickDistance:{
        return LVariant(5);
    }
    case Hint::WheelScrollLines:{
        return LVariant(3);
    }
    case Hint::TouchDoubleTapDistance:{
        return LVariant(10);
    }
    case Hint::MouseQuickSelectionThreshold:{
        return LVariant(10);
    }
    case Hint::ShowIsFullScreen:{
        return LVariant(false);
    }
    case Hint::ShowIsMaximized:{
        return LVariant(false);
    }
    }
    return LVariant();
}


LK_NAMESPACE_END
