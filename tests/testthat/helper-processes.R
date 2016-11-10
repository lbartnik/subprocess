is_windows <- function ()
{
  identical(.Platform$OS.type, 'windows')
}


R_binary <- function ()
{
  binary <- ifelse(is_windows(), 'R.exe', 'R')
  file.path(R.home("bin"), binary)
}


process_exists <- function (pid)
{
  pid <- as.integer(pid)
  
  if (is_windows()) {
    output <- system2("tasklist", c("/FI", paste0('"PID eq ', pid, '"')),
                      stdout = TRUE, stderr = NULL)
    return(length(grep(as.character(pid), output, fixed = TRUE)) > 0)
  }
  else {
    rc <- system2("pgrep", as.character(pid), stdout = NULL, stderr = NULL)
    return(rc == 0)
  }
}
