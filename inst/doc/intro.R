## ----setup, include=FALSE------------------------------------------------
library(subprocess)
library(knitr)

# if Windows, don't evaluate any code
eval_vignette <- (tolower(.Platform$OS.type) != "windows")
eval_ssh      <- FALSE

# if not Windows, make sure ssh can connect with the account
if (eval_vignette) {
  rc <- system2("ssh", c("test@localhost", "-o", "PasswordAuthentication=no"),
                stdout = FALSE, stderr = FALSE, input = "")
  eval_ssh <- (rc == 0)
}

knitr::opts_chunk$set(collapse = TRUE, comment = "#>", eval = eval_vignette)

## ----include=!eval_vignette, eval=!eval_vignette, echo=FALSE, results='asis'----
#  cat("##Important!\n")
#  cat("This vignette has been evaluated in Windows or in an environment\n")
#  cat("where the _ssh_ connection to _test&#64;localhost_ cannot be established.\n")
#  cat("Thus, the code in the examples that require certain executables or\n")
#  cat("signals is not evaluated but simply printed.")

## ----eval=eval_ssh-------------------------------------------------------
library(subprocess)

ssh_path <- '/usr/bin/ssh'
handle <- spawn_process(ssh_path, c('-T', 'test@localhost'))

## ----eval=eval_ssh-------------------------------------------------------
print(handle)

## ----eval=eval_ssh-------------------------------------------------------
process_read(handle, timeout = 1000)
process_read(handle, 'stderr')

## ----eval=eval_ssh-------------------------------------------------------
process_write(handle, 'ls\n')
process_read(handle, timeout = 1000)
process_read(handle, 'stderr')

## ----eval=eval_ssh-------------------------------------------------------
process_write(handle, 'exit\n')
process_read(handle, timeout = 1000)
process_read(handle, 'stderr')

## ----eval=eval_ssh-------------------------------------------------------
process_poll(handle)
process_return_code(handle)

## ------------------------------------------------------------------------
R_binary <- file.path(R.home("bin"), "R")
sub_command <- 'library(subprocess); .Call("C_signal", 15L, "ignore"); Sys.sleep(1000)'
handle <- spawn_process(R_binary, c('--slave', '-e', sub_command))

# process is hung
process_poll(handle, 1000)
process_return_code(handle)

# ask nicely to exit
process_terminate(handle)
process_poll(handle, 1000)
process_return_code(handle)

# forced exit
process_kill(handle)
process_poll(handle, 1000)
process_return_code(handle)

## ------------------------------------------------------------------------
length(signals)
signals[1:3]

## ------------------------------------------------------------------------
ls(pattern = 'SIG', envir = asNamespace('subprocess'))

## ------------------------------------------------------------------------
handle <- spawn_process(R_binary, '--slave')

process_send_signal(handle, SIGUSR1)

