context("signals")


test_that("sending signal in Linux/MacOS/Solaris", {
  skip_if_not(is_linux() || is_mac() || is_solaris())

  script_path <- file.path(getwd(), 'signal-trap.sh')
  expect_true(file.exists(script_path))

  bash_path <- "/bin/bash"
  expect_true(file.exists(bash_path))
  
  handle <- spawn_process(bash_path, c("-e", script_path))
  expect_true(process_exists(handle))
  
  # excluded signals kill or stop the child
  skip_sigs <- c(SIGHUP, SIGKILL, SIGCHLD, SIGSTOP)
  if (is_solaris()) {
    # on Solaris SIGQUIT behaviour is diffrent 
    skip_sigs <- c(skip_sigs, SIGQUIT) 
  }
  for (signal in setdiff(signals, skip_sigs)) {
    process_send_signal(handle, signal)
    output <- process_read(handle, PIPE_STDOUT, TIMEOUT_INFINITE)
    i <- which(signals == signal)
    expect_equal(output, names(signals)[[i]], info = names(signals)[[i]])
  }
})


test_that("sending signal in Windows", {
  skip_if_not(is_windows())

  spawn <- function () {
    spawn_process(R_binary(), c("--slave", "-e", "tryCatch(Sys.sleep(60))"))
  }

  # Ctrl+C
  handle <- spawn()
  expect_true(wait_until_appears(handle))
  
  process_send_signal(handle, CTRL_C_EVENT)
  
  # according to:
  # https://msdn.microsoft.com/en-us/library/cc704588.aspx
  # 
  # 0xC0000001 = STATUS_UNSUCCESSFUL
  expect_equal(process_wait(handle, TIMEOUT_INFINITE), 1)
  expect_false(process_exists(handle))

  # CTRL+Break
  handle <- spawn()
  expect_true(wait_until_appears(handle))

    process_send_signal(handle, CTRL_BREAK_EVENT)
  
  # 0xC000013A = STATUS_CONTROL_C_EXIT  
  expect_equal(process_wait(handle, TIMEOUT_INFINITE), -1073741510L)
  expect_false(process_exists(handle))
})
