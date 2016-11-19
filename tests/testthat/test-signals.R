context("signals")

test_that("sending signal to a child process", {
  skip_if_not(is_linux())

  script_path <- file.path(getwd(), 'signal-trap.sh')
  expect_true(file.exists(script_path))
  
  bash_path <- "/bin/bash"
  expect_true(file.exists(bash_path))
  
  handle <- spawn_process(bash_path, c("-e", script_path))
  expect_true(process_exists(handle))
  
  # excluded signals kill or stop the child
  for (signal in setdiff(signals, c(1, 9, 17, 19))) {
    process_send_signal(handle, signal)
    output <- process_read(handle, 'stdout', TIMEOUT_INFINITE)
    
    i <- which(signals == signal)
    expect_equal(output, names(signals)[[i]])
  }
})

test_that("sending signal to a child process", {
  skip_if_not(is_windows())
  
  # TODO fill in this test
  expect_true(FALSE)
})
