#include <windows.h>

struct stream_buffer {
  
};

typedef struct stream_buffer stream_buffer_t;

struct reader {
  HANDLE thread_handle;
  DWORD thread_id;
};

typedef struct reader reader_t;

DWORD WINAPI reader_function( LPVOID lpParam );

// https://msdn.microsoft.com/en-us/library/windows/desktop/ms682516(v=vs.85).aspx

int start_reader_thread (reader_t * _reader, HANDLE _stream, stream_buffer_t * _buffer)
{
  _reader->thread_handle = CreateThread( 
    NULL,                   // default security attributes
    0,                      // use default stack size  
    reader_function,       // thread function name
    _reader,          // argument to thread function 
    0,                      // use default creation flags 
    &_reader->thread_id);   // returns the thread identifier   

  return 0;
}
