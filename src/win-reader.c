#include <windows.h>
#include <synchapi.h>

/* min_gw that comes with Rtools 3.4 doesn't have these functions */
WINBASEAPI VOID WINAPI InitializeConditionVariable(PCONDITION_VARIABLE ConditionVariable);
WINBASEAPI VOID WINAPI WakeConditionVariable(PCONDITION_VARIABLE ConditionVariable);
WINBASEAPI BOOL WINAPI SleepConditionVariableCS(PCONDITION_VARIABLE ConditionVariable, PCRITICAL_SECTION CriticalSection, DWORD dwMilliseconds);


#include "win-reader.h"

#define BUFFER_SIZE 4096

struct chunk {
  struct chunk * next;
  size_t length;

  /*
   * C99 6.7.2.1, 16: As a special case, the last element of a structure with
   * more than one named member may have an incomplete array type; this is
   * called a flexible array member.
   */
  char data[]; 
};


// https://msdn.microsoft.com/en-us/library/windows/desktop/ms682516(v=vs.85).aspx
DWORD WINAPI reader_function (LPVOID data);


int start_reader_thread (reader_t * _reader, HANDLE _stream)
{
  // initialize synchronization mechanism
  InitializeConditionVariable (&_reader->data_arrived);
  InitializeCriticalSection (&_reader->reader_lock);

  // create the thread
  _reader->thread_handle = CreateThread( 
    NULL,                   // default security attributes
    0,                      // use default stack size  
    reader_function,        // thread function name
    _reader,                // argument to thread function 
    0,                      // use default creation flags 
    &_reader->thread_id);   // returns the thread identifier   

  // returns NULL on error
  if (_reader->thread_handle == NULL) {
    return -1;
  }

  _reader->stream_handle = _stream;
  _reader->state = THREAD_RUNNING;
  _reader->error = 0;
  _reader->head  = NULL;
  _reader->tail  = NULL;

  return 0;
}


int join_reader_thread (reader_t * _reader, int _timeout)
{
  if (_timeout)
    _timeout = INFINITE;

  WaitForSingleObject(_reader->thread_handle, _timeout);
  CloseHandle(_reader->thread_handle);
  
  return 0;
}


DWORD WINAPI reader_function (LPVOID data)
{
  reader_t * reader = (reader_t*)data;
  char buffer[BUFFER_SIZE];

  DWORD read = 0;
  BOOL rc = TRUE;
  do {
    rc = ReadFile(reader->stream_handle, buffer, sizeof(buffer), &read, NULL);
	if (rc != TRUE || read == 0)
	  break;

    chunk_t * new_chunk = (chunk_t*) HeapAlloc(GetProcessHeap(), 0, sizeof(chunk_t) + read);

    if (new_chunk == NULL) {
      goto error;
    }

    // initialize this chunk
    new_chunk->next   = NULL;
    new_chunk->length = read;
    CopyMemory(new_chunk->data, buffer, read);

	/* enter critical section - manipulating the list of chunks */
	EnterCriticalSection(&reader->reader_lock);

    chunk_t ** append_to = &reader->head;
    while (*append_to) {
      append_to = &((*append_to)->next);
    }

    *append_to   = new_chunk;
    reader->tail = new_chunk;

	/* leave critical section and signal into get_next_chunk() */
	LeaveCriticalSection(&reader->reader_lock);
	WakeConditionVariable(&reader->data_arrived);
  } while (42);

  return 0;

error:
  reader->state = THREAD_TERMINATED;
  reader->error = GetLastError();
  return -1;
}


// will zero-terminate the string copied into _output
int get_next_chunk (reader_t * _reader, char * _output, size_t _count, int _timeout)
{
  if (_count <= 1) {
    return -1;
  }

  int ret = 0;

  /* begin synchronized section */
  EnterCriticalSection(&_reader->reader_lock);

  /* if there's nothing there yet try waiting */
  FILETIME start, current;
  GetSystemTimeAsFileTime(&start);
  while (!_reader->head) {
    if (SleepConditionVariableCS(&_reader->data_arrived, &_reader->reader_lock, _timeout))
      break;

	GetSystemTimeAsFileTime(&current);
	_timeout -= (current.dwLowDateTime - start.dwLowDateTime)/10000;
    if (_timeout < 0)
      break;
  }

  /* if there is nothing here, then we run out of time */
  chunk_t * chunk = _reader->head;
  if (chunk == NULL) {
    *_output = 0;
	goto finish;
  }

  // partial
  if (chunk->length+1 > _count) {
    size_t to_copy = _count-1;
    CopyMemory(_output, chunk->data, to_copy);
    _output[to_copy] = 0;
    MoveMemory(chunk->data, chunk->data+to_copy, chunk->length-to_copy);
    chunk->length -= to_copy;
  }
  // full
  else {
    CopyMemory(_output, chunk->data, chunk->length);
    _output[chunk->length] = 0;
    _reader->head = chunk->next;
    if (_reader->head == NULL) {
      _reader->tail = NULL;
    }

    BOOL rc = HeapFree(GetProcessHeap(), 0, chunk);
    if (rc != TRUE) {
      ret = -1;
	  goto finish;
    }
  }

finish:
  LeaveCriticalSection(&_reader->reader_lock);
  return ret;
}
