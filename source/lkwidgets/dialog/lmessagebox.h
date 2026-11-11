#ifndef LMESSAGEBOX_H
#define LMESSAGEBOX_H

#include "lkwidgets/dialog/ldialog.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LMessageBox : public LDialog
{
public:
    LMessageBox(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0);

protected:
    // virtual std::shared_ptr<LPlatformDialogHelper> platformHelper() const override;
};


LK_NAMESPACE_END

#endif // LMESSAGEBOX_H
