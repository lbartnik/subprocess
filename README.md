subprocess
==========================

| CRAN version    | Travis build status   | AppVeyor | Coverage | Downloads |
| :-------------: |:---------------------:|:--------:|:--------:|:---------:|
| [![CRAN version](http://www.r-pkg.org/badges/version/subprocess)](https://cran.r-project.org/package=subprocess) | [![Build Status](https://travis-ci.org/lbartnik/subprocess.svg?branch=master)](https://travis-ci.org/lbartnik/subprocess) | [![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/github/lbartnik/subprocess?branch=master&svg=true)](https://ci.appveyor.com/project/lbartnik/subprocess) | [![codecov](https://codecov.io/gh/lbartnik/subprocess/branch/master/graph/badge.svg)](https://codecov.io/gh/lbartnik/subprocess)| [![cranlogs](https://cranlogs.r-pkg.org/badges/grand-total/subprocess)](https://cranlogs.r-pkg.org/) |



Run and interact with a child process in R! `subprocess` brings a new
R API to create, control the life cycle and shutdown a child process
in **Linux**, **Windows** and **Mac OS**. Check this out.


## Remote shell example

Here's an example of running a `ssh` client child process. It connects
to a ssh server using public key (thus, no password). Then we list files
in the remote account and finally gracefully shutdown the child process.

Load the `subprocess` package and start a new child process:

```r
library(subprocess)

ssh_path <- '/usr/bin/ssh'
handle <- spawn_process(ssh_path, c('-T', 'test@example'))
```

Here is the description of the child process:

```r
print(handle)
#> Process Handle
#> command   : /usr/bin/ssh -T test@example
#> system id : 17659
#> state     : running
```

And here is what the child process has sent so far to its output streams:

```r
process_read(handle, PIPE_STDOUT, timeout = TIMEOUT_INFINITE)
#> [1] "Welcome to Ubuntu 16.10 (GNU/Linux 4.8.0-27-generic x86_64)"
#> [2] ""                                                           
#> [3] " * Documentation:  https://help.ubuntu.com"                 
#> [4] " * Management:     https://landscape.canonical.com"         
#> [5] " * Support:        https://ubuntu.com/advantage"            
#> [6] ""                                                           
#> [7] "0 packages can be updated."                                 
#> [8] "0 updates are security updates."                            
#> [9] ""
process_read(handle, PIPE_STDERR)
#> character(0)
```

Nothing in the standard error output. Good! Now we ask the remote shell
to list files.

```r
process_write(handle, 'ls\n')
#> [1] 3
process_read(handle, PIPE_STDOUT, timeout = TIMEOUT_INFINITE)
#> [1] "Desktop"          "Download"         "examples.desktop"
#> [4] "Music"            "Public"           "Video"
process_read(handle, PIPE_STDERR)
#> character(0)
```

The first number in the output is the value returned by `process_write()`
which is the number of characters written to standard input of the
child process. The final `character(0)` is the output read from the
standard error stream.


We are now ready to close the connection by exiting the remote shell:

```r
process_write(handle, 'exit\n')
#> [1] 5
process_read(handle, PIPE_STDOUT, timeout = TIMEOUT_INFINITE)
#> character(0)
process_read(handle, PIPE_STDERR)
#> character(0)
```

The last thing is making sure that the child process is no longer alive:

```r
process_wait(handle, TIMEOUT_INFINITE)
#> [1] 0
process_status(handle)
#> [1] "exited"
```
