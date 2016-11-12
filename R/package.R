#' Manage Subprocesses in R
#'
#' Cross-platform child process management modelled after Python's
#' \code{subprocess} module.
#'
#' @details This R package extends R's capabilities of starting and
#' handling child processes. It brings the capability of alternating
#' read from and write to a child process, communicating via signals,
#' terminating it and handling its exit status (return code).
#' 
#' With R's standard \code{\link{system}} and \code{\link{system2}}
#' functions one can start a new process and capture its output but
#' cannot directly write to its standard input. Another tool, the
#' \code{\link[parallel]{mclapply}} function, is aimed at replicating
#' the current session and is limited to operating systems that come
#' with the \code{fork()} system call.
#' 
#'
#' @docType package
#' @name subprocess
#' @rdname subprocess
#' 
#' @references
#' \url{https://github.com/lbartnik/subprocess}
#' 
#' \url{https://docs.python.org/3/library/subprocess.html}
#'
NULL


.onLoad <- function (libname, pkgname)
{
  signals <<- as.list(known_signals())
  envir <- asNamespace(pkgname)
  
  mapply(name = names(signals),
         code = as.integer(signals),
         function (name, code) {
           suppressWarnings(assign(name, code, envir = envir, inherits = FALSE))
         })
}
