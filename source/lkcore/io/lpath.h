#ifndef LPATH_H
#define LPATH_H

#include <filesystem>
#include <string>
#include <string_view>

LK_NAMESPACE_BEGIN

/**
 * 路径编码约定
 *
 *  - 库对外接口里的 std::string 路径，一律视为 UTF-8
 *  - 内部真实路径统一用 std::filesystem::path 承载
 *  - 打开文件时优先使用 fstream / ifstream 的 path 重载
 *    （MSVC 会走宽字符实现，可正确处理任意 Unicode 路径）
 *  - Win32 API 使用 path::c_str()（宽字符）
 *  - 只有必须调用 ANSI 的旧 C 接口时，才使用 toLocal()
 *
 * 这样在 Windows 上即使工程/资源位于中文目录，也不会因为
 * std::string 是本地编码（GBK）而打开失败。
 */
namespace lpath {

/** path -> UTF-8 字符串（给 C 接口、网络、日志、配置使用） */
inline std::string toUtf8(const std::filesystem::path &p)
{
    const std::u8string u8 = p.u8string();
    return std::string(reinterpret_cast<const char *>(u8.data()), u8.size());
}

/** UTF-8 字符串 -> path（从配置、第三方接口、资源名转回路径） */
inline std::filesystem::path fromUtf8(std::string_view s)
{
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t *>(s.data()), s.size()));
}

/** path -> 本地编码字符串：仅用于无法接收宽字符的旧接口，慎用 */
inline std::string toLocal(const std::filesystem::path &p)
{
    return p.string();
}

/** 构造路径：宽字符 / 本地窄字符都可以，交给 filesystem 解析 */
inline std::filesystem::path make(const std::string &s)
{
    return std::filesystem::path(s);
}

inline std::filesystem::path make(const std::wstring &s)
{
    return std::filesystem::path(s);
}

}

LK_NAMESPACE_END

#endif // LPATH_H
