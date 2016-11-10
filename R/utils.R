is_windows <- function ()
{
  identical(.Platform$OS.type, 'windows')
}

known_signals <- function ()
{
  .Call("C_known_signals")
}
