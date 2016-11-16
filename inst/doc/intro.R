## ----setup, include=FALSE------------------------------------------------
library(subprocess)
library(knitr)

eval <- .Platform$OS.type != "windows"
knitr::opts_chunk$set(collapse = TRUE, comment = "#>", eval = eval)

## ------------------------------------------------------------------------
library(subprocess)

ssh_path <- '/usr/bin/ssh'
handle <- spawn_process(ssh_path, c('-T', 'test@localhost'))

## ------------------------------------------------------------------------
print(handle)

## ------------------------------------------------------------------------
process_read(handle, timeout = 1000)
process_read(handle, 'stderr')

## ------------------------------------------------------------------------
process_write(handle, 'ls\n')
process_read(handle, timeout = 1000)
process_read(handle, 'stderr')

## ------------------------------------------------------------------------
process_write(handle, 'exit\n')
process_read(handle, timeout = 1000)
process_read(handle, 'stderr')

## ------------------------------------------------------------------------
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

