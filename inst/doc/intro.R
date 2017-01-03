## ----setup, include=FALSE------------------------------------------------
library(subprocess)
library(knitr)

knitr::opts_chunk$set(collapse = TRUE, comment = "#>")

## ----helpers-------------------------------------------------------------
is_windows <- function () (tolower(.Platform$OS.type) == "windows")

R_binary <- function () {
  R_exe <- ifelse (is_windows(), "R.exe", "R")
  return(file.path(R.home("bin"), R_exe))
}

## ----platform------------------------------------------------------------
ifelse(is_windows(), "Windows", "Linux")

## ------------------------------------------------------------------------
library(subprocess)

## ----new_child-----------------------------------------------------------
handle <- spawn_process(R_binary(), c('--no-save'))
Sys.sleep(1)

## ------------------------------------------------------------------------
print(handle)

## ----read_from_child-----------------------------------------------------
process_read(handle, PIPE_STDOUT, timeout = 1000)
process_read(handle, PIPE_STDERR)

## ----new_n---------------------------------------------------------------
process_write(handle, 'n <- 10\n')
process_read(handle, PIPE_STDOUT, timeout = 1000)
process_read(handle, PIPE_STDERR)

## ----rnorn_n-------------------------------------------------------------
process_write(handle, 'rnorm(n)\n')
process_read(handle, PIPE_STDOUT, timeout = 1000)
process_read(handle, PIPE_STDERR)

## ----quit_child----------------------------------------------------------
process_write(handle, 'q(save = "no")\n')
process_read(handle, PIPE_STDOUT, timeout = 1000)
process_read(handle, PIPE_STDERR)

## ----verify_child_exited-------------------------------------------------
process_state(handle)
process_return_code(handle)

## ----spawn_shell---------------------------------------------------------
shell_binary <- function () {
  ifelse (tolower(.Platform$OS.type) == "windows",
          "C:/Windows/System32/cmd.exe", "/bin/sh")
}

handle <- spawn_process(shell_binary())
print(handle)

## ----interact_with_shell-------------------------------------------------
process_write(handle, "ls\n")
Sys.sleep(1)
process_read(handle, PIPE_STDOUT)
process_read(handle, PIPE_STDERR)

## ----signal_child--------------------------------------------------------
sub_command <- "library(subprocess);subprocess:::signal(15,'ignore');Sys.sleep(1000)"
handle <- spawn_process(R_binary(), c('--slave', '-e', sub_command))
Sys.sleep(1)

# process is hung
process_wait(handle, 1000)
process_state(handle)

# ask nicely to exit; will be ignored in Linux but not in Windows
process_terminate(handle)
process_wait(handle, 1000)
process_state(handle)

# forced exit; in Windows the same as the previous call to process_terminate()
process_kill(handle)
process_wait(handle, 1000)
process_state(handle)

## ----show_three_signals--------------------------------------------------
length(signals)
signals[1:3]

## ------------------------------------------------------------------------
ls(pattern = 'SIG', envir = asNamespace('subprocess'))

## ----eval=FALSE----------------------------------------------------------
#  handle <- spawn_process(R_binary, '--slave')
#  
#  process_send_signal(handle, SIGUSR1)

