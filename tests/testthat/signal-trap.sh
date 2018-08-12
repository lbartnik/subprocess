#!/bin/bash

function handler
{
  echo "$1"
}

function trap_with_name
{
  shift
  for sig in $*; do
    trap "handler $sig" "$sig"
  done
}

trap_with_name SIGHUP SIGINT SIGQUIT SIGILL SIGABRT SIGFPE\
               SIGKILL SIGSEGV SIGPIPE SIGALRM SIGTERM SIGUSR1\
               SIGUSR2 SIGCHLD SIGCONT SIGSTOP SIGTSTP SIGTTIN\
               SIGTTOU

echo "ready"

# wait until ready to exit
read
