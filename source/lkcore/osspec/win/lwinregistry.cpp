#include "lwinregistry.h"
#include "lkcore/ldebug.h"
LK_NAMESPACE_BEGIN

LWinRegistry::LWinRegistry(bool isOn) :m_isLogOn(isOn)
{

}

DWORD LWinRegistry::getDword(HKEY hKey, const std::wstring& subKey, const std::wstring& value, DWORD defValue)
{
    //
    // Read a 32-bit DWORD value from the registry
    //
    DWORD data = defValue;
    DWORD dataSize = sizeof(data);
    LONG retCode = ::RegGetValueW(
        hKey,
        subKey.c_str(),
        value.c_str(),
        RRF_RT_REG_DWORD,
        nullptr,
        &data,
        &dataSize);
    if (retCode != ERROR_SUCCESS)
    {
        if(m_isLogOn){
            lWarn()<< "Cannot read DWORD from registry:" <<retCode;
        }
    }

    return data;
}


ULONGLONG LWinRegistry::getQword(HKEY hKey, const std::wstring& subKey, const std::wstring& value, ULONGLONG defValue)
{
    //
    // Read a 64-bit QWORD value from the registry
    //
    ULONGLONG data = defValue;
    DWORD dataSize = sizeof(data);
    LONG retCode = ::RegGetValueW(
        hKey,
        subKey.c_str(),
        value.c_str(),
        RRF_RT_REG_QWORD,
        nullptr,
        &data,
        &dataSize);
    if (retCode != ERROR_SUCCESS)
    {
        if(m_isLogOn){
            lWarn()<< "Cannot read QWORD from registry:" << retCode;
        }
    }

    return data;
}


std::wstring LWinRegistry::getString(HKEY hKey, const std::wstring& subKey, const std::wstring& value)
{
    //
    // Request the size of the string, in bytes
    //
    DWORD dataSize = 0;
    LONG retCode = ::RegGetValueW(
        hKey,
        subKey.c_str(),
        value.c_str(),
        RRF_RT_REG_SZ,
        nullptr,
        nullptr,
        &dataSize);
    if (retCode != ERROR_SUCCESS)
    {
        if(m_isLogOn){
            lWarn()<< "Cannot read string from registry:" << hKey << ","<< subKey << ","<<value ;
        }
        return L"";
    }

    //
    // Allocate room for the result string.
    //
    // Note that dataSize is in bytes, but wstring::resize() expects length in wchar_ts,
    // so must convert from bytes to wchar_t count first.
    //
    std::wstring data;
    data.resize(dataSize / sizeof(wchar_t));

    //
    // Read the string from the registry into local wstring object
    //
    retCode = ::RegGetValueW(
        hKey,
        subKey.c_str(),
        value.c_str(),
        RRF_RT_REG_SZ,
        nullptr,
        &data[0],
        &dataSize);
    if (retCode != ERROR_SUCCESS)
    {
        if(m_isLogOn){
            lWarn()<< "Cannot read string from registry" << retCode;
        }
        return L"";
    }

    //
    // On return, RegGetValueW() writes in dataSize the actual size of the string, in bytes.
    // We must resize the wstring object to the proper string size.
    // Note that dataSize is expressed in bytes, and includes the terminating NUL; so we have to
    // subtract the NUL from the total string length, as wstring objects are already NUL-terminated.
    //
    DWORD stringLengthInWchars = dataSize / sizeof(wchar_t);
    stringLengthInWchars--; // exclude the NUL written by the Win32 API
    data.resize(stringLengthInWchars);

    return data;
}


std::vector<std::wstring> LWinRegistry::getMultiString(HKEY hKey, const std::wstring& subKey, const std::wstring& value)
{
    //
    // Request the size of the multi-string, in bytes
    //
    DWORD dataSize;
    LONG retCode = ::RegGetValueW(
        hKey,
        subKey.c_str(),
        value.c_str(),
        RRF_RT_REG_MULTI_SZ,
        nullptr,
        nullptr,
        &dataSize);
    if (retCode != ERROR_SUCCESS)
    {
        if(m_isLogOn){
            lWarn()<< "Cannot read multi-string from registry:" << retCode;
        }
        return std::vector<std::wstring>();
    }

    //
    // Allocate room for the result multi-string.
    //
    // Note that dataSize is in bytes, but our vector<wchar_t> object has size expressed in wchar_ts.
    //
    std::vector<wchar_t> data;
    data.resize(dataSize / sizeof(wchar_t));

    //
    // Read the multi-string from the registry into the vector object
    //
    retCode = ::RegGetValueW(
        hKey,
        subKey.c_str(),
        value.c_str(),
        RRF_RT_REG_MULTI_SZ,
        nullptr,
        &data[0],
        &dataSize);
    if (retCode != ERROR_SUCCESS)
    {
        if(m_isLogOn){
            lWarn()<< "Cannot read multi-string from registry:" << retCode;
        }
        return std::vector<std::wstring>();
    }

    //
    // Resize vector to the actual size returned by GetRegValue().
    // Note that the vector is a vector of wchar_ts, instead the size returned by GetRegValue()
    // is in bytes, so we have to scale from bytes to wchar_t count.
    //
    data.resize( dataSize / sizeof(wchar_t) );

    //
    // Parse the double-NUL-terminated string into a vector<wstring>
    //
    std::vector<std::wstring> result;
    const wchar_t* currStringPtr = &data[0];
    while (*currStringPtr != L'\0')
    {
        // Current string is NUL-terminated, so get its length with wcslen()
        const size_t currStringLength = wcslen(currStringPtr);

        // Add current string to result vector
        result.push_back(std::wstring{ currStringPtr, currStringLength });

        // Move to the next string
        currStringPtr += currStringLength + 1;
    }

    return result;
}


