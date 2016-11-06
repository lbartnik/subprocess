#include <string.h>

#include "subprocess.h"



int spawn_process (process_handle_t * _handle, const char * _command, char * const _arguments[], char * const _environment[])
{
  memset(_handle, 0, sizeof(process_handle_t));
  _handle->state = NOT_STARTED;

  /*
  BOOL WINAPI CreateProcess(
  _In_opt_    LPCTSTR               lpApplicationName,
  _Inout_opt_ LPTSTR                lpCommandLine,
  _In_opt_    LPSECURITY_ATTRIBUTES lpProcessAttributes,
  _In_opt_    LPSECURITY_ATTRIBUTES lpThreadAttributes,
  _In_        BOOL                  bInheritHandles,
  _In_        DWORD                 dwCreationFlags,
  _In_opt_    LPVOID                lpEnvironment,
  _In_opt_    LPCTSTR               lpCurrentDirectory,
  _In_        LPSTARTUPINFO         lpStartupInfo,
  _Out_       LPPROCESS_INFORMATION lpProcessInformation
  );
  */
  PROCESS_INFORMATION pi;
  CreateProcess(_command, &pi);

  /* close thread handle but keep the process handle */
  CloseHandle(pi.tHandle);
}

int teardown_process (process_handle_t * _handle)
{

}

ssize_t process_write (process_handle_t * _handle, const void * _buffer, size_t _count)
{

}

ssize_t process_read (process_handle_t * _handle, pipe_t _pipe, void * _buffer, size_t _count)
{

}

int process_poll (process_handle_t * _handle, int _wait)
{

}
