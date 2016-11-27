is_windows <- function ()
{
  identical(tolower(Sys.info()[["sysname"]]), 'windows')
}

is_linux <- function ()
{
  identical(tolower(Sys.info()[["sysname"]]), 'linux')
}

# --- R child ----------------------------------------------------------

R_binary <- function ()
{
  binary <- ifelse(is_windows(), 'R.exe', 'R')
  binary <- file.path(R.home("bin"), binary)
  stopifnot(file.exists(binary))
  binary
}

R_child <- function(...)
{
  handle <- spawn_process(R_binary(), '--slave', ...)
  # give the child a chance to start; Windows tends to be considerably slower
  while (!process_exists(handle)) {
    if (process_poll(handle) %in% c("exited", "terminated"))
      stop('failed to start ', R_binary(), call. = FALSE)
  }
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
    rc <- system2("ps", c("--pid", as.character(pid)), stdout = NULL, 
                  stderr = NULL)
    return(rc == 0)
  }
}
