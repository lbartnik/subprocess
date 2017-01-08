#include "subprocess.h"

#include <cstdio>
#include <cstring>
#include <sstream>

#include <signal.h>


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


namespace subprocess {

static char * strjoin (char *const* _array, char _sep);

char * prepare_environment(char *const* _environment);



string strerror (int _code, const string & _message)
{
  vector<char> buffer(BUFFER_SIZE, '\0');
  DWORD ret = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, _code,
                            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                            buffer.data(), (DWORD)buffer.size() - 1, NULL);

  std::stringstream message;
  message << _message << ": " 
          << ((ret > 0) ? buffer.data() : "system error message could not be fetched");

  return message.str().substr(0, message.str().find_last_not_of("\r\n\t"));
}



/* --- wrappers for system API -------------------------------------- */


static void DuplicateHandle(HANDLE src, HANDLE * dst)
{
  if (::DuplicateHandle(GetCurrentProcess(), src,
                        GetCurrentProcess(),
                        dst,      // Address of new handle.
                        0, FALSE, // Make it uninheritable.
                        DUPLICATE_SAME_ACCESS)
      != TRUE)
  {
    throw subprocess_exception(GetLastError(), "cannot duplicate handle");
  }
}


static void CloseHandle (HANDLE & _handle)
{
  if (_handle == HANDLE_CLOSED) return;

  auto rc = ::CloseHandle(_handle);
  _handle = HANDLE_CLOSED;

  if (rc == FALSE) {
    throw subprocess_exception(::GetLastError(), "could not close handle");
  }
}


/* ------------------------------------------------------------------ */

process_handle_t::process_handle_t ()
  : process_job(nullptr), child_handle(nullptr),
    pipe_stdin(HANDLE_CLOSED), pipe_stdout(HANDLE_CLOSED), pipe_stderr(HANDLE_CLOSED),
    child_id(0), state(NOT_STARTED), return_code(0),
    termination_mode(TERMINATION_GROUP)
{}


/* ------------------------------------------------------------------ */


/*
 * Wrap redirections in a class to achieve RAII.
 */
struct StartupInfo {

  struct pipe_holder {
    HANDLE read, write;

    pipe_holder(SECURITY_ATTRIBUTES & sa)
      : read(nullptr), write(nullptr)
    {
      if (!::CreatePipe(&read, &write, &sa, 0)) {
        throw subprocess_exception(GetLastError(), "could not create pipe");
      }
    }

    ~pipe_holder () {
      CloseHandle(read);
      CloseHandle(write);
    }
  };

  StartupInfo (process_handle_t & _process) {
    memset(&info, 0, sizeof(STARTUPINFO));
    pipe_redirection(_process);
  }

  ~StartupInfo () {
    CloseHandle(info.hStdInput);
    CloseHandle(info.hStdOutput);
    CloseHandle(info.hStdError);
  }


  /*
   * Set standard input/output redirection via pipes.
   */
  void pipe_redirection (process_handle_t & _process)
  {
    // Set the bInheritHandle flag so pipe handles are inherited
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    pipe_holder in(sa), out(sa), err(sa);

    // Ensure the write handle to the pipe for STDIN is not inherited. 
    if (!::SetHandleInformation(in.write, HANDLE_FLAG_INHERIT, 0)) {
      throw subprocess_exception(GetLastError(), "could not set handle information");
    }

    // Create new output read handle and the input write handles. Set
    // the Properties to FALSE. Otherwise, the child inherits the
    // properties and, as a result, non-closeable handles to the pipes
    // are created.
    DuplicateHandle(in.write, &_process.pipe_stdin);
    DuplicateHandle(out.read, &_process.pipe_stdout);
    DuplicateHandle(err.read, &_process.pipe_stderr);

    // prepare the info object
    info.cb = sizeof(STARTUPINFO);
    info.hStdError  = err.write;
    info.hStdOutput = out.write;
    info.hStdInput  = in.read;
    info.dwFlags = STARTF_USESTDHANDLES;

    // set to null so that destructor does not close those handles
    err.write = nullptr;
    out.write = nullptr;
    in.read   = nullptr;
  }


  /*
   * Alternative (debug) redirection to/from files.
   */
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

    DuplicateHandle(output, &_process->pipe_stdout);
    DuplicateHandle(output, &_process->pipe_stderr);
    DuplicateHandle(output, &error);
  
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

  /**
   * The actual startup info object.
   */
  STARTUPINFO info;
 
};


