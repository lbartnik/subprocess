#' Communicating with a Child Process
#' 
#' \code{process_read} reads data from one of the child process' streams,
#' \emph{standard output} or \emph{standard error output}, and returns
#' it as a \code{character} vector.
#' 
#' If \code{flush=TRUE} in \code{process_read()} then the invocation of
#' the underlying \code{read()} \emph{system-call} will be repeated until
#' the pipe buffer is empty.
#' 
#' For details on \code{timeout} see \code{\link{terminating}}.
#' 
#' @param handle Process handle obtained from \code{spawn_process}.
#' @param pipe Output stream name, \code{"stdout"} or \code{"stderr"}.
#' @param timeout Optional timeout in milliseconds.
#' @param flush If there is any data within the given \code{timeout}
#'              try again with \code{timeout=0} until C buffer is empty. 
#' 
#' @return \code{process_read} returns a \code{character} vector
#'         which contains lines of child's output.
#' 
#' @rdname readwrite
#' @name readwrite
#' @export
#' 
process_read <- function (handle, pipe = "stdout", timeout = TIMEOUT_IMMEDIATE, flush = TRUE)
{
  stopifnot(is_process_handle(handle))
  output <- .Call("C_process_read", handle$c_handle,
                  as.character(pipe), as.integer(timeout))

  if (!is.character(output)) {
    return(output)
  }
  
  # there is some output, maybe there will be more?
  if (isTRUE(flush)) {
    while (TRUE) {
      more <- .Call("C_process_read", handle$c_handle, as.character(pipe), TIMEOUT_IMMEDIATE)
      if (!is.character(more) || !nchar(more))
        break
      output <- paste0(output, more)
    }
  }

  # replace funny line ending and break into multiple lines
  output <- gsub("\r", "", output, fixed = TRUE)
  strsplit(output, "\n", fixed = TRUE)[[1]]
}


#' @description \code{process_write} writes data into child's
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

