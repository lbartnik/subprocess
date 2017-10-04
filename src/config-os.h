/*
 * Puts together Windows-related includes and (re)defines.
 */

#ifndef CONFIG_WIN_H_GUARD
#define CONFIG_WIN_H_GUARD


#if defined WIN64 || defined WIN32 || defined _MSC_VER || __MINGW32__
#define SUBPROCESS_WINDOWS
#elif defined(__MACH__)
#define SUBPROCESS_MACOS
#else
#define SUBPROCESS_LINUX
#endif


#ifdef SUBPROCESS_WINDOWS
#define EXPORT __declspec( dllexport )
#else
#define EXPORT 
#endif


/* When included in rapi.h, OS API causes compilation errors. */
#ifndef NO_SYSTEM_API

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

#undef min
#undef max
#undef length

typedef HANDLE process_handle_type;
typedef HANDLE pipe_handle_type;

constexpr pipe_handle_type HANDLE_CLOSED = nullptr;

#else /* !SUBPROCESS_WINDOWS */

#include <unistd.h>
typedef pid_t process_handle_type;
typedef int pipe_handle_type;

constexpr pipe_handle_type HANDLE_CLOSED = -1;

#endif /* SUBPROCESS_WINDOWS */

#endif /* NO_SYSTEM_API */

#endif /* CONFIG_WIN_H_GUARD */
