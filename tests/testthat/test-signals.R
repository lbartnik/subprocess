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
    spawn_process(Sys.which("cmd"), c("/C", '"sleep 60"'))
  }


  # according to:
  # https://msdn.microsoft.com/en-us/library/cc704588.aspx
  #
  # 0xC0000001 = STATUS_UNSUCCESSFUL
  # 0xC000013A = STATUS_CONTROL_C_EXIT
  #
  # However, exit code doesn't seem to be consistent between deployments
  # (AppVeyor vs. CRAN's win-builder vs. a local Windows system) and
  # return codes vary: 0, 1, -1073741510L. For that reason we do not
  # check the exit code in the test below.

  # Ctrl+C
  handle <- spawn()
  expect_true(wait_until_appears(handle))

  process_send_signal(handle, CTRL_C_EVENT)
  expect_silent(process_wait(handle, TIMEOUT_INFINITE))
  expect_false(process_exists(handle))

  # CTRL+Break
  handle <- spawn()
  expect_true(wait_until_appears(handle))

  process_send_signal(handle, CTRL_BREAK_EVENT)
  expect_silent(process_wait(handle, TIMEOUT_INFINITE))
  expect_false(process_exists(handle))
})

