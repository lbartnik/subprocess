is_windows <- function () subprocess:::is_windows()

is_linux <- function () subprocess:::is_linux()

is_mac <- function ()
{
  identical(tolower(Sys.info()[["sysname"]]), 'darwin')
}

is_solaris <- function()
{
  identical(tolower(Sys.info()[["sysname"]]), 'sunos')
}

# --- R child ----------------------------------------------------------

R_binary <- function ()
{
  binary <- ifelse(is_windows(), 'Rterm.exe', 'R')
  binary <- file.path(R.home("bin"), binary)
  stopifnot(file.exists(binary))
  binary
}

R_child <- function(args = '--slave', ...)
{
  handle <- spawn_process(R_binary(), args, ...)
  wait_until_appears(handle)
  handle
}


# --- OS interface -----------------------------------------------------

# wait_until_*
#
# Wait infinitey - on CRAN tests will timeout, locally we can always
# tell that something is wrong. This is because some systems are simply
# overloaded and it might take *minutes* for the processes to appear
# or exit.

wait_until_appears <- function (handle)
{
  while (!process_exists(handle)) {
    process_wait(handle, TIMEOUT_IMMEDIATE)
    if (process_state(handle) %in% c("exited", "terminated"))
      stop('failed to start ', handle$command, call. = FALSE)
    Sys.sleep(.25)
  }
  return(TRUE)
}


wait_until_exits <- function (handle)
{
  while (process_exists(handle)) {
    Sys.sleep(.25)
  }
  return(TRUE)
}


terminate_gracefully <- function (handle, message = "q('no')\n")
{
  if (!process_exists(handle)) return(TRUE)

  if (!is.null(message)) {
    process_write(handle, message)
  }

  process_close_input(handle)
  process_wait(handle)
  wait_until_exits(handle)
}
