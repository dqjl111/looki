#include "utf8iterator.h"

#include <string.h>
#include <assert.h>

//LUtf8Iterator

LK_NAMESPACE_BEGIN

LUtf8Iterator::LUtf8Iterator(){}

LUtf8Iterator::LUtf8Iterator(const std::string &s)
{
    init(s.data(), s.length());
}

LUtf8Iterator::LUtf8Iterator(const char *ptr)
{
    init(ptr);
}

LUtf8Iterator::LUtf8Iterator(const char *ptr, uint32_t length)
{
    init(ptr, length);
}

void LUtf8Iterator::init(const std::string &s)
{
    init(s.data(), s.length());
}

void LUtf8Iterator::init(const char* ptr) {
    m_ptr     = ptr;
    m_unicode = 0;
    m_pos  = 0;
    m_nextPos   = 0;
    m_count     = 0;
    m_length    = (ptr == nullptr) ? uint32_t(0) : strlen(ptr);
}

void LUtf8Iterator::init(const char* ptr, uint32_t length) {
    m_ptr       = ptr;
    m_unicode = 0;
    m_pos  = 0;
    m_nextPos   = 0;
    m_count     = 0;
    m_length    = length;
}


bool LUtf8Iterator::atBegin() const
{
    if (m_ptr == nullptr){
        return true;
    }
    return m_pos == 0;
}

bool LUtf8Iterator::atEnd() const
{
    if (m_ptr == nullptr){
        return true;
    }
    return !(m_pos < m_length);
}

bool LUtf8Iterator::next() {

    if (m_ptr == nullptr){
        return false;
    }

    if (m_nextPos < m_length) {
        m_pos = m_nextPos;

        const char*  t_pointer 	= m_ptr + m_nextPos; //Set Current Pointer
        m_size 	= utf8Charsize(t_pointer);

        if (m_size == 0){
            return false;
        }

        m_nextPos = m_nextPos + m_size;
        m_unicode = utf8ToUnicode(t_pointer, m_size);

        if (m_unicode == 0){
            return false;
        }

        m_count++;
        return true;
	}
	else {
        m_pos = m_nextPos;
        return false;
	}
}

bool LUtf8Iterator::previous() {
    if (m_ptr == nullptr){
        return false;
    }

    if (m_length != 0) {
        if (m_pos == 0 && m_nextPos == 0) {
            m_pos 	= m_length;
            m_count = utf8Length(m_ptr, m_length);
		}
	}

    if (m_pos > 0) {
        m_nextPos = m_pos;
        m_pos--;

        if ((m_ptr[m_pos] & 0x80) == 0) {
            m_size = 1;
		}
		else {
            m_size = 1;
            while ((m_ptr[m_pos] & 0xC0) == 0x80 && m_size < 6) {
                m_pos--;
                m_size++;
			}
		}

        const char* t_pointer = m_ptr + m_pos;
        m_unicode = utf8ToUnicode(t_pointer, m_size);

        if (m_unicode == 0){
            return false;
        }
        m_count--;
        return true;
	}
	else {
        m_nextPos = 0;
        return false;
	}
}

std::string LUtf8Iterator::utf8Code() {

    char str[10];
    str[0] = '\0';
    if (m_ptr == nullptr){
        return str;
    }
    if (m_size == 0){
        return str;
    }

    if (m_size == 1) {
        str[0] = m_ptr[m_pos ];
		str[1] = '\0';
		return str;
	}

    const char* t_pointer = m_ptr + m_pos;
    for (uint8_t i = 0; i < m_size; i++) {
		str[i] = t_pointer[i];
	}
    str[m_size] = '\0';
	return str;
}

//Utilities

uint32_t LUtf8Iterator::utf8Length(const char* string) {

	if (string == NULL) return 0;

	uint32_t t_length 	= 0;
	uint32_t t_pos 		= 0;

	while (string[t_pos]) {
        t_pos = t_pos + utf8Charsize(string + t_pos);
		t_length++;
	}

	return t_length;
}

uint32_t LUtf8Iterator::utf8Length(const char* string, uint32_t end) {

	if (string == NULL) return 0;

	uint32_t t_length 	= 0;
	uint32_t t_pos 		= 0;

	while (string[t_pos] && t_pos < end) {
        t_pos = t_pos + utf8Charsize(string + t_pos);
		t_length++;
	}

	return t_length;
}

uint32_t LUtf8Iterator::utf8ToUnicode(const char* character) {

	if (character == NULL) return 0;
	if (character[0] == 0) return 0;

    uint8_t t_size = utf8Charsize(character);

    if (t_size == 0){
        return 0;
    }
    return utf8ToUnicode(character, t_size);
}

std::string LUtf8Iterator::unicodeToUtf8(uint32_t codepoint) {
    return unicodeToUtf8(codepoint, unicodeCharsize(codepoint));
}

//Internal use / Advanced use.

////// UTF8 to Unicode

uint8_t LUtf8Iterator::utf8Charsize(const char* character) {

    if (character == nullptr) return 0;
	if (character[0] == 0) return 0;

	if ((character[0] & 0x80) == 0) return 1;
	else if ((character[0] & 0xE0) == 0xC0) return 2;
	else if ((character[0] & 0xF0) == 0xE0) return 3;
	else if ((character[0] & 0xF8) == 0xF0) return 4;
	else if ((character[0] & 0xFC) == 0xF8) return 5;
	else if ((character[0] & 0xFE) == 0xFC) return 6;

	return 0;
}

static const uint8_t ToUnicode[] = {0, 0, 0x1F, 0xF, 0x7, 0x3, 0x1};

uint32_t LUtf8Iterator::utf8ToUnicode(const char* character, uint8_t size) {

	if (size == 0) return 0;
    if (character == nullptr) return 0;
	if (character[0] == 0) return 0;

	static uint32_t codepoint = 0;

	if (size == 1) {
		return character[0];
	}

	codepoint = ToUnicode[size] & character[0];

	for (uint8_t i = 1; i < size; i++) {
		codepoint = codepoint << 6;
		codepoint = codepoint | (character[i] & 0x3F);
	}

	return codepoint;
}

////// Unicode to UTF8

uint8_t LUtf8Iterator::unicodeCharsize(uint32_t codepoint) {

	if (codepoint == 0) return 0;

	if (codepoint < 0x80) return 1;
	else if (codepoint < 0x800) return 2;
	else if (codepoint < 0x10000) return 3;
	else if (codepoint < 0x200000) return 4;
	else if (codepoint < 0x4000000) return 5;
	else if (codepoint <= 0x7FFFFFFF) return 6;

	return 0;
}

static const uint8_t ToUTF8[] = {0, 0,  0xC0,  0xE0, 0xF0,  0xF8,  0xFC};

std::string LUtf8Iterator::unicodeToUtf8(uint32_t codepoint, uint8_t size) {
    char m_convertBuf[10];
    m_convertBuf[size] = '\0';

    if (size == 0){
        return m_convertBuf;
    }

	if (size == 1) {
        m_convertBuf[0] = codepoint;
        return m_convertBuf;
	}

	for (uint8_t i = size - 1; i > 0; i--) {
        m_convertBuf[i] = 0x80 | (codepoint & 0x3F);
		codepoint = codepoint >> 6;
	}

    m_convertBuf[0] = ToUTF8[size] | codepoint;
    return m_convertBuf;
}

LK_NAMESPACE_END
