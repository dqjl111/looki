#include "linifile.h"

LK_NAMESPACE_BEGIN

LIniSection::LIniSection()
{
}

void LIniSection::clear() noexcept
{
    m_items.clear();
}

bool LIniSection::empty() const noexcept
{
    return m_items.empty();
}

bool LIniSection::hasKey(const std::string& key) const
{
    return m_items.find(key) != m_items.end();
}

size_t LIniSection::removeKey(const std::string& key)
{
    if (m_items.find(key) == m_items.end()) {
        throw std::invalid_argument("key does not exist");
    }

    return m_items.erase(key);
}

void LIniSection::renameKey(const std::string& oldKey, const std::string& newKey)
{
    if (oldKey.empty() || newKey.empty()) {
        throw std::invalid_argument("keys cannot be empty");
    }

    if (m_items.find(oldKey) == m_items.end()) {
        throw std::invalid_argument("old key does not exist");
    }

    if (m_items.find(newKey) != m_items.end()) {
        throw std::invalid_argument("new key already exists");
    }

    auto item = m_items.extract(oldKey);
    item.key() = newKey;
    m_items.insert(std::move(item));
}

size_t LIniSection::size() const noexcept
{
    return m_items.size();
}

std::unordered_map<std::string, std::string>::const_iterator LIniSection::begin() const noexcept
{
    return m_items.begin();
}

std::unordered_map<std::string, std::string>::iterator LIniSection::begin() noexcept
{
    return m_items.begin();
}

std::unordered_map<std::string, std::string>::const_iterator LIniSection::end() const noexcept
{
    return m_items.end();
}

std::unordered_map<std::string, std::string>::iterator LIniSection::end() noexcept
{
    return m_items.end();
}

std::string& LIniSection::operator[](const std::string& key)
{
    if (key.empty()) {
        throw std::invalid_argument("keys cannot be empty");
    }
    return m_items[key];
}

////
LIniFile::LIniFile()
{
}

LIniFile::LIniFile(std::ifstream& stream)
{
    if (!stream.is_open()) {
        throw std::invalid_argument("stream is closed");
    }

    read(stream);
}

LIniFile::LIniFile(const std::string& text)
{
    std::istringstream stream(text);
    read(stream);
}

void LIniFile::addSection(const std::string& sectionName)
{
    std::unordered_map<std::string, LIniSection>::const_iterator iter = m_sections.find(sectionName);
    if (iter != m_sections.cend()) {
        throw std::invalid_argument("section already exists");
    }

    m_sections.insert({sectionName,LIniSection()});
}

void LIniFile::clear() noexcept
{
    m_sections.clear();
}

bool LIniFile::empty() const noexcept
{
    return m_sections.empty();
}

bool LIniFile::hasSection(const std::string& sectionName) const
{
    return m_sections.find(sectionName) != m_sections.end();
}

size_t LIniFile::removeSection(const std::string& sectionName)
{
    if (sectionName.empty()) {
        throw std::invalid_argument("section names cannot be empty");
    }

    if (m_sections.find(sectionName) == m_sections.end()) {
        throw std::invalid_argument("section does not exist");
    }

    return m_sections.erase(sectionName);
}

void LIniFile::renameSection(const std::string& old_section_name, const std::string& newSectionName)
{
    if (old_section_name.empty() || newSectionName.empty()) {
        throw std::invalid_argument("section names cannot be empty");
    }

    if (m_sections.find(old_section_name) == m_sections.end()) {
        throw std::invalid_argument("old section name does not exist");
    }

    if (m_sections.find(newSectionName) != m_sections.end()) {
        throw std::invalid_argument("new section name already exists");
    }

    auto section = m_sections.extract(old_section_name);
    section.key() = newSectionName;
    m_sections.insert(std::move(section));
}

size_t LIniFile::size() const noexcept
{
    return m_sections.size();
}

void LIniFile::write(const std::filesystem::path& path) const
{
    std::ofstream stream(path);
    if (!stream.is_open()) {
        throw std::invalid_argument("stream is closed");
    }

    for (auto const& [sectionName, section] : m_sections) {
        stream << "[" << sectionName << "]" << std::endl;
        for (auto const& [key, value] : section) {
            stream << key << " = " << value << std::endl;
        }

        stream << std::endl;
    }

    stream.close();
}

std::unordered_map<std::string, LIniSection>::const_iterator LIniFile::begin() const noexcept
{
    return m_sections.begin();
}

std::unordered_map<std::string, LIniSection>::iterator LIniFile::begin() noexcept
{
    return m_sections.begin();
}

std::unordered_map<std::string, LIniSection>::const_iterator LIniFile::end() const noexcept
{
    return m_sections.end();
}

std::unordered_map<std::string, LIniSection>::iterator LIniFile::end() noexcept
{
    return m_sections.end();
}

LIniSection& LIniFile::operator[](const std::string& sectionName)
{
    if (sectionName.empty()) {
        throw std::invalid_argument("section headers cannot be empty");
    }

    return m_sections[sectionName];
}

void LIniFile::read(std::istream& stream)
{
    LIniSection* section = nullptr;
    std::string line;
    while (std::getline(stream, line)) {
        // Removes whitespace on both ends.
        line = LStringUtil::trim(line);

        // Checks if the current line is a section declaration.
        size_t rBracketPos = line.rfind(']');
        if (line.find('[') == 0 && rBracketPos != std::string::npos) {
            if (rBracketPos == 1) {
                throw std::invalid_argument("section headers cannot be empty");
            }

            std::string sectionName = line.substr(1, rBracketPos - 1);
            section = &m_sections[sectionName];
            continue;
        }

        // Skips the current line if it doesn't contain a delimiter ('=', ':')
        // or if the line is a comment.
        size_t delimiterPos = std::min<size_t>(line.find('='), line.find(':'));
        if (line.find(';') == 0 || line.find('#') == 0 || delimiterPos == std::string::npos) {
            continue;
        }

        // Throws an exception because if the section is null
        // then a section header is missing from the configuration file.
        if (section == nullptr) {
            throw std::invalid_argument("file is missing a section header");
        }

        std::string key = LStringUtil::trim(line.substr(0, delimiterPos));

        // Checks if the key is an empty string.
        if (key.empty()) {
            throw std::invalid_argument("keys cannot be empty");
        }

        // Checks if the key already exists.
        if ((*section).hasKey(key)) {
            throw std::invalid_argument("key already exists");
        }

        std::string value = LStringUtil::trim(line.substr(delimiterPos + 1));

        // Adds the key and value to the section.
        (*section)[key] = value;
    }
}

LK_NAMESPACE_END
