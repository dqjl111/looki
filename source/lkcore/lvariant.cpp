#include "lvariant.h"
#include <cmath>
#include <memory.h>
#include <format>
#include "lstringutil.h"
LK_NAMESPACE_BEGIN


bool LVariant::VariantBase::operator == (const VariantBase &rhs) const
{
    if (type != rhs.type) return false;
    else
    {
        switch (type)
        {
        case VarBool:
        case VarChar:
        case VarInt8:
        case VarUInt8:
        case VarInt16:
        case VarUInt16:
        case VarInt32:
        case VarUInt32:{
            return data.i32 == rhs.data.i32;
        }
        case VarInt64:
        case VarUInt64:{
            return data.i64 == rhs.data.i64;
        }
        case VarFloat:{
            return std::abs(data.f32 - rhs.data.f32)
                   < std::numeric_limits<float>::epsilon();
        }
        case VarDouble:{
            return std::abs(data.f64 - rhs.data.f64)
                   < std::numeric_limits<double>::epsilon();
        }
        case VarTimePoint:{
            return data.tp == rhs.data.tp;
        }
        case VarByteArray:
        case VarString:
        case VarWString:{
            if (data.bin.len != rhs.data.bin.len){
                return false;
            }
            else{
                return memcmp(data.bin.bin, rhs.data.bin.bin, data.bin.len) == 0;
            }
        }
        case VarVariants:{
            return *data.var == *rhs.data.var;
        }
        case VarCbor:{
            return *data.cbor == *rhs.data.cbor;
        }
        case VarLIVarPtr:{
            return ivar == rhs.ivar;
        }
        default:{
            return data.bin.bin == rhs.data.bin.bin;
        }
        }
    }
}
bool LVariant::VariantBase::operator != (const VariantBase &rhs) const
{
    return !(*this == rhs);
}

LVariant::VariantBase::VariantBase(const void *mem, uint32_t len) : type(VarByteArray)
{
    initUD();
    setData(mem, len);
}

LVariant::VariantBase::VariantBase(const char *str, uint32_t len) : type(VarString)
{
    initUD();
    setData(str, len + 1U);
}

LVariant::VariantBase::VariantBase(const wchar_t *v, uint32_t s) : type(VarWString){
    initUD();
    setData(v, (s+1)*sizeof(wchar_t));
}

LVariant::VariantBase::VariantBase(const LVariants &v) : type(VarVariants)
{
    initUD();
    data.var = new LVariants(v);
}
LVariant::VariantBase::VariantBase(LVariants &&v) : type(VarVariants) {
    initUD();
    data.var = new LVariants(std::move(v));
}

LVariant::VariantBase::VariantBase(const LCbor& v): type(VarCbor)
{
    initUD();
    data.cbor = new LCbor(v);
}

LVariant::VariantBase::VariantBase(const std::shared_ptr<LIVar> v): type(VarLIVarPtr)
{
    initUD();
    ivar = v;
}

void LVariant::VariantBase::clear(void)
{
    switch (type)
    {
    case VarByteArray:
    case VarString:
    case VarWString:{
        if(data.bin.bin != nullptr){
            delete[] static_cast< char * >(data.bin.bin);
            data.bin.bin = nullptr;
        }
        break;
    }
    case VarVariants:{
        if(data.var != nullptr){
            delete data.var;
            data.var = nullptr;
        }
        break;
    }
    case VarCbor:{
        if(data.cbor != nullptr){
            delete data.cbor;
            data.cbor = nullptr;
        }
        break;
    }
    default:
        break;
    }
    ivar = nullptr;
    type = VarNull;
    initUD();
}

void LVariant::VariantBase::setVariants(const LVariants &v)
{
    clear();
    type = VarVariants;
    data.var = new LVariants(v);
}

void LVariant::VariantBase::setCbor(const LCbor &v)
{
    clear();
    type = VarCbor;
    data.cbor = new LCbor(v);
}
void LVariant::VariantBase::setByteArray(const char *v, uint32_t s)
{
    clear();
    if(v!= nullptr){
        type = VarByteArray;
        setData(v, s);
    }
}
void LVariant::VariantBase::setString(const char *v, uint32_t s)
{
    clear();
    if(v != nullptr){
        type = VarString;
        setData(v, s+1);
    }
}

void LVariant::VariantBase::setWString(const wchar_t *v, uint32_t s)
{
    clear();
    if(v != nullptr){
        type = VarWString;
        setData(v, (s+1)*sizeof(wchar_t));
    }
}

void LVariant::VariantBase::setData(const void *mem, uint32_t len)
{
    data.bin.bin	= new char[len];
    data.bin.len	= len;
    memcpy(data.bin.bin, mem, len);
}

