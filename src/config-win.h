/*
 * Puts together Windows-related includes and (re)defines.
 */

#ifndef CONFIG_WIN_H_GUARD
#define CONFIG_WIN_H_GUARD


#if defined WIN64 || defined WIN32 || defined _MSC_VER
#define SUBPROCESS_WINDOWS
#endif


#ifdef SUBPROCESS_WINDOWS

/* MinGW defines this by default */
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif

/* enables thread synchronization API */
#define _WIN32_WINNT 0x0601

#include <windows.h>
#undef ERROR // R.h already defines this

#ifndef _In_
#define _In_
#endif

#ifndef _In_opt_
#define _In_opt_
#endif

#ifndef _Out_
#define _Out_
#endif


#endif /* SUBPROCESS_WINDOWS */

#endif /* CONFIG_WIN_H_GUARD */
