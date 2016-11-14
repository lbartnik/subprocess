#ifndef WIN_READER_H_GUARD
#define WIN_READER_H_GUARD

typedef enum { THREAD_RUNNING, THREAD_EXITED, THREAD_TERMINATED } reader_state_t; 

typedef struct chunk chunk_t; 

struct reader {
  HANDLE stream_handle;
  HANDLE thread_handle;
  CRITICAL_SECTION reader_lock;
  CONDITION_VARIABLE data_arrived;

  DWORD thread_id;
  reader_state_t state;
  DWORD error;

  // buffer
  chunk_t * head, * tail;
};

typedef struct reader reader_t;

int start_reader_thread (reader_t * _reader, HANDLE _stream);

int join_reader_thread (reader_t * _reader, int _timeout);

int get_next_chunk (reader_t * _reader, char * _output, size_t _count, int _timeout);

#endif /* WIN_READER_H_GUARD */