void LVariant::VariantBase::cpyObjc(const VariantBase &rhs)
{
    switch (type = rhs.type)
    {
    case VarBool:
    case VarChar:
    case VarInt8:
    case VarUInt8:
    case VarInt16:
    case VarUInt16:
    case VarInt32:
    case VarUInt32:{
        data.i32 = rhs.data.i32;
        break;
    }
    case VarInt64:
    case VarUInt64:{
        data.i64 = rhs.data.i64;
        break;
    }
    case VarFloat:{
        data.f32 = rhs.data.f32;
        break;
    }
    case VarDouble:{
        data.f64 = rhs.data.f64;
        break;
    }
    case VarTimePoint:{
        data.tp = rhs.data.tp;
        break;
    }
    case VarByteArray:
    case VarString:
    case VarWString:{
        setData(rhs.data.bin.bin, rhs.data.bin.len);
        break;
    }
    case VarVariants:{
        data.var = new LVariants(*rhs.data.var);
        break;
    }
    case VarCbor:{
        data.cbor = new LCbor(*rhs.data.cbor);
        break;
    }
    case VarLIVarPtr:{
        ivar = rhs.ivar;
    }
    default:{
        data = rhs.data;
        break;
    }
    }
}
void LVariant::VariantBase::movObjc(VariantBase&& rhs)
{
    ivar = rhs.ivar;
    type = rhs.type;
    data = rhs.data;
    rhs.type = VarNull;
}

//void LVariant::VariantBase::initUD()
//{
//    memset((char*)(&data), 0, sizeof(UD));
//}


std::string LVariant::toString(bool* isOk) const
{
    if(isOk != nullptr){
        *isOk = true;
    }
    switch(m_data.type){
    case VarNull:{
        return "";
    }
    case VarBool:{		// boolean
        return m_data.data.b01 ?  "true" : "false";
    }
    case VarChar:{		// 字符
        return std::string(&m_data.data.c08, 1);
    }
    case VarInt8:{		// 有符号8位有意义
        int32_t t = (int8_t)m_data.data.c08;
        return std::to_string(t);
    }
    case VarUInt8:{		// 无符合8位有意义
        uint32_t t = (uint8_t)m_data.data.c08;
        return std::to_string(t);
    }
    case VarInt16:{		// 有符号16位有意义
        int32_t t = (int16_t)m_data.data.i16;
        return std::to_string(t);
    }
    case VarUInt16:{		// 无符合16位有意义
        uint32_t t = (uint16_t)m_data.data.i16;
        return std::to_string(t);
    }
    case VarInt32:{		// 有符号32位有意义
        return std::to_string((int32_t)m_data.data.i32);
    }
    case VarUInt32:{		// 无符合32位有意义
        return std::to_string((uint32_t)m_data.data.i32);
    }
    case VarInt64:{		// 有符号64位有意义
        return std::to_string((int64_t)m_data.data.i64);
    }
    case VarUInt64:{		// 无符合64位有意义
        return std::to_string((uint64_t)m_data.data.i64);
    }
    case VarFloat:{	// 32位浮点
        return std::to_string(m_data.data.f32);
    }
    case VarDouble:{	// 64位浮点
        return std::to_string(m_data.data.f64);
    }
    case VarTimePoint:{
        return std::format("{0:%F}T{0:%T}Z", LSysClock::from_time_t(m_data.data.tp));
    }
    case VarByteArray: // 8位二进制数据
    case VarString:{	// 8位字符串数据
        return {LVariant::getString(), LVariant::getByteSize() - 1U};
    }
    case VarWString:{		// 字符串数据块'\0'结尾
        std::wstring t = {LVariant::getWString(), LVariant::getByteSize()/sizeof(wchar_t) - 1U};
        return LStringUtil::wideToUtf8(t);
    }
    case VarVariants:{	// 嵌套的变量
        if(isOk != nullptr){
            *isOk = false;
        }
        return "";
    }
    case VarCbor:{
        if(m_data.data.cbor != nullptr){
            return m_data.data.cbor->toString();
        }else{
            return "";
        }
    }
    case VarLIVarPtr:{
        if(m_data.ivar != nullptr){
            return m_data.ivar->toCbor().toString();
        }
        return "";
    }
    default:{
        if(isOk != nullptr){
            *isOk = false;
        }
        return "";
    }
    }
}

