#ifndef LOOKICONFIG_H
#define LOOKICONFIG_H


#define LOOKI_VERSION "0.0.1"
#define LOOKI_LITTLE_ENDIAN 1
#define LOOKI_NAMESPACE 1
#define LOOKI_BUILD_SHARED 1

#if LOOKI_NAMESPACE
#define LK_NAMESPACE_BEGIN namespace lk{
#define LK_NAMESPACE_END }
#define _LK lk
#else
#define LK_NAMESPACE_BEGIN
#define LK_NAMESPACE_END
#define _LK
#endif

#define UTF_CPP_CPLUSPLUS 202002L

#ifdef _WIN32
#   define L_DECL_EXPORT     __declspec(dllexport)
#   define L_DECL_IMPORT     __declspec(dllimport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#   define L_DECL_EXPORT     __attribute__((visibility("default")))
#   define L_DECL_IMPORT     __attribute__((visibility("default")))
#else
#   define L_DECL_EXPORT
#   define L_DECL_IMPORT
#endif

#define L_DECL_HIDDEN     __attribute__((visibility("hidden")))

#if LOOKI_BUILD_SHARED
#   if defined(LKBUILD_CORE)
#       define LKCOREDEF L_DECL_EXPORT
#   else
#       define LKCOREDEF L_DECL_IMPORT
#   endif
#   if defined(LKBUILD_NETWORK)
#       define LKNETWORKDEF L_DECL_EXPORT
#   else
#       define LKNETWORKDEF L_DECL_IMPORT
#   endif
#   if defined(LKBUILD_SQL)
#       define LKSQLDEF L_DECL_EXPORT
#   else
#       define LKSQLDEF L_DECL_IMPORT
#   endif
#   if defined(LKBUILD_AUDIO)
#       define LKAUDIODEF L_DECL_EXPORT
#   else
#       define LKAUDIODEF L_DECL_IMPORT
#   endif
#   if defined(LKBUILD_GUI)
#       define LKGUIDEF L_DECL_EXPORT
#   else
#       define LKGUIDEF L_DECL_IMPORT
#   endif
#   if defined(LKBUILD_WIDGETS)
#       define LKWIDGETSDEF L_DECL_EXPORT
#   else
#       define LKWIDGETSDEF L_DECL_IMPORT
#   endif
#   if defined(LKBUILD_SVG)
#       define LKSVGDEF L_DECL_EXPORT
#   else
#       define LKSVGDEF L_DECL_IMPORT
#   endif
#   if defined(LKBUILD_3D)
#       define LK3DDEF L_DECL_EXPORT
#   else
#       define LK3DDEF L_DECL_IMPORT
#   endif
#else
#   define LKCOREDEF
#   define LKNETWORKDEF
#   define LKSQLDEF
#   define LKAUDIODEF
#   define LKGUIDEF
#   define LKWIDGETSDEF
#   define LKSVGDEF
#   define LK3DDEF
#endif


#if defined(_MSC_VER)
#  define L_FUNC_INFO __FUNCSIG__
#  define L_NEVER_INLINE __declspec(noinline)
#  define L_ALWAYS_INLINE __forceinline
#  define WIN32_LEAN_AND_MEAN
#  pragma warning( disable: 4251 )
#elif defined(__GNUC__)
#  define L_FUNC_INFO ""
#  define L_NEVER_INLINE __attribute__((noinline))
#  define L_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#  define L_FUNC_INFO ""
#  define L_NEVER_INLINE
#  define L_ALWAYS_INLINE inline
#endif

#include <stdint.h>
#include <thread>


#endif // LOOKICONFIG_H
