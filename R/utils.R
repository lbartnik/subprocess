is_windows <- function ()
{
  identical(tolower(Sys.info()[["sysname"]]), 'windows')
}

is_linux <- function ()
{
  identical(tolower(Sys.info()[["sysname"]]), 'linux')
}

known_signals <- function ()
{
  .Call("C_known_signals")
}
