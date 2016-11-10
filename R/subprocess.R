#' @useDynLib subprocess
NULL

#' @export
spawn_process <- function (command, arguments = character(), environment = character(), workdir = "")
{
  command <- as.character(command)
  
  if (is_windows()) {
    command <- chartr('/', '\\', command)
  }
  
  .Call("C_process_spawn", command, c(command, as.character(arguments)),
        as.character(environment), as.character(workdir))
}

#' @export
process_terminate <- function (handle)
{
  .Call("C_process_terminate", handle)
}

#' @export
process_poll <- function (handle, timeout)
{
  .Call("C_process_poll", handle, as.integer(timeout))
}

#' @export
process_return_code <- function (handle)
{
  .Call("C_process_return_code", handle)
}


#' @export
process_wait <- function (handle, timeout)
{
  process_poll(handle, timeout)
  process_return_code(handle)
}


#' @export
process_read <- function (handle, pipe = "stdout", timeout = 0)
{
  output <- .Call("C_process_read", handle, as.character(pipe), as.integer(timeout))
  if (!is.character(output)) {
    return(output)
  }
  
  # replace funny line ending and break into multiple lines
  output <- gsub("\r", "", output, fixed = TRUE)
  strsplit(output, "\n", fixed = TRUE)[[1]]
}

#' @export
process_write <- function (handle, message)
{
  .Call("C_process_write", handle, as.character(message))
}

