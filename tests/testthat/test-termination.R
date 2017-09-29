context("termination")


# Hopefully there is no other process 
windows_process_id <- function (command_line)
{
  args <- c("process", "where",
            paste0("\"Name='cmd.exe' and CommandLine LIKE '%%",
                   command_line, "%%' and Name='cmd.exe'\" get ProcessId"))
  output <- system2("wmic.exe", args, stdout = TRUE)
  as.integer(trimws(grep("\\d+", output, value = TRUE)))
}



test_that("child process is terminated in Windows", {
  skip_if_not(is_windows())
  
  shell_script_child  <- shell_script  <- tempfile(fileext = '.bat')
  write(file = shell_script_child, "waitfor SomethingThatIsNeverHappening /t 100 2>NUL")

  shell_script_parent <- tempfile(fileext = '.bat')
  write(file = shell_script_parent, paste('start "subprocess test child" /b',
                                          shell_script_child))

  # start the parent process which in turn spawns a child process
  parent_handle <- spawn_process("c:\\Windows\\System32\\cmd.exe",
                                 c("/k", shell_script_parent))
  expect_true(process_exists(parent_handle))

  # find the child process' id and make sure it exists now...
  child_id <- windows_process_id(basename(shell_script_child))
  expect_length(child_id, 1)
  expect_true(process_exists(child_id))
  
  # ... and not after we kill the parent
  process_kill(parent_handle)
  process_wait(parent_handle, TIMEOUT_INFINITE)
  expect_equal(process_state(parent_handle), "terminated")
  
  # give the child a moment to dissapear from OS tables
  expect_true(wait_until_exits(parent_handle))
  expect_true(wait_until_exits(child_id))
})



# In RStudio this test will pass even if termination_mode is set to
# "child_only" when run with Ctrl+Shift+T. It's quite possible that
# RStudio creates a new session when running tests and kills that
# session before completing the test run.
#
# This test will, however, fail in plain R if termination_mode is
# set to "child_only".
test_that("child process is terminated in Linux", {
  skip_if_not(is_linux())

  # the parent shell script will start "sleep" and print its PID
  shell <- Sys.getenv("SHELL", '/bin/sh')
  shell_script_parent <- tempfile()
  shell_script_child  <- tempfile()

  write(file = shell_script_parent,
        paste0('#!', shell, '\n',
               shell, ' ', shell_script_child, ' &', '\n',
               'echo $!', '\n',
               'sleep 50'))
  write(file = shell_script_child,
        paste0('#!', shell, '\n',
               'sleep 100'))
  
  # start the parent process which in turn spawns a child process
  parent_handle <- spawn_process(shell, shell_script_parent)
  expect_true(process_exists(parent_handle))
  
  # make sure the child exists; this sometimes fails in Linux (seen only
  # in runs performed by other people), I'm not really sure why but one
  # possibility is that the first shell script already knows its child's
  # PID but pgrep cannot see it just yet - that is, a RACE condition;
  # we address it by adding a timeout...
  start <- proc.time()
  child_id <- as.integer(process_read(parent_handle, 'stdout', 1000))

  # now make sure it actually has appeared
  expect_true(wait_until_appears(child_id))

  # ... and not after we kill the parent
  process_kill(parent_handle)

  expect_true(wait_until_exits(parent_handle))
  process_wait(parent_handle, TIMEOUT_INFINITE)
  expect_equal(process_state(parent_handle), "terminated")

  expect_true(wait_until_exits(child_id))
  expect_false(process_exists(parent_handle))
  expect_false(process_exists(child_id))
})


# --- closing the stdin stream -----------------------------------------


test_that("child exits when stdin is closed", {
  on.exit(process_terminate(handle))
  handle <- R_child()
  expect_true(process_exists(handle))

  process_close_input(handle)
  expect_equal(process_wait(handle, TIMEOUT_INFINITE), 0)
  expect_equal(process_state(handle), "exited")
})

