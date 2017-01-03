is_windows <- function () subprocess:::is_windows()

is_linux <- function () subprocess:::is_linux()

is_mac <- function ()
{
  identical(tolower(Sys.info()[["sysname"]]), 'darwin')
}


# --- R child ----------------------------------------------------------

R_binary <- function ()
{
  binary <- ifelse(is_windows(), 'R.exe', 'R')
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

process_exists <- function (handle)
{
  pid <- ifelse (is_process_handle(handle), as.integer(handle$c_handle), handle)
  
  if (is_windows()) {
    output <- system2("tasklist", stdout = TRUE, stderr = TRUE)
    return(length(grep(as.character(pid), output, fixed = TRUE)) > 0)
  }
  else {
    flag <- ifelse(is_mac(), "-p", "--pid")
    rc <- system2("ps", c(flag, as.character(pid)), stdout = NULL, 
                  stderr = NULL)
    return(rc == 0)
  }
}


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
    Sys.sleep(1)
  }
  return(TRUE)
}


wait_until_exits <- function (handle)
{
  while (process_exists(handle)) {
    Sys.sleep(1)
  }
  return(TRUE)
}

