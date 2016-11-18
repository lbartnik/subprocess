#include "subprocess.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>


/* min_gw that comes with Rtools 3.4 doesn't have these functions */
WINBASEAPI BOOL WINAPI CancelIoEx(_In_ HANDLE hFile, _In_opt_ LPOVERLAPPED lpOverlapped);
WINBASEAPI BOOL WINAPI CancelSynchronousIo(_In_ HANDLE hThread);

/*
 * There are probably many places that need to be adapted to make this
 * package Unicode-ready. One place that for sure needs a change is
 * the prepare_environment() function. It currently assumes a double
 * NULL environment block delimiter; if UNICODE is enabled, it will
 * be a four-NULL block.
 *
 * See this page for more details:
 * https://msdn.microsoft.com/en-us/library/windows/desktop/ms682425(v=vs.85).aspx
 */
#ifdef UNICODE
#error "This package is not ready for UNICODE"
#endif



static char * strjoin (char *const* _array, char _sep);

int pipe_redirection(process_handle_t * _process, STARTUPINFO * _si);

int file_redirection(process_handle_t * _process, STARTUPINFO * _si);

char * prepare_environment(char *const* _environment);

HANDLE create_job_for_process ();




int full_error_message (char * _buffer, size_t _length)
{
  DWORD code = GetLastError();
  DWORD ret = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, code,
	                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
	                        _buffer, (DWORD)_length, NULL);
  if (ret == 0) return -1;
  return 0;
}


int duplicate_handle(HANDLE src, HANDLE * dst)
{
  BOOL rc = DuplicateHandle(GetCurrentProcess(), src,
	                        GetCurrentProcess(),
	                        dst,      // Address of new handle.
                            0, FALSE, // Make it uninheritable.
                            DUPLICATE_SAME_ACCESS);
  if (rc != TRUE)
	return -1;
  return 0;
}

//
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms682499(v=vs.85).aspx
// https://support.microsoft.com/en-us/kb/190351
//
int spawn_process (process_handle_t * _handle, const char * _command, char *const _arguments[],
	               char *const _environment[], const char * _workdir, termination_mode_t _termination_mode)
{
  memset(_handle, 0, sizeof(process_handle_t));
  _handle->state = NOT_STARTED;
  
  /* if the command is part of arguments, pass NULL to CreateProcess */
  if (!strcmp(_arguments[0], _command)) {
    _command = NULL;
  }
  /* if the environment is empty (most cases) don't bother with passing
   * just this single NULLed element */
  char * environment = NULL;
  if (*_environment != NULL) {
    environment = prepare_environment(_environment);
  }

  /* put all arguments into one line */
  char * command_line = strjoin(_arguments, ' ');

  /* Windows magic */
  PROCESS_INFORMATION pi;
  memset(&pi, 0, sizeof(PROCESS_INFORMATION));

  STARTUPINFO si;
  memset(&si, 0, sizeof(STARTUPINFO));

  int pipe_rc = 0;

  // TODO expose this as a debugging interface
  if (1) pipe_rc = pipe_redirection(_handle, &si);
    else pipe_rc = file_redirection(_handle, &si);

  if (pipe_rc < 0) {
    CloseHandle(si.hStdInput);
    CloseHandle(si.hStdOutput);
    CloseHandle(si.hStdError);
    return -1;
  }

  // creation flags
  DWORD creation_flags = CREATE_NO_WINDOW;

  // if termination is set to "group", create a job for this process;
  // attempt at it at the beginning and not even try to start the process
  // if it fails
  if (_termination_mode == TERMINATION_GROUP) {
    creation_flags |= CREATE_SUSPENDED | CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB;
    _handle->process_job = create_job_for_process();
    if (_handle->process_job == NULL) {
      return -1;
    }
  }

  BOOL rc = CreateProcess(_command,         // lpApplicationName
                          command_line,     // lpCommandLine, command line
                          NULL,             // lpProcessAttributes, process security attributes
                          NULL,             // lpThreadAttributes, primary thread security attributes
                          TRUE,             // bInheritHandles, handles are inherited
                          creation_flags,   // dwCreationFlags, creation flags
                          environment,      // lpEnvironment
                          _workdir,         // lpCurrentDirectory
                          &si,              // lpStartupInfo
                          &pi);             // lpProcessInformation

  HeapFree(GetProcessHeap(), 0, command_line);
  HeapFree(GetProcessHeap(), 0, environment);

  CloseHandle(si.hStdInput);
  CloseHandle(si.hStdOutput);
  CloseHandle(si.hStdError);

  /* translate from Windows to Linux; -1 means error */
  if (rc != TRUE) {
    return -1;
  }

  /* if termination mode is "group" add process to the job; see here
   * for more details:
   * https://msdn.microsoft.com/en-us/library/windows/desktop/ms684161(v=vs.85).aspx
   * 
   * "After a process is associated with a job, by default any child
   *  processes it creates using CreateProcess are also associated
   *  with the job."
   */
  if (_termination_mode == TERMINATION_GROUP) {
    BOOL rc = AssignProcessToJobObject(_handle->process_job, pi.hProcess);
    if (rc == FALSE) {
      int error_code = GetLastError();
      _handle->state = RUNNING;
      process_terminate(_handle);
      SetLastError(error_code);
      return -1;
    }

    if (ResumeThread(pi.hThread) == (DWORD)-1) {
      return -1;
    }
  }

  /* close thread handle but keep the process handle */
  CloseHandle(pi.hThread);
  
  _handle->state            = RUNNING;
  _handle->child_handle     = pi.hProcess;
  _handle->child_id         = pi.dwProcessId;
  _handle->termination_mode = _termination_mode;

  /* start reader threads */
  if (start_reader_thread (&_handle->stdout_reader, _handle->pipe_stdout) < 0) {
    return -1;
  }
  if (start_reader_thread (&_handle->stderr_reader, _handle->pipe_stderr) < 0) {
    TerminateThread(&_handle->stdout_reader, 0); // TODO is this safe? how else to handle it?
    return -1;
  }

  /* again, in Linux 0 is "good" */
  return 0;
}


