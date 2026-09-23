/**
 * @file	export.h
 * @author	Steven Toth <stoth@kernellabs.com>
 * @copyright	Copyright (c) 2025 Kernel Labs Inc. All Rights Reserved.
 * @brief	Public symbol export macros
 */

#ifndef _LIBKLSMPTE2064_EXPORT_H
#define _LIBKLSMPTE2064_EXPORT_H

#if defined(_WIN32)
# if defined(KLSMPTE2064_BUILD)
#  define KLSMPTE2064_API __declspec(dllexport)
# else
#  define KLSMPTE2064_API __declspec(dllimport)
# endif
#elif defined(__GNUC__) || defined(__clang__)
# define KLSMPTE2064_API __attribute__((visibility("default")))
#else
# define KLSMPTE2064_API
#endif

#endif /* _LIBKLSMPTE2064_EXPORT_H */
