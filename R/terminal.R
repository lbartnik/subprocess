#' Emulate terminal
#'
#' Emulate terminal
#' @param handle Process handle obtained from \code{spawn_process}.
#'
#' @export
#'
#' @examples
#' \dontrun{
#' is_windows <- function () (tolower(.Platform$OS.type) == "windows")
#' 
#' R_binary <- function () {
#'   R_exe <- ifelse (is_windows(), "R.exe", "R")
#'   return(file.path(R.home("bin"), R_exe))
#' }
#' 
#' handle <- spawn_process(R_binary(), c('--no-save'))
#' Sys.sleep(5)
#' if(interactive()) process_terminal(handle)
#' process_kill(handle)
#' }

process_terminal <- function(handle){
  stopifnot(is_process_handle(handle))
  message("This is subprocess version ", 
          packageVersion("subprocess"), 
          ". Type \"exit\" to exit.")
  on.exit(message("Exiting subprocess terminal emulator."))
  cat(paste(process_read(handle), collpase = "\n"))
  buffer <- character();
  
  # OSX R.app does not support savehistory
  has_history <- !inherits(
    try(savehistory(tempfile()), silent=T), 
    "try-error"
  )
  if(has_history){
    savehistory()
    on.exit(loadhistory(), add = TRUE)
    histfile <- ".subprocesshistory"
    if(file.exists(histfile)){
      loadhistory(histfile)
    } else {
      file.create(histfile)
    }
  }
  
  # REPL
  repeat {
    prompt <- ifelse(length(buffer), "  ", "$ ")
    if(nchar(line <- readline(prompt))){
      buffer <- c(buffer, line)
    }
    if(identical(buffer, "exit")){
      break;
    }else{
      if(nchar(line > 0)){
        chk <- process_write(handle, paste0(line, "\n"))
      }
    }
    if(length(buffer)){
      if(has_history){
        write(buffer, histfile, append = TRUE)
        loadhistory(histfile)
      }
      tryCatch({
        cat(paste(process_read(handle), collapse = "\n"))
        cat(paste(process_read(handle, "stderr"), collapse = "\n"))
      },
        error = function(e){
          message(e$message)
        }
      )
      buffer <- character();
    }
  }
}