// see: https://blogs.msdn.microsoft.com/oldnewthing/20131209-00/?p=2433
HANDLE create_job_for_process ()
{
  HANDLE job_handle = CreateJobObject(NULL, NULL);
  
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
  memset(&info, 0, sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION));

  info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  SetInformationJobObject(job_handle,
                          JobObjectExtendedLimitInformation,
                          &info, sizeof(info));
  
  return job_handle;
}


int pipe_redirection(process_handle_t * _process, STARTUPINFO * _si)
{
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

  if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0)) { 
    goto error;
  }
  if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
    goto error;
  }
  if (!CreatePipe(&stderr_read, &stderr_write, &sa, 0)) {
    goto error;
  }

  // Ensure the write handle to the pipe for STDIN is not inherited. 
  if (!SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0)) {
    goto error;
  }

  // Create new output read handle and the input write handles. Set
  // the Properties to FALSE. Otherwise, the child inherits the
  // properties and, as a result, non-closeable handles to the pipes
  // are created.
  if (duplicate_handle(stdin_write, &_process->pipe_stdin) < 0) {
    goto error;
  }
  if (duplicate_handle(stdout_read, &_process->pipe_stdout) < 0) {
    goto error;
  }
  if (duplicate_handle(stderr_read, &_process->pipe_stderr) < 0) {
    goto error;
  }

  // close those we don't want to inherit
  CloseHandle(stdin_write);
  CloseHandle(stdout_read);
  CloseHandle(stderr_read);

  _si->cb = sizeof(STARTUPINFO);
  _si->hStdError = stderr_write;
  _si->hStdOutput = stdout_write;
  _si->hStdInput = stdin_read;
  _si->dwFlags = STARTF_USESTDHANDLES;

  return 0;

error:
  if (stdin_read)   CloseHandle(stdin_read);
  if (stdin_write)  CloseHandle(stdin_write);
  if (stdout_read)  CloseHandle(stdout_read);
  if (stdout_write) CloseHandle(stdout_write);
  if (stderr_read)  CloseHandle(stderr_read);
  if (stderr_write) CloseHandle(stderr_write);
  return -1;
}


