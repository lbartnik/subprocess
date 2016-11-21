## ----setup, include=FALSE------------------------------------------------
library(subprocess)
library(knitr)

# if Windows, don't evaluate any code
eval_ssh <- tolower(.Platform$OS.type != "windows")

# if not Windows, make sure ssh can connect with the account
if (eval_ssh) {
  rc <- system2("ssh", c("test@localhost", "-o", "PasswordAuthentication=no"),
                stdout = FALSE, stderr = FALSE, input = "")
  eval_ssh <- (rc == 0)
}

knitr::opts_chunk$set(collapse = TRUE, comment = "#>")

## ----include=!eval_ssh, eval=!eval_ssh, echo=FALSE, results='asis'-------
#  cat("##Important!\n")
#  cat("This vignette has been evaluated in an environment where the _ssh_\n")
#  cat("connection to _test&#64;localhost_ cannot be established and thus the\n")
#  cat("code in the _ssh_ example is not evaluated but simply printed.")

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
R_binary <- file.path(R.home("bin"), "R")
handle <- spawn_process(R_binary, '--slave')

process_send_signal(handle, SIGUSR1)

