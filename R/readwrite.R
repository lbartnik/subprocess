#' Communicating with a Child Process
#' 
#' \code{process_read} reads data from one of the child process' streams,
#' \emph{standard output} or \emph{standard error output}, and returns
#' it as a \code{character} vector.
#' 
#' For details on \code{timeout} see \code{\link{terminating}}.
#' 
#' @param handle Process handle obtained from \code{spawn_process}.
#' @param pipe Output stream name, \code{"stdout"} or \code{"stderr"}.
#' @param timeout Optional timeout in milliseconds.
#' 
#' @return \code{process_read} returns a \code{character} vector
#'         which contains lines of child's output.
#' 
#' @rdname readwrite
#' @name readwrite
#' @export
#' 
process_read <- function (handle, pipe = "stdout", timeout = TIMEOUT_IMMEDIATE)
{
  output <- .Call("C_process_read", handle, as.character(pipe), as.integer(timeout))
  if (!is.character(output)) {
    return(output)
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
  .Call("C_process_write", handle, as.character(message))
}