int file_redirection(process_handle_t * _process, STARTUPINFO * _si)
{
  HANDLE input  = NULL;
  HANDLE output = NULL;
  HANDLE error  = NULL;

  input = CreateFile("C:/Windows/TEMP/subprocess.in", // name of the write
                     GENERIC_READ,           // open for writing
                     0,                      // do not share
                     NULL,                   // default security
                     OPEN_EXISTING,          // create new file only
                     FILE_ATTRIBUTE_NORMAL,  // normal file
                     NULL);                  // no attr. template

  if (input == INVALID_HANDLE_VALUE) {
    goto error;
  }

  output = CreateFile("C:/Windows/TEMP/subprocess.out", // name of the write
                      GENERIC_WRITE,          // open for writing
                      0,                      // do not share
                      NULL,                   // default security
                      CREATE_ALWAYS,          // create new file only
                      FILE_ATTRIBUTE_NORMAL,  // normal file
                      NULL);                  // no attr. template

  if (output == INVALID_HANDLE_VALUE) { 
    goto error;
  }

  if ((duplicate_handle(output, &_process->pipe_stdout) < 0) ||
	  (duplicate_handle(output, &_process->pipe_stderr) < 0) ||
	  (duplicate_handle(output, &error) < 0))
  {
    goto error;
  }

  _si->cb = sizeof(STARTUPINFO);
  _si->hStdError = error;
  _si->hStdOutput = output;
  _si->hStdInput = input;
  _si->dwFlags = STARTF_USESTDHANDLES;

  return 0;

error:
  if (input) CloseHandle(input);
  if (output) CloseHandle(output);
  if (error) CloseHandle(error);
  return -1;
}



int teardown_process (process_handle_t * _handle)
{
  if (_handle->state != RUNNING) {
    return 0;
  }

  int rc = 1;

  process_poll(_handle, -1); // -1 means wait infinitely

  // Close OS handles 
  CloseHandle(_handle->child_handle);
  CloseHandle(_handle->pipe_stdin);

  // terminate reader threads
  rc &= CancelSynchronousIo(&_handle->stdout_reader);
  rc &= CancelSynchronousIo(&_handle->stderr_reader); 
  rc &= CancelIoEx(_handle->pipe_stdout, NULL);
  rc &= CancelIoEx(_handle->pipe_stderr, NULL);
  if (!rc) {
    // TODO produce a warning
  }

  // wait until threads exit
  join_reader_thread (&_handle->stdout_reader, INFINITE);
  join_reader_thread (&_handle->stderr_reader, INFINITE);

  // finally close read handles
  CloseHandle(_handle->pipe_stdout);
  CloseHandle(_handle->pipe_stderr);

  return 0;
}

ssize_t process_write (process_handle_t * _handle, const void * _buffer, size_t _count)
{
  DWORD written = 0;
  BOOL rc = WriteFile(_handle->pipe_stdin, _buffer, (DWORD)_count, &written, NULL);
  
  if (!rc)
    return -1;
  
  // give the reader a chance to receive the response before we return
  SwitchToThread();

  return (ssize_t)written;
}

ssize_t process_read (process_handle_t * _handle, pipe_t _pipe, void * _buffer, size_t _count, int _timeout)
{
  // choose pipe
  reader_t * reader = NULL;
  if (_pipe == PIPE_STDOUT)
    reader = &_handle->stdout_reader;
  else if (_pipe == PIPE_STDERR)
    reader = &_handle->stderr_reader;
  else
    return -1;

  // identify timeout
  if (_timeout < 0)
    _timeout = INFINITE;

  // event though return code is negative, buffer might hold data
  // this will happen if HeapFree() fails
  if (get_next_chunk (reader, _buffer, _count, _timeout) < 0)
    return -1;
 
  return (ssize_t)strlen(_buffer);
}

