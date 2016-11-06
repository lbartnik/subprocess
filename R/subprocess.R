#' @useDynLib subprocess
NULL

#' @export
spawn_process <- function (command, arguments = character(), environment = character())
{
  .Call("C_spawn_process", as.character(command), as.character(arguments), as.character(environment))
}

#' @export
end_process <- function (handle)
{
  .Call("C_end_process", handle)
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

