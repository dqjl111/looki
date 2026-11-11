#ifndef LUTF8_ITERATOR_H
#define LUTF8_ITERATOR_H

#include <stdint.h>
#include "lkcore/lglobal.h"

LK_NAMESPACE_BEGIN

class LKCOREDEF LUtf8Iterator {

public:
    LUtf8Iterator();

    LUtf8Iterator(const std::string& s);
    LUtf8Iterator(const char* ptr);
    LUtf8Iterator(const char* ptr, uint32_t length);

    LUtf8Iterator(const LUtf8Iterator&) = default;
    LUtf8Iterator& operator=(const LUtf8Iterator& a) = default;

    inline LUtf8Iterator& operator++(){
        next(); return *this;
    }

    inline LUtf8Iterator& operator--(){
        previous(); return *this;
    }

    void init(const std::string& s);
    void init(const char* ptr); //All values to 0 and set ptr.
    void init(const char* ptr, uint32_t length); //allows you to set a custom length.

    bool atBegin() const;
    bool atEnd() const;
    bool next(); //returns true if there is a character in the next position. If there is not, return false.
    bool previous(); //returns true if there is a character in the back position. If there is not, return false.
    std::string utf8Code(); //return current character in UFT8 - no same that iter.unicode (not codepoint/unicode)

    // Utilities
    static uint32_t utf8Length(const char* string);
    static uint32_t utf8Length(const char* string, uint32_t end);
    static uint32_t utf8ToUnicode(const char* character); //UTF8 to Unicode.
    static std::string unicodeToUtf8(uint32_t codepoint); //Unicode to UTF8.

    // Internal use / Advanced use.
    static uint8_t	utf8Charsize(const char* character); //calculate the number of bytes a UTF8 character occupies in a string.
    static uint8_t	unicodeCharsize	(uint32_t codepoint); //calculates the number of bytes occupied by a Unicode character in UTF8.
    static uint32_t utf8ToUnicode(const char* character, uint8_t size);
    static std::string unicodeToUtf8(uint32_t codepoint, uint8_t size);


    inline const uint32_t& unicode() const { //current 32 bit unicode
        return m_unicode;
    }
    inline const uint8_t& codeSize() const{ //utf8 code character size in bytes
        return m_size;
    }
    inline const uint32_t& codePosition() const{ //current utf8 code position
        return m_pos;
    }

    inline const uint32_t& nextCodePosition() const{ //next utf8 code position
        return m_nextPos;
    }

    inline const uint32_t& codeCount(){ //number of utf8 code currently
        return m_count;
    }

    inline const uint32_t& length(){ //strlen()
        return m_length;
    }
private:
    const char* m_ptr = nullptr;
    uint32_t m_unicode = 0;
    uint8_t	m_size = 0; 		//character size in bytes
    uint32_t m_pos = 0; 	//current character position
    uint32_t m_nextPos = 0; 		//next character position
    uint32_t m_count = 0; 		//number of counter characters currently
    uint32_t m_length = 0;		//strlen()
};

LK_NAMESPACE_END

#endif