std::vector<BYTE> LWinRegistry::getBinary(HKEY hKey, const std::wstring& subKey, const std::wstring& value)
{
    //
    // Request the size of the binary data, in bytes
    //
    DWORD dataSize{};
    LONG retCode = ::RegGetValueW(
        hKey,
        subKey.c_str(),
        value.c_str(),
        RRF_RT_REG_BINARY,
        nullptr,
        nullptr,
        &dataSize);
    if (retCode != ERROR_SUCCESS)
    {
        if(m_isLogOn){
            lWarn()<< "Cannot read binary data from registry:" << retCode;
        }
        return std::vector<BYTE>();
    }

    //
    // Allocate room for the result binary data
    //
    std::vector<BYTE> data;
    data.resize(dataSize);

    //
    // Read the binary data from the registry into the vector object
    //
    retCode = ::RegGetValueW(
        hKey,
        subKey.c_str(),
        value.c_str(),
        RRF_RT_REG_BINARY,
        nullptr,
        &data[0],
        &dataSize);
    if (retCode != ERROR_SUCCESS)
    {
        if(m_isLogOn){
            lWarn()<< "Cannot read binary data from registry:" << retCode;
        }
        return std::vector<BYTE>();
    }

    //
    // Resize vector to the actual size returned by GetRegValue().
    //
    data.resize(dataSize);


    return data;
}


std::vector<std::pair<std::wstring, DWORD>> LWinRegistry::enumValues(HKEY hKey)
{
    //
    // Get useful pre-enumeration info, like the total number of values
    // and the maximum length of the value names
    //
    DWORD valueCount = 0;
    DWORD maxValueNameLen = 0;
    LONG retCode = ::RegQueryInfoKey(
        hKey,
        nullptr,    // no user-defined class
        nullptr,    // no user-defined class size
        nullptr,    // reserved
        nullptr,    // no subkey count
        nullptr,    // no subkey max length
        nullptr,    // no subkey class length
        &valueCount,
        &maxValueNameLen,
        nullptr,    // no max value length
        nullptr,    // no security descriptor
        nullptr     // no last write time
        );
    if (retCode != ERROR_SUCCESS)
    {
        if(m_isLogOn){
            lWarn()<< "RegQueryInfoKey failed while preparing for value enumeration:" << retCode;
        }
        return std::vector<std::pair<std::wstring, DWORD>>();
    }

    //
    // NOTE: According to the MSDN documentation, the size returned for value name max length
    // does *not* include the terminating NUL, so let's add +1 to take it into account
    // when I allocate the buffer for reading value names.
    //
    maxValueNameLen++;

    //
    // Preallocate a buffer for the value names
    //
    auto nameBuffer = std::make_unique<wchar_t[]>(maxValueNameLen);

    //
    // The value names and types will be stored here
    //
    std::vector<std::pair<std::wstring, DWORD>> valueInfo;

    //
    // Enumerate all the values
    //
    for (DWORD index = 0; index < valueCount; index++)
    {
        //
        // Get the name and the type of the current value
        //
        DWORD valueNameLen = maxValueNameLen;
        DWORD valueType{};
        retCode = ::RegEnumValueW(
            hKey,
            index,
            nameBuffer.get(),
            &valueNameLen,
            nullptr,    // reserved
            &valueType,
            nullptr,    // no data
            nullptr     // no data size
            );
        if (retCode != ERROR_SUCCESS)
        {
            if(m_isLogOn){
                lWarn()<< "Cannot enumerate values: RegEnumValueW failed:" << retCode;
            }
            continue;
        }

        //
        // On success, the RegEnumValueW API writes the length of the
        // value name in the valueNameLen output parameter
        // (not including the terminating NUL).
        // So we can build a wstring based on that length.
        //
        // The value info pair is made by the value's name and the value's type.
        //
        valueInfo.push_back(std::make_pair(
            std::wstring{ nameBuffer.get(), valueNameLen },
            valueType
            ));
    }

    return valueInfo;
}


