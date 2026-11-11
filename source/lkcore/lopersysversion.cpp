#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "lopersysversion.h"
#include "lglobal.h"

#include <string>


LK_NAMESPACE_BEGIN

static inline OSVERSIONINFOEX determineWinOsVersion()
{
    OSVERSIONINFOEX result = { sizeof(OSVERSIONINFOEX), 0, 0, 0, 0, {'\0'}, 0, 0, 0, 0, 0};

    HMODULE ntdll = GetModuleHandle("ntdll.dll");
    if (L_UNLIKELY(!ntdll)){
        return result;
    }

    // NTSTATUS is not defined on WinRT
    typedef LONG NTSTATUS;
    typedef NTSTATUS (NTAPI *RtlGetVersionFunction)(LPOSVERSIONINFO);

    // RtlGetVersion is documented public API but we must load it dynamically
    // because linking to it at load time will not pass the Windows App Certification Kit
    // https://msdn.microsoft.com/en-us/library/windows/hardware/ff561910.aspx
    RtlGetVersionFunction pRtlGetVersion = reinterpret_cast<RtlGetVersionFunction>(
        reinterpret_cast<LFunctionPointer>(GetProcAddress(ntdll, "RtlGetVersion")));
    if (L_UNLIKELY(!pRtlGetVersion)){
        return result;
    }

    // GetVersionEx() has been deprecated in Windows 8.1 and will return
    // only Windows 8 from that version on, so use the kernel API function.
    pRtlGetVersion(reinterpret_cast<LPOSVERSIONINFO>(&result)); // always returns STATUS_SUCCESS
    return result;
}



LOperSysVersion::LOperSysVersion()
{

}

LOperSysVersion LOperSysVersion::current()
{
    LOperSysVersion v;
    v.m_os = currentType();
    const OSVERSIONINFOEX osv = determineWinOsVersion();
    v.m_major = osv.dwMajorVersion;
    v.m_minor = osv.dwMinorVersion;
    v.m_micro = osv.dwBuildNumber;
    return v;
}

/*!
    \variable LOperSysVersion::Windows7
    \brief a version corresponding to Windows 7 (version 6.1).
    \since 5.9
 */
const LOperSysVersion LOperSysVersion::Windows7 =
    LOperSysVersion(LOperSysVersion::Windows, 6, 1);

/*!
    \variable LOperSysVersion::Windows8
    \brief a version corresponding to Windows 8 (version 6.2).
    \since 5.9
 */
const LOperSysVersion LOperSysVersion::Windows8 =
    LOperSysVersion(LOperSysVersion::Windows, 6, 2);

/*!
    \variable LOperSysVersion::Windows8_1
    \brief a version corresponding to Windows 8.1 (version 6.3).
    \since 5.9
 */
const LOperSysVersion LOperSysVersion::Windows8_1 =
    LOperSysVersion(LOperSysVersion::Windows, 6, 3);

/*!
    \variable LOperSysVersion::Windows10
    \brief a version corresponding to Windows 10 (version 10.0).
    \since 5.9
 */
const LOperSysVersion LOperSysVersion::Windows10 =
    LOperSysVersion(LOperSysVersion::Windows, 10);

static inline int compareVersionComponents(int lhs, int rhs)
{
    return lhs >= 0 && rhs >= 0 ? lhs - rhs : 0;
}


int LOperSysVersion::compare(const LOperSysVersion &v1, const LOperSysVersion &v2)
{
    if (v1.m_major == v2.m_major) {
        if (v1.m_minor == v2.m_minor) {
            return compareVersionComponents(v1.m_micro, v2.m_micro);
        }
        return compareVersionComponents(v1.m_minor, v2.m_minor);
    }
    return compareVersionComponents(v1.m_major, v2.m_major);
}


LK_NAMESPACE_END