// see: https://blogs.msdn.microsoft.com/oldnewthing/20131209-00/?p=2433
static HANDLE CreateAndAssignChildToJob (HANDLE _process)
{
  HANDLE job_handle = ::CreateJobObject(NULL, NULL);
  if (!job_handle) {
    throw subprocess_exception(::GetLastError(), "group termination: could not create a new job");
  }
  
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
  ::memset(&info, 0, sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION));

  info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (FALSE == ::SetInformationJobObject(job_handle,
                                         JobObjectExtendedLimitInformation,
                                         &info, sizeof(info)))
  {
    CloseHandle(job_handle);
    throw subprocess_exception(::GetLastError(), "could not set job information");
  }

  if (::AssignProcessToJobObject(job_handle, _process) == FALSE) {
    CloseHandle(job_handle);
    throw subprocess_exception(::GetLastError(), "group termination: could not assign process to a job");
  }

  return job_handle;
}


//
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms682499(v=vs.85).aspx
// https://support.microsoft.com/en-us/kb/190351
//
void process_handle_t::spawn (const char * _command, char *const _arguments[],
                             char *const _environment[], const char * _workdir,
                             termination_mode_type _termination_mode)
{
  /* if the command is part of arguments, pass NULL to CreateProcess */
  if (!strcmp(_arguments[0], _command)) {
    _command = NULL;
  }
  /* if the environment is empty (most cases) don't bother with passing
   * just this single NULLed element */
  char * environment = NULL;
  if (_environment != NULL && *_environment != NULL) {
    environment = prepare_environment(_environment);
  }

  /* put all arguments into one line */
  char * command_line = strjoin(_arguments, ' ');

  /* Windows magic */
  PROCESS_INFORMATION pi;
  memset(&pi, 0, sizeof(PROCESS_INFORMATION));

  StartupInfo startupInfo(*this);

  // creation flags
  DWORD creation_flags = CREATE_NEW_PROCESS_GROUP;

  // if termination is set to "group", create a job for this process;
  // attempt at it at the beginning and not even try to start the process
  // if it fails
  if (_termination_mode == TERMINATION_GROUP) {
    creation_flags |= CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB;
  }

  BOOL rc = ::CreateProcess(_command,         // lpApplicationName
                            command_line,     // lpCommandLine, command line
                            NULL,             // lpProcessAttributes, process security attributes
                            NULL,             // lpThreadAttributes, primary thread security attributes
                            TRUE,             // bInheritHandles, handles are inherited
                            creation_flags,   // dwCreationFlags, creation flags
                            environment,      // lpEnvironment
                            _workdir,         // lpCurrentDirectory
                            &startupInfo.info,// lpStartupInfo
                            &pi);             // lpProcessInformation

  ::HeapFree(::GetProcessHeap(), 0, command_line);
  ::HeapFree(::GetProcessHeap(), 0, environment);

  /* translate from Windows to Linux; -1 means error */
  if (!rc) {
    throw subprocess_exception(::GetLastError(), "could not create process");
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
    // if cannot create and/or assign process to a new job, terminate
    try {
      process_job = CreateAndAssignChildToJob(pi.hProcess);
    }
    catch (subprocess_exception & e) {
      state = RUNNING;
      terminate();
      throw;
    }

    if (::ResumeThread(pi.hThread) == (DWORD)-1) {
      throw subprocess_exception(::GetLastError(), "could not resume thread");
    }
  }

  /* close thread handle but keep the process handle */
  CloseHandle(pi.hThread);

  state            = RUNNING;
  child_handle     = pi.hProcess;
  child_id         = pi.dwProcessId;
  termination_mode = _termination_mode;
}


/* --- process::shutdown -------------------------------------------- */


void process_handle_t::shutdown ()
{
  if (child_handle == HANDLE_CLOSED)
    return;

  // close the process handle
  CloseHandle(child_handle);
  CloseHandle(process_job);

  // close read & write handles
  CloseHandle(pipe_stdin);
  CloseHandle(pipe_stdout);
  CloseHandle(pipe_stderr);
}


/* --- process::write ----------------------------------------------- */

size_t process_handle_t::write (const void * _buffer, size_t _count)
{
  DWORD written = 0;
  if (!::WriteFile(pipe_stdin, _buffer, (DWORD)_count, &written, NULL)) {
    throw subprocess_exception(::GetLastError(), "could not write to child process");
  }

  return static_cast<size_t>(written);
}


/* --- process::read ------------------------------------------------ */


