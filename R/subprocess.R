#' @useDynLib subprocess
NULL

#' @export
spawn_process <- function (command, arguments = character(), environment = character())
{
  command <- as.character(command)
  .Call("C_process_spawn", command, c(command, as.character(arguments)), as.character(environment))
}

#' @export
process_terminate <- function (handle)
{
  .Call("C_process_terminate", handle)
}

#' @export
process_poll <- function (handle)
{
  .Call("C_process_poll", handle)
}

#' @export
process_read <- function (handle, pipe = "stdout")
{
  x <- .Call("C_process_read", handle, as.character(pipe))
  if (!is.character(x)) {
    return(x)
  }
  strsplit(gsub("\r", "", x, fixed = TRUE), "\n", fixed = TRUE)[[1]]
}

#' @export
process_write <- function (handle, message)
{
  .Call("C_process_write", handle, as.character(message))
}

