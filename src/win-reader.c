#include <windows.h>

#include "win-reader.h"

#define BUFFER_SIZE 4096

struct chunk {
  struct chunk * next;
  size_t length;
  char data[0]; 
};


// https://msdn.microsoft.com/en-us/library/windows/desktop/ms682516(v=vs.85).aspx
DWORD WINAPI reader_function (LPVOID data);
int acquire_mutex(reader_t * _reader);
int release_mutex(reader_t * _reader);



int start_reader_thread (reader_t * _reader, HANDLE _stream)
{
  // create the access mutex
  _reader->mutex = CreateMutex(
	  NULL,              // default security attributes
	  FALSE,             // initially not owned
	  NULL);             // unnamed mutex

  if (_reader->mutex == NULL) {
	return -1;
  }

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
	CloseHandle(_reader->mutex);
    return -1;
  }

  _reader->stream_handle = _stream;
  _reader->state = THREAD_RUNNING;
  _reader->error = 0;
  _reader->head  = NULL;
  _reader->tail  = NULL;

  return 0;
}


int join_reader_thread (reader_t * _reader)
{
  WaitForSingleObject(_reader->thread_handle, INFINITE);
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

	if (acquire_mutex(reader) < 0) {
	  goto error;
	}

    chunk_t ** append_to = &reader->head;
    while (*append_to) {
      append_to = &((*append_to)->next);
    }

    *append_to   = new_chunk;
    reader->tail = new_chunk;

    if (release_mutex(reader) < 0) {
	  goto error;
    }

  } while (42);

  return 0;

error:
  reader->state = THREAD_TERMINATED;
  reader->error = GetLastError();
  return -1;
}


// will zero-terminate the string copied into _output
int get_next_chunk (reader_t * _reader, char * _output, size_t _count)
{
  if (_count <= 1) {
    return -1;
  }

  if (acquire_mutex(_reader) < 0) {
	return -1;
  }

  int ret = 0;
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
  if (release_mutex(_reader) < 0) {
	  return -1;
  }
  return ret;
}


int acquire_mutex(reader_t * _reader)
{
  DWORD rc = WaitForSingleObject(_reader->mutex, INFINITE);
  if (rc == WAIT_OBJECT_0)
	return 0;
  return -1;
}


int release_mutex(reader_t * _reader)
{
  if (ReleaseMutex(_reader->mutex) != TRUE)
    return -1;
  return 0;
}