int process_poll (process_handle_t * _handle, int _timeout)
{
  if (_handle->state != RUNNING)
	return 0;

  // to wait or not to wait?
  if (_timeout < 0)
    _timeout = INFINITE;

  DWORD rc = WaitForSingleObject(_handle->child_handle, _timeout);
  
  // if already exited
  if (rc == WAIT_OBJECT_0) {
    DWORD status;
    if (GetExitCodeProcess(_handle->child_handle, &status) == FALSE) {
      return -1;
	}
 
    if (status == STILL_ACTIVE) {
      return -1;
    }
    
    _handle->return_code = (int)status;
    _handle->state = EXITED;

	return 0;
  }
 
  if (rc == WAIT_TIMEOUT)
    return 0;
  
  // error while waiting
  return -1;
}


// compare with: https://github.com/andreisavu/python-process/blob/master/killableprocess.py
int process_terminate(process_handle_t * _handle)
{
  // first make sure it's even still running
  if (process_poll(_handle, 0) < 0)
    return -1;
  if (_handle->state != RUNNING)
    return 0;

  // first terminate the child process; if mode is "group" terminate
  // the whole job
  if (_handle->termination_mode == TERMINATION_GROUP) {
    if (TerminateJobObject(_handle->process_job, 127) == FALSE) {
      return -1;
    }
    if (CloseHandle(_handle->process_job) == FALSE) {
      return -1;
    }
  }
  else {
    // now terminate just the process itself
    HANDLE to_terminate = OpenProcess(PROCESS_TERMINATE, FALSE, _handle->child_id);
    if (!to_terminate)
      return -1;

    BOOL rc = TerminateProcess(to_terminate, 127);
    CloseHandle(to_terminate);
    if (rc == FALSE)
      return -1;
  }

  // clean up
  teardown_process(_handle);
  _handle->state = TERMINATED;

  // if any of the threads reported an error
  if (_handle->stdout_reader.state == THREAD_TERMINATED) {
    SetLastError(_handle->stdout_reader.error);
    return -1;
  }
  if (_handle->stderr_reader.state == THREAD_TERMINATED) {
    SetLastError(_handle->stderr_reader.error);
    return -1;
  }

  // everything went smoothly
  return 0;
}


int process_kill (process_handle_t * _handle)
{
  return process_terminate (_handle);
}


int process_send_signal (process_handle_t * _handle, int _signal)
{
  if (_signal == SIGTERM) {
    return process_terminate(_handle);
  }
  if (_signal == CTRL_C_EVENT || _signal == CTRL_BREAK_EVENT) {
    BOOL rc = GenerateConsoleCtrlEvent(_signal, (DWORD)_handle->child_id);
    if (rc == FALSE)
      return -1;
  }

  // unsupported `signal` value
  SetLastError(ERROR_INVALID_SIGNAL_NUMBER);
  return -1;
}


/**
 * You have to Free() the buffer returned from this function
 * yourself - or let R do it, since we allocate it with Calloc().
 */
static char * strjoin (char *const* _array, char _sep)
{
  /* total length is the sum of lengths plus spaces */
  size_t total_length = 0;
  char *const* ptr;

  for ( ptr = _array ; *ptr != NULL; ++ptr) {
    total_length += strlen(*ptr) + 1; /* +1 for space */
  }

  char * buffer = (char*)HeapAlloc(GetProcessHeap(), 0, total_length + 2); /* +2 for double NULL */

  /* combine all parts, put spaces between them */
  char * tail = buffer;
  for ( ptr = _array ; *ptr != NULL; ++ptr, ++tail) {
    size_t len = strlen(*ptr);
	strncpy_s(tail, total_length+1, *ptr, len);
    tail += len;
    *tail = _sep;
  }

  *tail++ = 0;
  *tail   = 0;

  return buffer;
}


int find_double_0 (const char * _str)
{
  int length = 0;
  while (*_str++ || *_str) {
	++length;
  }
  return length;
}

char * prepare_environment (char *const* _environment)
{
  char * current_env = GetEnvironmentStrings();
  char * new_env = strjoin(_environment, 0);

  int length_1 = find_double_0(current_env);
  int length_2 = find_double_0(new_env);

  char * combined_env = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, length_1 + length_2 + 5);
  CopyMemory(combined_env, current_env, length_1);
  CopyMemory(combined_env+length_1+1, new_env, length_2);

  FreeEnvironmentStrings(current_env);
  HeapFree(GetProcessHeap(), 0, new_env);
  return combined_env;
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
