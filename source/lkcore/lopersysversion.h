#ifndef LOPERSYSVERSION_H
#define LOPERSYSVERSION_H
#include <cmath>
#include "lkcore/lookiconfig.h"

LK_NAMESPACE_BEGIN

class LKCOREDEF LOperSysVersion
{
public:
    LOperSysVersion();

    enum OSType {
        Unknown = 0,
        Windows,
    };

    OSType m_os;
    int m_major;
    int m_minor;
    int m_micro;


    static const LOperSysVersion Windows7;
    static const LOperSysVersion Windows8;
    static const LOperSysVersion Windows8_1;
    static const LOperSysVersion Windows10;

    LOperSysVersion(OSType osType, int vmajor, int vminor = -1, int vmicro = -1)
        : m_os(osType),
        m_major(std::max<int>(-1, vmajor)),
        m_minor(vmajor < 0 ? -1 : std::max<int>(-1, vminor)),
        m_micro(vmajor < 0 || vminor < 0 ? -1 : std::max<int>(-1, vmicro))
    { }


    static OSType currentType()
    {
#if defined(_WIN32)
        return Windows;
#else
        return Unknown;
#endif
    }

    static LOperSysVersion current();


    friend bool operator>(const LOperSysVersion &lhs, const LOperSysVersion &rhs)
    { return lhs.m_os == rhs.m_os && LOperSysVersion::compare(lhs, rhs) > 0; }

    friend bool operator>=(const LOperSysVersion &lhs, const LOperSysVersion &rhs)
    { return lhs.m_os == rhs.m_os && LOperSysVersion::compare(lhs, rhs) >= 0; }

    friend bool operator<(const LOperSysVersion &lhs, const LOperSysVersion &rhs)
    { return lhs.m_os == rhs.m_os && LOperSysVersion::compare(lhs, rhs) < 0; }

    friend bool operator<=(const LOperSysVersion &lhs, const LOperSysVersion &rhs)
    { return lhs.m_os == rhs.m_os && LOperSysVersion::compare(lhs, rhs) <= 0; }

private:
    static int compare(const LOperSysVersion &v1, const LOperSysVersion &v2);
};

LK_NAMESPACE_END

#endif // LOPERSYSVERSION_H
