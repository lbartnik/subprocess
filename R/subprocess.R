#' @useDynLib subprocess
NULL

#' @export
spawn_process <- function (command, arguments = character(), environment = character())
{
  command <- as.character(command)
  .Call("C_process_spawn", command, c(command, as.character(arguments)), as.character(environment))
}

#' @export
end_process <- function (handle)
{
  .Call("C_process_end", handle)
}

#' @export
process_poll <- function (handle)
{
  .Call("C_process_poll", handle)
}

#' @export
process_read <- function (handle, pipe = "stdout")
{
  .Call("C_process_read", handle, as.character(pipe))
}

#' @export
process_write <- function (handle, message)
{
  .Call("C_process_write", handle, as.character(message))
}