std::wstring LVariant::toWString(bool* isOk) const
{
    if(isOk != nullptr){
        *isOk = true;
    }
    switch(m_data.type){
    case VarNull:{
        return std::wstring();
    }
    case VarBool:{		// boolean
        return m_data.data.b01 ?  L"true" : L"false";
    }
    case VarChar:{		// 字符
        wchar_t t = m_data.data.c08;
        return std::wstring(&t, 1);
    }
    case VarInt8:{		// 有符号8位有意义
        int32_t t = (int8_t)m_data.data.c08;
        return std::to_wstring(t);
    }
    case VarUInt8:{		// 无符合8位有意义
        uint32_t t = (uint8_t)m_data.data.c08;
        return std::to_wstring(t);
    }
    case VarInt16:{		// 有符号16位有意义
        int32_t t = (int16_t)m_data.data.i16;
        return std::to_wstring(t);
    }
    case VarUInt16:{		// 无符合16位有意义
        uint32_t t = (uint16_t)m_data.data.i16;
        return std::to_wstring(t);
    }
    case VarInt32:{		// 有符号32位有意义
        return std::to_wstring((int32_t)m_data.data.i32);
    }
    case VarUInt32:{		// 无符合32位有意义
        return std::to_wstring((uint32_t)m_data.data.i32);
    }
    case VarInt64:{		// 有符号64位有意义
        return std::to_wstring((int64_t)m_data.data.i64);
    }
    case VarUInt64:{		// 无符合64位有意义
        return std::to_wstring((uint64_t)m_data.data.i64);
    }
    case VarFloat:{	// 32位浮点
        return std::to_wstring(m_data.data.f32);
    }
    case VarDouble:{	// 64位浮点
        return std::to_wstring(m_data.data.f64);
    }
    case VarTimePoint:{
        return std::format(L"{0:%F}T{0:%T}Z", LSysClock::from_time_t(m_data.data.tp));
    }
    case VarByteArray: // 8位二进制数据
    case VarString:{	// 8位字符串数据
        std::string t = {LVariant::getString(), LVariant::getByteSize() - 1U};
        return LStringUtil::utf8ToWide(t);
    }
    case VarWString:{		// 字符串数据块'\0'结尾
        return {LVariant::getWString(), LVariant::getByteSize()/sizeof(wchar_t) - 1U};
    }
    case VarVariants:{	// 嵌套的变量
        return std::wstring();
    }
    case VarCbor:{
        if(m_data.data.cbor != nullptr){
            return LStringUtil::utf8ToWide(m_data.data.cbor->toString());
        }else{
            return std::wstring();
        }
    }
    case VarLIVarPtr:{
        if(m_data.ivar != nullptr){
            return LStringUtil::utf8ToWide(m_data.ivar->toCbor().toString());
        }
        return std::wstring();
    }
    default:{
        if(isOk != nullptr){
            *isOk = false;
        }
        return std::wstring();
    }
    }
}

LCbor LVariant::toCbor(bool* isOk) const
{
    if(isOk != nullptr){
        *isOk = true;
    }

    switch(m_data.type){
    case VarNull:{
        return LCbor();
    }
    case VarBool:{		// boolean
        if(m_data.data.b01){
            return LCbor(true);
        }else{
            return LCbor(false);
        }
    }
    case VarChar:
    case VarInt8:
    case VarInt16:
    case VarInt32:
    case VarInt64:{
        return LCbor(toInt64());
    }
    case VarUInt8:
    case VarUInt16:
    case VarUInt32:
    case VarUInt64:
    case VarTimePoint:{
        return LCbor(toUInt64());
    }
    case VarFloat:
    case VarDouble:{
        return LCbor(toDouble());
    }
    case VarByteArray:{
        LCbor::binary bin;
        if(m_data.data.bin.bin != nullptr){
            bin.resize(m_data.data.bin.len);
            memcpy(bin.data(), m_data.data.bin.bin, m_data.data.bin.len);
        }
        return LCbor(bin);
    }
    case VarString:{
        return LCbor(get<std::string>());
    }
    case VarWString:{
        return LCbor(LStringUtil::wideToUtf8(get<std::wstring>()));
    }
    case VarVariants:{	// 嵌套的变量
        LCbor::array arr;
        if(m_data.data.var != nullptr){
            for(const LVariant& var : *m_data.data.var){
                arr.push_back(var.toCbor());
            }
        }
        return arr;
    }
    case VarCbor:{
        if(m_data.data.cbor != nullptr){
            return *m_data.data.cbor;
        }
        return LCbor();
    }
    case VarLIVarPtr:{
        if(m_data.ivar != nullptr){
            return m_data.ivar->toCbor();
        }
        return LCbor();
    }
    default:{
        if(isOk != nullptr){
            *isOk = false;
        }
        return 0;
    }
    }
}

bool LVariant::toBool(bool* isOk) const
{
    return toInt64(isOk) != 0;
}

