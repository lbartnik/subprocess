#include <windows.h>

#include "win-reader.h"

static const int BUFFER_SIZE = 4096;

struct chunk {
  struct chunk * next;
  size_t length;
  char data[0]; 
};


// https://msdn.microsoft.com/en-us/library/windows/desktop/ms682516(v=vs.85).aspx
DWORD WINAPI reader_function (LPVOID data);




int start_reader_thread (reader_t * _reader, HANDLE _stream)
{
  _reader->thread_handle = CreateThread( 
    NULL,                   // default security attributes
    0,                      // use default stack size  
    reader_function,       // thread function name
    _reader,          // argument to thread function 
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
    chunk_t * new_chunk = (chunk_t*) HeapAlloc(GetProcessHeap(), 0, sizeof(chunk_t) + read);

    if (new_chunk == NULL) {
      reader->state = THREAD_TERMINATED;
      reader->error = GetLastError();
      return -1;
    }

    // initialize this chunk
    new_chunk->next   = NULL;
    new_chunk->length = read;
    CopyMemory(new_chunk->data, buffer, read);

    chunk_t ** append_to = &reader->head;
    while (*append_to) {
      append_to = &((*append_to)->next);
    }

    *append_to   = new_chunk;
    reader->tail = new_chunk;

  } while (rc != TRUE || read == 0);

  if (!rc)
    return -1;
 
  return 0;
}


// will zero-terminate the string copied into _output
int get_next_chunk (reader_t * _reader, char * _output, size_t _count)
{
  if (_count <= 1) {
    return -1;
  }

  chunk_t * chunk = _reader->head;
  if (chunk == NULL) {
    *_output = 0;
    return 0;
  }

  // partial
  if (chunk->length+1 > _count) {
    size_t to_copy = _count-1;
    CopyMemory(chunk->data, _output, to_copy);
    _output[to_copy] = 0;
    MoveMemory(chunk->data, chunk->data+to_copy, chunk->length-to_copy);
    chunk->length -= to_copy;
  }
  // full
  else {
    CopyMemory(chunk->data, _output, chunk->length);
    _output[chunk->length] = 0;
    _reader->head = chunk->next;
    if (_reader->head == NULL) {
      _reader->tail = NULL;
    }

    BOOL rc = HeapFree(GetProcessHeap(), 0, chunk);
    if (rc != TRUE) {
      return -1;
    }
  }

  return 0;
}


