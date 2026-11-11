#include "lclipboard.h"
#include "lplatformclipboard.h"
#include "lguiapplication.h"

LK_NAMESPACE_BEGIN

LClipboard::LClipboard()
{

}

std::wstring LClipboard::text() const
{
    const std::shared_ptr<LPlatformClipboard> clipboard = LGuiApplication::platformIntegration()->clipboard();
    if(clipboard != nullptr){
        return clipboard->text();
    }
    return L"";
}


void LClipboard::setText(const std::wstring &s)
{
    const std::shared_ptr<LPlatformClipboard> clipboard = LGuiApplication::platformIntegration()->clipboard();
    if(clipboard != nullptr){
        clipboard->setText(s);
    }
}

LCbor LClipboard::cbor(const std::string typeName)
{
    const std::shared_ptr<LPlatformClipboard> clipboard = LGuiApplication::platformIntegration()->clipboard();
    if(clipboard != nullptr){
        return clipboard->cbor(typeName);
    }
    return LCbor();
}

void LClipboard::setCbor(const LCbor &c, const std::string typeName)
{
    const std::shared_ptr<LPlatformClipboard> clipboard = LGuiApplication::platformIntegration()->clipboard();
    if(clipboard != nullptr){
        clipboard->setCbor(c, typeName);
    }
}

std::shared_ptr<LImage> LClipboard::image() const
{
    const std::shared_ptr<LPlatformClipboard> clipboard = LGuiApplication::platformIntegration()->clipboard();
    if(clipboard != nullptr){
        return clipboard->image();
    }
    return nullptr;
}


void LClipboard::setImage(const std::shared_ptr<LImage> p)
{
    const std::shared_ptr<LPlatformClipboard> clipboard = LGuiApplication::platformIntegration()->clipboard();
    if(clipboard != nullptr){
        clipboard->setImage(p);
    }
}

std::shared_ptr<LPixmap> LClipboard::pixmap() const
{
    std::shared_ptr<LImage> t = image();
    if(t != nullptr){
        return LPixmap::fromImage(t);
    }
    return nullptr;
}

void LClipboard::setPixmap(const std::shared_ptr<LPixmap> p)
{
    if(p != nullptr){
        setImage(p->toImage());
    }
}

void LClipboard::clear()
{
    const std::shared_ptr<LPlatformClipboard> clipboard = LGuiApplication::platformIntegration()->clipboard();
    if(clipboard != nullptr){
        clipboard->clear();
    }
}


LK_NAMESPACE_END