int64_t LVariant::toInt64(bool* isOk) const
{
    if(isOk != nullptr){
        *isOk = true;
    }

    switch(m_data.type){
    case VarNull:{
        return 0;
    }
    case VarBool:{		// boolean
        if(m_data.data.b01){
            return 1;
        }else{
            return 0;
        }
    }
    case VarChar:
    case VarInt8:{
        return (int8_t)m_data.data.c08;
    }
    case VarUInt8:{
        return (uint8_t)m_data.data.c08;
    }
    case VarInt16:{
        return (int16_t)m_data.data.i16;
    }
    case VarUInt16:{
        return (uint16_t)m_data.data.i16;
    }
    case VarInt32:{
        return (int32_t)m_data.data.i32;
    }
    case VarUInt32:{
        return (uint32_t)m_data.data.i32;
    }
    case VarInt64:{
        return (int64_t)m_data.data.i64;
    }
    case VarUInt64:{
        return (uint64_t)m_data.data.i64;
    }
    case VarFloat:{
        return std::llround(m_data.data.f32);
    }
    case VarDouble:{
        return std::llround(m_data.data.f64);
    }
    case VarTimePoint:{
        return m_data.data.tp;
    }
    case VarByteArray:
    case VarString:
    case VarWString:{	// 8位二进制数据
        if(m_data.data.bin.len == 0){
            return 0;
        }else{
            return (uint64_t)m_data.data.bin.bin;
        }
    }
    case VarVariants:{	// 嵌套的变量
        return (uint64_t)m_data.data.var;
    }
    case VarCbor:{
        if(m_data.data.cbor != nullptr){
            return m_data.data.cbor->toInt64();
        }else{
            return false;
        }
    }
    case VarLIVarPtr:{
        return (uint64_t)m_data.ivar.get();
    }
    default:{
        if(isOk != nullptr){
            *isOk = false;
        }
        return 0;
    }
    }
}

uint64_t LVariant::toUInt64(bool *isOk) const
{
    return (uint64_t)toInt64(isOk);
}

int LVariant::toInt(bool *isOk) const
{
    return (int)toInt64(isOk);
}

uint32_t LVariant::toUInt(bool *isOk) const
{
    return (uint32_t)toInt64(isOk);
}

double LVariant::toDouble(bool* isOk) const
{
    switch(m_data.type){
    case VarFloat:{
        if(isOk != nullptr){
            *isOk = true;
        }
        return m_data.data.f32;
    }
    case VarDouble:{
        if(isOk != nullptr){
            *isOk = true;
        }
        return m_data.data.f64;
    }
    default:{
        return toInt64(isOk);
    }
    }
}

float LVariant::toFloat(bool *isOk) const
{
    switch(m_data.type){
    case VarFloat:{
        if(isOk != nullptr){
            *isOk = true;
        }
        return m_data.data.f32;
    }
    case VarDouble:{
        if(isOk != nullptr){
            *isOk = true;
        }
        return m_data.data.f64;
    }
    default:{
        return toInt64(isOk);
    }
    }
}

bool LVariant::isNull() const
{
    switch(m_data.type){
    case VarNull:{
        return true;
    }
    case VarByteArray:
    case VarString:
    case VarWString:{	// 8位二进制数据
        return m_data.data.bin.bin == nullptr;
    }
    case VarVariants:{	// 嵌套的变量
        return m_data.data.var == nullptr;
    }
    case VarCbor:{
        return m_data.data.cbor == nullptr;
    }
    case VarLIVarPtr:{
        return m_data.ivar == nullptr;
    }
    default:{
        return false;
    }
    }
}

uint64_t LVariant::hashValue() const
{
    switch(m_data.type){
    case VarNull:{
        return 0;
    }
    case VarBool:{		// boolean
        if(m_data.data.b01){
            return 0x371819;
        }else{
            return 0x371818;
        }
    }
    case VarChar:
    case VarInt8:
    case VarUInt8:
    case VarInt16:
    case VarUInt16:
    case VarInt32:
    case VarUInt32:
    case VarInt64:
    case VarUInt64:
    case VarTimePoint:{
        return 0x811890U + toInt64();
    }
    case VarFloat:
    case VarDouble:{
        return std::hash<double>{}(toDouble());
    }
    case VarByteArray:
    case VarString:
    case VarWString:{	// 8位二进制数据
        return std::hash<std::string>{}(toString());
    }
    case VarVariants:{	// 嵌套的变量
        return toInt64();
    }
    case VarCbor:{
        return std::hash<std::string>{}(toString());
    }
    case VarLIVarPtr:{
        return toInt64();
    }
    default:{
        return 0;
    }
    }
}

const LVariant &LVariant::staticNull()
{
    const static LVariant nullVar;
    return nullVar;
}



LK_NAMESPACE_END