size_t process_handle_t::read (pipe_type _pipe, int _timeout)
{
  stdout_.clear();
  stderr_.clear();
  
  ULONGLONG start = GetTickCount64();
  int timediff, sleep_time = 100; /* by default sleep 0.1 seconds */
  
  if (_timeout >= 0) {
    sleep_time = _timeout / 10;
  }
  
  do {
    size_t rc1 = 0, rc2 = 0;
    if (_pipe & PIPE_STDOUT) rc1 = stdout_.read(pipe_stdout);
    if (_pipe & PIPE_STDERR) rc2 = stderr_.read(pipe_stderr);

    // if anything has been read or no timeout is specified return now
    if (rc1 > 0 || rc2 > 0 || sleep_time == 0) {
      return std::max(rc1, rc2);
    }
    
    // sleep_time is now guaranteed to be positive
    Sleep(sleep_time);
    timediff = (int)(GetTickCount64() - start);
    
  } while (_timeout < 0 || timediff < _timeout);

  // out of time
  return 0;
}


/* --- process::close_input ----------------------------------------- */

void process_handle_t::close_input ()
{
  if (pipe_stdin == HANDLE_CLOSED) {
    throw subprocess_exception(EALREADY, "child's standard input already closed");
  }

  CloseHandle(pipe_stdin);
  pipe_stdin = HANDLE_CLOSED;
}


/* ------------------------------------------------------------------ */


void process_handle_t::wait (int _timeout)
{
  if (!child_handle || state != RUNNING)
    return;

  // to wait or not to wait?
  if (_timeout == TIMEOUT_INFINITE)
    _timeout = INFINITE;

  DWORD rc = ::WaitForSingleObject(child_handle, _timeout);
  
  // if already exited
  if (rc == WAIT_OBJECT_0) {
    DWORD status;
    if (::GetExitCodeProcess(child_handle, &status) == FALSE) {
      throw subprocess_exception(::GetLastError(), "could not read child exit code");
    }
 
    if (status == STILL_ACTIVE) {
      return;
    }
    
    return_code = (int)status;
    state = EXITED;
  }
  else if (rc != WAIT_TIMEOUT) {
    throw subprocess_exception(::GetLastError(), "wait for child process failed");
  }
}


/* --- process::terminate ------------------------------------------- */


// compare with: https://github.com/andreisavu/python-process/blob/master/killableprocess.py
void process_handle_t::terminate()
{
  // first make sure it's even still running
  wait(TIMEOUT_IMMEDIATE);
  if (!child_handle || state != RUNNING)
    return;

  // first terminate the child process; if mode is "group" terminate
  // the whole job
  if (termination_mode == TERMINATION_GROUP) {
    BOOL rc = ::TerminateJobObject(process_job, 127);
    CloseHandle(process_job);
    if (rc == FALSE) {
      throw subprocess_exception(::GetLastError(), "could not terminate child job");
    }
  }
  else {
    // now terminate just the process itself
    HANDLE to_terminate = ::OpenProcess(PROCESS_TERMINATE, FALSE, child_id);
    if (!to_terminate) {
      throw subprocess_exception(::GetLastError(), "could open child process for termination");
    }

    BOOL rc = ::TerminateProcess(to_terminate, 127);
    CloseHandle(to_terminate);
    if (rc == FALSE) {
      throw subprocess_exception(::GetLastError(), "could not terminate child process");
    }
  }

  // clean up
  wait(TIMEOUT_INFINITE);
  state = TERMINATED;

  // release handles
  shutdown();
}


/* --- process::kill ------------------------------------------------ */


void process_handle_t::kill ()
{
  terminate();
}


/* --- process::send_signal ----------------------------------------- */


/*
 * This is tricky. Look here for details:
 * http://codetitans.pl/blog/post/sending-ctrl-c-signal-to-another-application-on-windows
 *
 * WARNING! I cannot make it work. It seems that there is no way of
 * sending Ctrl+C to the child process without killing the parent R
 * at the same time.
 *
 * From:
 * https://msdn.microsoft.com/en-us/library/windows/desktop/ms683155(v=vs.85).aspx
 *
 * Generates a CTRL+C signal. This signal cannot be generated for process groups.
 * If dwProcessGroupId is nonzero, this function will succeed, but the CTRL+C
 * signal will not be received by processes within the specified process group.
 */
void process_handle_t::send_signal (int _signal)
{
  if (_signal == SIGTERM) {
    return terminate();
  }

  // unsupported `signal` value
  if (_signal != CTRL_C_EVENT && _signal != CTRL_BREAK_EVENT) {
    throw subprocess_exception(ERROR_INVALID_SIGNAL_NUMBER, "signal not supported");
  }

  if (::GenerateConsoleCtrlEvent(_signal, child_id) == FALSE) {
    throw subprocess_exception(::GetLastError(), "signal could not be sent");
  }
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

} /* namespace subprocess */



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
