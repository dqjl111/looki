#ifndef LUUID_H
#define LUUID_H
#include "lkcore/lookiconfig.h"
#include "ldebug.h"

LK_NAMESPACE_BEGIN

union LKCOREDEF LUuid
{
private:
    static LUuid generatev4();

public:
    uint64_t wideIntegers[2];
    struct _internalData
    {
        uint32_t data1;
        uint16_t  data2;
        uint16_t data3;
        uint8_t data4[8];
    } Uuid;
    struct _byteRepresentation
    {
        uint8_t data1[4];
        uint8_t data2[2];
        uint8_t data3[2];
        uint8_t data4[8];
    } Bytes;

    bool operator == (const LUuid &guid2) const;
    bool operator != (const LUuid &guid2) const;
    bool operator < (const LUuid &guid2) const;
    bool operator > (const LUuid &guid2) const;

    LUuid() = default;

    LUuid(const char *uuidString);
    LUuid(const std::string &uuidString);
    static LUuid fromString(const char *uuidString);
    static LUuid fromString(const std::string &uuidString);

    static inline LUuid createUuid()
    {
        return generatev4();
    }

    std::string toString(bool withBraces = true) const;
};

LK_NAMESPACE_END
#endif // LUUID_H
