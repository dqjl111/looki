#ifndef LSQLDRIVERPLUGIN_H
#define LSQLDRIVERPLUGIN_H

#include "lkcore/plugin/lplugin.h"
#include "lksql/lsqldriver.h"

LK_NAMESPACE_BEGIN

class LKSQLDEF LSqlDriverPlugin : public LPlugin
{
public:
    LSqlDriverPlugin();

    virtual std::shared_ptr<LSqlDriver> create() = 0;
};


LK_NAMESPACE_END

#endif // LSQLDRIVERPLUGIN_H
