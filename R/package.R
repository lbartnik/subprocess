
.onLoad <- function (libname, pkgname)
{
  signals <<- as.list(known_signals())
  attach(signals)
}

.onUnload <- function (libpath)
{
  detach(signals)
}
