#' Communicating with a Child Process
#' 
#' \code{process_read()} reads data from one of the child process' streams,
#' \emph{standard output} or \emph{standard error output}, and returns
#' it as a \code{character} vector.
#' 
#' If \code{flush=TRUE} in \code{process_read()} then the invocation of
#' the underlying \code{read()} \emph{system-call} will be repeated until
#' the pipe buffer is empty.
#' 
#' If \code{pipe} is set to either \code{PIPE_STDOUT} or \code{PIPE_STDERR},
#' the returned value is a single list with a single key, \emph{stdout} or
#' \emph{stderr}, respectively. If \code{pipe} is set to \code{PIPE_BOTH}
#' the returned \code{list} contains both keys. Values in the list are
#' \code{character} vectors of 0 or more elements, lines read from the
#' respective output stream of the child process.
#' 
#' For details on \code{timeout} see \code{\link{terminating}}.
#' 
#' @param handle Process handle obtained from \code{spawn_process}.
#' @param pipe Output stream identifier: \code{PIPE_STDOUT},
#'             \code{PIPE_STDERR} or \code{PIPE_BOTH}.
#' @param timeout Optional timeout in milliseconds.
#' @param flush If there is any data within the given \code{timeout}
#'              try again with \code{timeout=0} until C buffer is empty. 
#' 
#' @return \code{process_read} returns a \code{list} containing either of
#'         or both keys: \emph{stdout} and \emph{stderr}; the value is in
#'         both cases a \code{character} vector which contains lines of
#'         child's output.
#' 
#' @format \code{PIPE_STDOUT}, \code{PIPE_STDERR} and \code{PIPE_BOTH}
#'         are single \code{character} values.
#'
#' @rdname readwrite
#' @name readwrite
#' @export
#' 
process_read <- function (handle, pipe = PIPE_BOTH, timeout = TIMEOUT_IMMEDIATE, flush = TRUE)
{
  stopifnot(is_process_handle(handle))
  output <- .Call("C_process_read", handle$c_handle,
                  as.character(pipe), as.integer(timeout))

  is_output <- function (x) {
    return(is.list(output) && all(vapply(output, is.character, logical(1))))
  }
  paste0_list <- function (x, y) {
    z <- lapply(names(x), function (n) paste0(x[[n]], y[[n]]))
    `names<-`(z, names(x))
  }

  # needs to be a list of character vectors
  if (!is_output(output)) return(output)

  # there is some output, maybe there will be more?
  if (isTRUE(flush)) {
    while (TRUE) {
      more <- .Call("C_process_read", handle$c_handle, as.character(pipe), TIMEOUT_IMMEDIATE)
      if (!is_output(more) || all(vapply(more, nchar, integer(1)) == 0))
        break
      output <- paste0_list(output, more)
    }
  }

  # replace funny line ending and break into multiple lines
  output <- lapply(output, function (single_stream) {
    if (!length(single_stream)) return(character())
    single_stream <- gsub("\r", "", single_stream, fixed = TRUE)
    strsplit(single_stream, "\n", fixed = TRUE)[[1]]
  })
  
  # if asked for only one pipe return the vector, not the list
  if (identical(pipe, PIPE_STDOUT) || identical(pipe, PIPE_STDERR)) {
    return(output[[pipe]])
  }
  
  # return a lits
  return(output)
}


#' @description \code{process_write()} writes data into child's
#' \emph{standard input} stream.
#' 
#' @param message Input for the child process.
#' 
#' @return \code{process_write} returns the number of characters
#'         written.
#' 
#' @rdname readwrite
#' @name readwrite
#' @export
#' 
process_write <- function (handle, message)
{
  stopifnot(is_process_handle(handle))
  .Call("C_process_write", handle$c_handle, as.character(message))
}


#' @description \code{process_close_input()} closes the \emph{write} end
#' of the pipe whose \emph{read} end is the standard input stream of the
#' child process. This is a standard way to gracefully request the child
#' process to exit.
#'  
#' @rdname readwrite
#' @name readwrite
#' @export
#' 
process_close_input <- function (handle)
{
  stopifnot(is_process_handle(handle))
  .Call("C_process_close_input", handle$c_handle)
}



#' @description \code{PIPE_STDOUT}: read from child's standard output.
#' 
#' @rdname readwrite
#' @export
PIPE_STDOUT <- "stdout"


#' @description \code{PIPE_STDERR}: read from child's standard error
#' output.
#' 
#' @rdname readwrite
#' @export
PIPE_STDERR <- "stderr"


#' @description \code{PIPE_BOTH}: read from both child's output streams:
#' standard output and standard error output.
#' 
#' @rdname readwrite
#' @export
PIPE_BOTH <- "both"

