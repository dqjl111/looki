#ifndef LINIFILE_H
#define LINIFILE_H

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <istream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include "lkcore/lstringutil.h"
#include "lkcore/ldebug.h"
#include "lkcore/3rd/nameof.h"

LK_NAMESPACE_BEGIN

class LKCOREDEF LIniSection {
public:
    LIniSection();

    void clear() noexcept;
    bool empty() const noexcept;
    bool hasKey(const std::string&) const;
    size_t removeKey(const std::string&);
    void renameKey(const std::string&, const std::string&);
    size_t size() const noexcept;
    template <typename T>
    T get(const std::string&) const;
    template <typename T>
    void set(const std::string&, const T&);
    std::unordered_map<std::string, std::string>::const_iterator begin() const noexcept;
    std::unordered_map<std::string, std::string>::iterator begin() noexcept;
    std::unordered_map<std::string, std::string>::const_iterator end() const noexcept;
    std::unordered_map<std::string, std::string>::iterator end() noexcept;
    std::string& operator[](const std::string&);

private:
    std::unordered_map<std::string, std::string> m_items;
};


template <typename T>
T LIniSection::get(const std::string& key) const
{
    std::unordered_map<std::string, std::string>::const_iterator iter = m_items.find(key);
    if (iter == m_items.end()) {
        lWarn() << "Key:" << key << " does not exist";
    }

    if constexpr (std::is_same<T, bool>::value) {
        return LStringUtil::stob(key);
    }
    return LStringUtil::sto<T>(key);
}

template <typename T>
void LIniSection::set(const std::string& key, const T& value)
{
    if constexpr (std::is_same<T, bool>::value) {
        m_items[key] = value ? "true" : "false";
    } else if constexpr (std::is_same<T, int>::value || std::is_same<T, float>::value || std::is_same<T, double>::value || std::is_same<T, size_t>::value) {
        std::ostringstream stream;
        stream << value;
        m_items[key] = stream.str();
    } else if constexpr (std::is_same<T, std::string>::value) {
        m_items[key] = value;
    } else {
        lWarn() << "Type:" << NAMEOF_SHORT_TYPE(T)<< " is not supported";
    }
}


class LKCOREDEF LIniFile {
public:
    LIniFile();
    LIniFile(std::ifstream&);
    LIniFile(const std::string&);

    void addSection(const std::string&);
    void clear() noexcept;
    bool empty() const noexcept;
    bool hasSection(const std::string&) const;
    size_t removeSection(const std::string&);
    void renameSection(const std::string&, const std::string&);
    size_t size() const noexcept;
    void write(const std::filesystem::path&) const;
    std::unordered_map<std::string, LIniSection>::const_iterator begin() const noexcept;
    std::unordered_map<std::string, LIniSection>::iterator begin() noexcept;
    std::unordered_map<std::string, LIniSection>::const_iterator end() const noexcept;
    std::unordered_map<std::string, LIniSection>::iterator end() noexcept;
    LIniSection& operator[](const std::string&);

private:
    void read(std::istream&);

    std::unordered_map<std::string, LIniSection> m_sections;
};

LK_NAMESPACE_END
#endif // LINIFILE_H