std::vector<std::wstring> LWinRegistry::enumSubKeys(HKEY hKey)
{
    //
    // Get some useful pre-enumeration info, like the total number of subkeys
    // and the maximum length of the subkey names
    //
    DWORD subKeyCount{};
    DWORD maxSubKeyNameLen{};
    LONG retCode = ::RegQueryInfoKey(
        hKey,
        nullptr,    // no user-defined class
        nullptr,    // no user-defined class size
        nullptr,    // reserved
        &subKeyCount,
        &maxSubKeyNameLen,
        nullptr,    // no subkey class length
        nullptr,    // no value count
        nullptr,    // no value name max length
        nullptr,    // no max value length
        nullptr,    // no security descriptor
        nullptr     // no last write time
        );
    if (retCode != ERROR_SUCCESS)
    {
        if(m_isLogOn){
            lWarn()<< "RegQueryInfoKey failed while preparing for subkey enumeration:" << retCode;
        }
        return std::vector<std::wstring>();
    }

    //
    // NOTE: According to the MSDN documentation, the size returned for subkey name max length
    // does *not* include the terminating NUL, so let's add +1 to take it into account
    // when I allocate the buffer for reading subkey names.
    //
    maxSubKeyNameLen++;

    //
    // Preallocate a buffer for the subkey names
    //
    auto nameBuffer = std::make_unique<wchar_t[]>(maxSubKeyNameLen);

    //
    // The subkey names will be stored here
    //
    std::vector<std::wstring> subkeyNames;

    //
    // Enumerate all the subkeys
    //
    for (DWORD index = 0; index < subKeyCount; index++)
    {
        //
        // Get the name of the current subkey
        //
        DWORD subKeyNameLen = maxSubKeyNameLen;
        retCode = ::RegEnumKeyExW(
            hKey,
            index,
            nameBuffer.get(),
            &subKeyNameLen,
            nullptr, // reserved
            nullptr, // no class
            nullptr, // no class
            nullptr  // no last write time
            );
        if (retCode != ERROR_SUCCESS)
        {
            if(m_isLogOn){
                lWarn()<< "Cannot enumerate subkeys: RegEnumKeyExW failed:" << retCode;
            }
        }

        //
        // On success, the ::RegEnumKeyExW API writes the length of the
        // subkey name in the subKeyNameLen output parameter
        // (not including the terminating NUL).
        // So I can build a wstring based on that length.
        //
        subkeyNames.push_back(std::wstring{ nameBuffer.get(), subKeyNameLen });
    }

    return subkeyNames;
}


LWinRegKey LWinRegistry::createKey(
    HKEY hKeyParent,
    const std::wstring& keyName,
    LPWSTR keyClass,
    DWORD options,
    REGSAM access,
    SECURITY_ATTRIBUTES* securityAttributes,
    DWORD* disposition)
{
    //
    // Create the key invoking the native RegCreateKeyEx Win32 API
    //
    HKEY hKey = nullptr;
    LONG retCode = ::RegCreateKeyExW(
        hKeyParent,
        keyName.c_str(),
        0,          // reserved
        keyClass,
        options,
        access,
        securityAttributes,
        &hKey,
        disposition
        );
    if (retCode != ERROR_SUCCESS)
    {
        if(m_isLogOn){
            lWarn()<< "Cannot create registry key: RegCreateKeyEx failed:" << retCode;
        }
    }

    //
    // Return the raw key handle safely wrapped in the C++ key resource manager
    //
    return LWinRegKey{ hKey };
}


LWinRegKey LWinRegistry::openKey(HKEY hKeyParent, const std::wstring& keyName, REGSAM access)
{
    //
    // Open the key invoking the native RegOpenKeyEx Win32 API
    //
    HKEY hKey = nullptr;
    LONG retCode = ::RegOpenKeyExW(
        hKeyParent,
        keyName.c_str(),
        0, // default options
        access,
        &hKey
        );
    if (retCode != ERROR_SUCCESS)
    {
        if(m_isLogOn){
            lWarn()<< "Cannot open registry key: RegOpenKeyEx failed:" << retCode;
        }
    }

    //
    // Return the raw key handle safely wrapped in the C++ key resource manager
    //
    return LWinRegKey{ hKey };
}

LK_NAMESPACE_END
