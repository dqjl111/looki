#ifndef LWINREGISTRY_H
#define LWINREGISTRY_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>        // Win32 Platform SDK
#include <string.h>         // For wcslen()
#include <memory>           // For std::unique_ptr
#include <stdexcept>        // For std::runtime_error
#include <string>           // For std::wstring
#include <utility>          // For std::pair, std::swap
#include <vector>           // For std::vector
#include "lkcore/lookiconfig.h"

LK_NAMESPACE_BEGIN
class LKCOREDEF LWinRegKey
{
public:

    //
    // Initialize as an empty key handle
    //
    LWinRegKey() noexcept = default;

    //
    // Take ownership of the input raw key handle
    //
    explicit LWinRegKey(HKEY hKey) noexcept
        : m_hKey{ hKey }
    {}

    //
    // Take ownership of the input key handle.
    // The input key handle wrapper is reset to an empty state
    //
    LWinRegKey(LWinRegKey&& other) noexcept
        : m_hKey{ other.m_hKey }
    {
        //
        // Other doesn't own the raw handle anymore
        //
        other.m_hKey = nullptr;
    }

    //
    // Move-assign from the input key handle.
    // Properly check against self-move-assign (which is safe and does nothing).
    //
    LWinRegKey& operator=(LWinRegKey&& other) noexcept
    {
        //
        // Prevent self-move-assign
        //
        if ((this != &other) && (m_hKey != other.m_hKey))
        {
            close();

            m_hKey = other.m_hKey;
            other.m_hKey = nullptr;
        }
        return *this;
    }

    //
    // Ban copy
    //
    LWinRegKey(const LWinRegKey&) = delete;
    LWinRegKey& operator=(const LWinRegKey&) = delete;

    //
    // Safely close the wrapped key handle (if any)
    //
    ~LWinRegKey() noexcept
    {
        close();
    }

    //
    // Give read-only access to the wrapped HKEY handle
    //
    HKEY get() const noexcept
    {
        return m_hKey;
    }

    //
    // Close current HKEY handle.
    // If key wasn't open, does nothing.
    //
    void close() noexcept
    {
        if (isValid())
        {
            ::RegCloseKey(m_hKey);
            m_hKey = nullptr;
        }
    }

    //
    // Is the wrapped HKEY handle valid?
    //
    bool isValid() const noexcept
    {
        return m_hKey != nullptr;
    }

    //
    // Has the same semantic of IsValid, but allows a short "if (regKey)" syntax
    //
    explicit operator bool() const noexcept
    {
        return isValid();
    }

    //
    // Transfer ownership of current HKEY to the caller.
    // Note that the caller is responsible for closing the key raw handle!
    //
    HKEY detach() noexcept
    {
        HKEY hKey = m_hKey;
        m_hKey = nullptr;
        return hKey;
    }

    //
    // Take ownership of the HKEY handle.
    // Safely close any previously open handle.
    // Input key handle can be nullptr; in this case this wrapper wraps an empty handle.
    //
    void attach(HKEY hKey) noexcept
    {
        //
        // Prevent self-attach
        //
        if (m_hKey != hKey)
        {
            close();
            m_hKey = hKey;
        }
    }

    //
    // Non-throwing swap
    //
    friend void swap(LWinRegKey& a, LWinRegKey& b) noexcept
    {
        std::swap(a.m_hKey, b.m_hKey);
    }


    //
    // Overload comparison and relational operators
    //

    friend bool operator==(const LWinRegKey& a, const LWinRegKey& b) noexcept
    {
        return a.m_hKey == b.m_hKey;
    }

    friend bool operator!=(const LWinRegKey& a, const LWinRegKey& b) noexcept
    {
        return a.m_hKey != b.m_hKey;
    }

    friend bool operator<(const LWinRegKey& a, const LWinRegKey& b) noexcept
    {
        return a.m_hKey < b.m_hKey;
    }

    friend bool operator<=(const LWinRegKey& a, const LWinRegKey& b) noexcept
    {
        return a.m_hKey <= b.m_hKey;
    }

    friend bool operator>(const LWinRegKey& a, const LWinRegKey& b) noexcept
    {
        return a.m_hKey > b.m_hKey;
    }

    friend bool operator>=(const LWinRegKey& a, const LWinRegKey& b) noexcept
    {
        return a.m_hKey >= b.m_hKey;
    }


private:
    //
    // The wrapped registry key handle
    //
    HKEY m_hKey = nullptr;
};

class LKCOREDEF LWinRegistry
{
    bool m_isLogOn;
public:
    LWinRegistry(bool isOn = false);
    inline void setLogOn(bool isOn){
        m_isLogOn = isOn;
    }
    bool isLogOn(){
        return m_isLogOn;
    }


    DWORD                       getDword         (HKEY hKey, const std::wstring& subKey, const std::wstring& valueName, DWORD defValue = 0);
    ULONGLONG                   getQword         (HKEY hKey, const std::wstring& subKey, const std::wstring& valueName, ULONGLONG defValue = 0);
    std::wstring                getString        (HKEY hKey, const std::wstring& subKey, const std::wstring& valueName);
    std::vector<std::wstring>   getMultiString   (HKEY hKey, const std::wstring& subKey, const std::wstring& valueName);
    std::vector<BYTE>           getBinary        (HKEY hKey, const std::wstring& subKey, const std::wstring& valueName);

    //
    // Enumerates all the values under the given (open) hKey registry key.
    // The information for each value is stored in a pair<wstring, DWORD>,
    // representing the value's name and value's type respectively.
    //
    std::vector<std::pair<std::wstring, DWORD>> enumValues(HKEY hKey);

    //
    // Enumerate sub-keys of the given (open) hKey registry key.
    //
    std::vector<std::wstring> enumSubKeys(HKEY hKey);

    //
    // Wrappers for creating and opening registry keys
    //
    LWinRegKey createKey(
        HKEY hKeyParent,
        const std::wstring& keyName,
        LPWSTR keyClass = REG_NONE,
        DWORD options = REG_OPTION_NON_VOLATILE,
        REGSAM access = KEY_READ | KEY_WRITE,
        SECURITY_ATTRIBUTES* securityAttributes = nullptr,
        DWORD* disposition = nullptr
        );

    LWinRegKey openKey(HKEY hKeyParent, const std::wstring& keyName, REGSAM access = KEY_READ | KEY_WRITE);
};
LK_NAMESPACE_END
#endif // LWINREGISTRY_H
