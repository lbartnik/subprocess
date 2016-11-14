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
  
  shell_script_parent <- tempfile(fileext = '.bat')
  shell_script_child  <- tempfile(fileext = '.bat')
  
  write(file = shell_script_parent, paste('start "subprocess test child" /b',
                                          shell_script_child))
  write(file = shell_script_child, "waitfor SomethingThatIsNeverHappening /t 100 2>NUL")

  # start the parent process which in turn spawns a child process
  parent_handle <- spawn_process("c:\\Windows\\System32\\cmd.exe",
                                 c("/k", shell_script_parent))
  expect_true(process_exists(as.integer(parent_handle)))

  # find the child process' id and make sure it exists now...
  child_id <- windows_process_id(basename(shell_script_child))
  expect_length(child_id, 1)
  expect_true(process_exists(child_id))
  
  # ... and not after we kill the parent
  process_kill(parent_handle)
  expect_false(process_exists(as.integer(parent_handle)))
  expect_false(process_exists(child_id))
})

