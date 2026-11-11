#include "luuid.h"
#include <random>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "lkcore/lstringutil.h"

LK_NAMESPACE_BEGIN

#ifndef SCNx8
#define SCNx8        "hhx"
#endif
#ifndef SCNx16
#define SCNx16       "hx"
#endif
#ifndef SCNx32
#define SCNx32       "x"
#endif


bool LUuid::operator == (const LUuid & guid2) const
{
    return memcmp(&guid2, this, sizeof(LUuid)) == 0;
}

bool LUuid::operator != (const LUuid & guid2) const
{
    return !(*this == guid2);
}

bool LUuid::operator < (const LUuid &guid2) const
{
    return memcmp(this, &guid2, sizeof(LUuid)) < 0;
}

bool LUuid::operator > (const LUuid &guid2) const
{
    return memcmp(this, &guid2, sizeof(LUuid)) > 0;
}

LUuid::LUuid (const std::string &uuidString)
    : LUuid(uuidString.c_str())
{
}

LUuid::LUuid (const char *uuidString)
{
    if (uuidString == nullptr)
    {
        // Special case (and prevents random bugs)
        memset(this, 0, sizeof(LUuid));
        return;
    }

    if (uuidString[0] == '{')
    {
        sscanf(uuidString, "{%08" SCNx32 "-%04" SCNx16 "-%04" SCNx16 "-%02" SCNx8 "%02" SCNx8 "-%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "}", &Uuid.data1, &Uuid.data2, &Uuid.data3, &Uuid.data4[0], &Uuid.data4[1], &Uuid.data4[2], &Uuid.data4[3], &Uuid.data4[4], &Uuid.data4[5], &Uuid.data4[6], &Uuid.data4[7]);
    }
    else
    {
        sscanf(uuidString, "%08" SCNx32 "-%04" SCNx16 "-%04" SCNx16 "-%02" SCNx8 "%02" SCNx8 "-%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "", &Uuid.data1, &Uuid.data2, &Uuid.data3, &Uuid.data4[0], &Uuid.data4[1], &Uuid.data4[2], &Uuid.data4[3], &Uuid.data4[4], &Uuid.data4[5], &Uuid.data4[6], &Uuid.data4[7]);
    }
}

std::string LUuid::toString(bool withBraces) const
{
    char buffer[39];
    sprintf(buffer, "%s%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X%s", withBraces ? "{" : "", Uuid.data1, Uuid.data2, Uuid.data3, Uuid.data4[0], Uuid.data4[1], Uuid.data4[2], Uuid.data4[3], Uuid.data4[4], Uuid.data4[5], Uuid.data4[6], Uuid.data4[7], withBraces ? "}" : "");
    return buffer;
}

LUuid LUuid::fromString(const char *uuidString)
{
    LUuid temp(uuidString);
    return temp;
}

LUuid LUuid::fromString(const std::string &uuidString)
{
    LUuid temp(uuidString.c_str());
    return temp;
}

LUuid LUuid::generatev4()
{
    // Mach-O does not support TLS and clang still has issues with thread_local
#if !defined(__APPLE__) && !defined(__clang__)
    thread_local std::random_device rd;
    thread_local auto gen = std::mt19937_64(rd());
#else
    std::random_device rd;
    std::mt19937_64 gen(rd());
#endif
    std::uniform_int_distribution<uint64_t> dis64;

    LUuid newGuid;
    newGuid.wideIntegers[0] = dis64(gen);
    newGuid.wideIntegers[1] = dis64(gen);

    // RFC4122 defines (psuedo)random uuids (in big-endian notation):
    // MSB of DATA4[0] specifies the variant and should be 0b10 to indicate standard uuid,
    // and MSB of DATA3 should be 0b0100 to indicate version 4
    newGuid.Bytes.data4[0] = (newGuid.Bytes.data4[0] & 0x3F) | static_cast<uint8_t>(0x80);
    newGuid.Bytes.data3[1] = (newGuid.Bytes.data3[1] & 0x0F) | static_cast<uint8_t>(0x40);

    return newGuid;
}
LK_NAMESPACE_END
