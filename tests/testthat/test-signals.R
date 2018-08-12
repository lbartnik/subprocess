context("signals")


test_that("sending signal in Linux/MacOS/Solaris", {
  skip_if_not(is_linux() || is_mac() || is_solaris())

  script_path <- file.path(getwd(), 'signal-trap.sh')
  expect_true(file.exists(script_path))

  bash_path <- "/bin/bash"
  expect_true(file.exists(bash_path))

  on.exit(terminate_gracefully(handle), add = TRUE)
  handle <- spawn_process(bash_path, c("-e", script_path))
  expect_true(process_exists(handle))

  # this is necessary to give bash time to set up the signal trap;
  # otherwise it is a race
  output <- process_read(handle, PIPE_STDOUT, TIMEOUT_INFINITE)
  expect_equal(output, "ready")

  # exclude signals to kill or stop the child
  skip <- c(SIGHUP, SIGKILL, SIGCHLD, SIGSTOP, if (is_solaris()) SIGQUIT)

  for (signal in setdiff(signals, skip)) {
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

  # https://stackoverflow.com/a/46610564/2493260
  # 
  # Globally speaking, Exit Code 0xC000013A means that the application
  # terminated as a result of a CTRL+C or closing command prompt window
  expect_equal(process_wait(handle, TIMEOUT_INFINITE), 0xC000013A)
  expect_false(process_exists(handle))
})

