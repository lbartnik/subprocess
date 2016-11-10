#' @useDynLib subprocess
NULL


#' Start a new child process.
#' 
#' @details
#' \code{environment} can be passed as a \code{character} vector whose
#' elements take the form \code{"NAME=VALUE"}, a named \code{character}
#' vector or a named \code{list}.
#' 
#' \code{workdir} is the path to the directory where the new process is
#' ought to be started. \code{NULL} and \code{""} mean that working
#' directory is inherited from the parent.
#' 
#' @param command Path to the executable.
#' @param arguments Optional arguments for the program.
#' @param environment Optional environment.
#' @param workdir Optional new working directory.
#' 
#' @return A process handle.
#' 
#' @export
spawn_process <- function (command, arguments = character(), environment = character(), workdir = "")
{
  command <- as.character(command)
  
  # apps from C:\Rtools\bin accepted "/" as delimiter but even R itself
  # didn't; I'm not sure why this happened but it seems the replacement
  # below is necessary
  if (is_windows()) {
    command <- chartr('/', '\\', command)
  }
  
  .Call("C_process_spawn", command, c(command, as.character(arguments)),
        as.character(environment), as.character(workdir))
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
