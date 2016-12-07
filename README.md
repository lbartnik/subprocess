subprocess
==========================

| CRAN version    | Travis build status   | AppVeyor | Coverage |
| :-------------: |:---------------------:|:--------:|:--------:|
| [![CRAN version](http://www.r-pkg.org/badges/version/subprocess)](https://cran.r-project.org/package=subprocess) | [![Build Status](https://travis-ci.org/lbartnik/subprocess.svg?branch=master)](https://travis-ci.org/lbartnik/subprocess) | [![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/github/lbartnik/subprocess?branch=master&svg=true)](https://ci.appveyor.com/project/lbartnik/subprocess) | [![codecov](https://codecov.io/gh/lbartnik/subprocess/branch/master/graph/badge.svg)](https://codecov.io/gh/lbartnik/subprocess)|


Now you can run and interact with a child process in R! `subprocess`
brings to R new API to create, control and shutdown a child process
in Linux, Windows and Mac OS. Check this out.


In this simple example we start a `ssh` client and connect to the local
ssh server. The connection is password-less, based on the public key.
Then, we list files in the remote  account. The example can be run in
both **Linux** and **Windows**, as long as a `ssh` __daemon__ is running
on `localhost` and a correct path to the `ssh` client is passed to
`spawn_process()`.


Open the connection:

```r
library(subprocess)

ssh_path <- '/usr/bin/ssh'
handle <- spawn_process(ssh_path, c('-T', 'test@localhost'))
```

Let's see the description:
```r
print(handle)
```
```
#> Process Handle
#> command   : /usr/bin/ssh -T test@localhost
#> system id : 17659
#> state     : running
```

And now let's see what we can find it the child's output:
```r
process_read(handle, timeout = 1000)
#> character(0)
process_read(handle, 'stderr')
#> character(0)
```


The first number in the output is the value returned by `process_write`
which is the number of characters written to standard input of the
child process. The final `character(0)` is the output read from the
standard error stream.

Next, list files in the remote account:

```{r}
process_write(handle, 'ls\n')
#> [1] 3
process_read(handle, timeout = 1000)
#> [1] "Welcome to Ubuntu 16.10 (GNU/Linux 4.8.0-27-generic x86_64)"
#> [2] ""                                                           
#> [3] " * Documentation:  https://help.ubuntu.com"                 
#> [4] " * Management:     https://landscape.canonical.com"         
#> [5] " * Support:        https://ubuntu.com/advantage"            
#> [6] ""                                                           
#> [7] "0 packages can be updated."                                 
#> [8] "0 updates are security updates."                            
#> [9] ""
process_read(handle, 'stderr')
#> character(0)
```

We are ready to close the connection by exiting the remote __shell__:

```{r}
process_write(handle, 'exit\n')
#> [1] 5
process_read(handle, timeout = 1000)
#> [1] "Desktop"          "Download"         "examples.desktop"
#> [4] "Music"            "Public"           "Video"
process_read(handle, 'stderr')
#> character(0)
```

The last thing is making sure that the child process is no longer alive:

```{r}
process_poll(handle)
#> [1] "exited"
process_return_code(handle)
#> [1] 0
```




