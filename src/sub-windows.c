#include "subprocess.h"

#include <stdio.h>
#include <string.h>


static char * strjoin (char *const* _array);

void full_error_message (char * _buffer, size_t _length)
{
  DWORD code = GetLastError();
  DWORD ret = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, code,
	                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
	                        _buffer, _length, NULL);
}


// https://msdn.microsoft.com/en-us/library/windows/desktop/ms682499(v=vs.85).aspx
int spawn_process (process_handle_t * _handle, const char * _command, char *const _arguments[], char *const _environment[])
{
  memset(_handle, 0, sizeof(process_handle_t));
  _handle->state = NOT_STARTED;
  
  /* if the command is part of arguments, pass NULL to CreateProcess */
  if (!strcmp(_arguments[0], _command)) {
    _command = NULL;
  }

  /* put all arguments into one line */
  char * command_line = strjoin(_arguments);

  /* Windows magic */
  PROCESS_INFORMATION pi;
  memset(&pi, 0, sizeof(PROCESS_INFORMATION));

  // Set the bInheritHandle flag so pipe handles are inherited
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;

  HANDLE stdin_read   = NULL;
  HANDLE stdin_write  = NULL;
  HANDLE stdout_read  = NULL;
  HANDLE stdout_write = NULL;
  HANDLE stderr_read  = NULL;
  HANDLE stderr_write = NULL;

  #define CLOSE_HANDLES do {                     \
    if (stdin_read)   CloseHandle(stdin_read);   \
    if (stdin_write)  CloseHandle(stdin_write);  \
    if (stdout_read)  CloseHandle(stdout_read);  \
    if (stdout_write) CloseHandle(stdout_write); \
    if (stderr_read)  CloseHandle(stderr_read);  \
    if (stderr_write) CloseHandle(stderr_write); \
  } while (0);

  if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0)) { 
    return -1;
  }
  if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
    CLOSE_HANDLES;
    return -1;
  }
  if (!CreatePipe(&stderr_read, &stderr_write, &sa, 0)) {
    CLOSE_HANDLES;
    return -1;
  }

  // Ensure the write handle to the pipe for STDIN is not inherited. 
  if (!SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0)) {
    CLOSE_HANDLES;
    return -1;
  }

  STARTUPINFO si;
  memset(&si, 0, sizeof(STARTUPINFO));

  si.cb         = sizeof(STARTUPINFO);
  si.hStdError  = stderr_write;
  si.hStdOutput = stdout_write;
  si.hStdInput  = stdin_read;
  si.dwFlags    = STARTF_USESTDHANDLES;

  BOOL rc = CreateProcess(_command,         // lpApplicationName
                          command_line,     // lpCommandLine, command line
                          NULL,             // lpProcessAttributes, process security attributes
                          NULL,             // lpThreadAttributes, primary thread security attributes
                          TRUE,             // bInheritHandles, handles are inherited
                          CREATE_NO_WINDOW, // dwCreationFlags, creation flags
                          (void**)_environment, // lpEnvironment
                          NULL,         // lpCurrentDirectory
                          &si,          // lpStartupInfo
                          &pi);         // lpProcessInformation

  free(command_line);

  /* translate from Windows to Linux; -1 means error */
  if (rc != TRUE) {
    CLOSE_HANDLES;
    return -1;
  }

  /* close thread handle but keep the process handle */
  CloseHandle(pi.hThread);
//  CloseHandle(stdin_read);
//  CloseHandle(stdout_write);
//  CloseHandle(stderr_write);
  
  _handle->state        = RUNNING;
  _handle->child_handle = pi.hProcess;
  _handle->child_id     = pi.dwProcessId;
  _handle->pipe_stdin   = stdin_write;
  _handle->pipe_stdout  = stdout_read;
  _handle->pipe_stderr  = stderr_read;

  /* start reader threads */
  if (start_reader_thread (&_handle->stdout_reader, _handle->pipe_stdout) < 0) {
    CLOSE_HANDLES;
    return -1;
  }
  if (start_reader_thread (&_handle->stderr_reader, _handle->pipe_stderr) < 0) {
    TerminateThread(&_handle->stdout_reader, 0); // TODO is this safe? how else to handle it?
    CLOSE_HANDLES;
    return -1;
  }

  /* again, in Linux 0 is "good" */
  return 0;
}


int teardown_process (process_handle_t * _handle)
{
  if (_handle->state != RUNNING) {
    return 0;
  }

  process_poll(_handle, 1); // 1 means wait
  
  // Close OS handles 
  CloseHandle(_handle->child_handle);
  CloseHandle(_handle->pipe_stdin);
  CloseHandle(_handle->pipe_stdout);
  CloseHandle(_handle->pipe_stderr);

  // wait until threads exit
  join_reader_thread (&_handle->stdout_reader);
  join_reader_thread (&_handle->stderr_reader);

  return 0;
}

ssize_t process_write (process_handle_t * _handle, const void * _buffer, size_t _count)
{
  DWORD written = 0;
  BOOL rc = WriteFile(_handle->pipe_stdin, _buffer, _count, &written, NULL);
  
  if (!rc)
    return -1;
  
  return (ssize_t)written;
}

ssize_t process_read (process_handle_t * _handle, pipe_t _pipe, void * _buffer, size_t _count)
{
  // choose pipe
  reader_t * reader = NULL;
  if (_pipe == PIPE_STDOUT)
    reader = &_handle->stdout_reader;
  else if (_pipe == PIPE_STDERR)
    reader = &_handle->stderr_reader;
  else
    return -1;

  // event though return code is negative, buffer might hold data
  // this will happen if HeapFree() fails
  if (get_next_chunk (reader, _buffer, _count) < 0)
    return -1;
 
  return strlen(_buffer);
}

int process_poll (process_handle_t * _handle, int _wait)
{
  // to wait or not to wait?
  DWORD tm = 0;
  if (_wait > 0)
    tm = INFINITE;

  DWORD rc = WaitForSingleObject(_handle->child_handle, tm);
  
  // if already exited
  if (rc == WAIT_OBJECT_0) {
    DWORD status;
    GetExitCodeProcess(_handle->child_handle, &status);
 
    if (status == STILL_ACTIVE) {
      return -1;
    }
    
    _handle->return_code = (int)status;
    _handle->state = EXITED;
  }
 
  if (WAIT_TIMEOUT)
    return 0;
  
  // error while waiting
  return -1;
}

int process_terminate(process_handle_t * _handle)
{
  if (TerminateProcess(_handle->child_handle, 0) == 0)
    return -1;
  return 0;
}


/**
 * You have to Free() the buffer returned from this function
 * yourself - or let R do it, since we allocate it with Calloc().
 */
static char * strjoin (char *const* _array)
{
  /* total length is the sum of lengths plus spaces */
  size_t total_length = 0;
  char *const* ptr;

  for ( ptr = _array ; *ptr != NULL; ++ptr) {
    total_length += strlen(*ptr) + 1; /* +1 for space */
  }

  char * buffer = (char*)malloc(total_length + 1);

  /* combine all parts, put spaces between them */
  char * tail = buffer;
  for ( ptr = _array ; *ptr != NULL; ++ptr, ++tail) {
    size_t len = strlen(*ptr);
    strncpy(tail, *ptr, len);
    tail += len;
    *tail = ' ';
  }

  *tail = 0;

  return buffer;
}


#ifdef WINDOWS_TEST
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  const char * command = "x";
  char * args[] = { "x", NULL };
  char * env[]  = { NULL };
  
  process_handle_t handle;
  if (spawn_process(&handle, command, args, env) < 0) {
    fprintf(stderr, "error in spawn_process\n");
    exit(EXIT_FAILURE);
  }
  
  teardown_process(&handle);
  
  return 0;
}
#endif /* WINDOWS_TEST */
