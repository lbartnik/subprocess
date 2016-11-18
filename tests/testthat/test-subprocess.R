context("subprocess")

test_that("helper works", {
  expect_true(process_exists(Sys.getpid()))
  expect_false(process_exists(99999999))
})


test_that("a subprocess can be spawned and killed", {
  handle <- R_child()
  expect_named(handle, c('c_handle', 'command', 'arguments'))
  expect_true('handle_ptr' %in% names(attributes(handle$c_handle)))

  ptr <- attr(handle$c_handle, 'handle_ptr')
  expect_equal(class(ptr), 'externalptr')
  
  expect_true(process_exists(handle))
  
  process_kill(handle)
  process_wait(handle, 1000)
  expect_false(process_exists(handle))
})


test_that("exchange data", {
  on.exit(process_kill(handle))
  handle <- R_child()
  
  expect_true(process_exists(handle))
  
  process_write(handle, 'cat("A")\n')
  expect_equal(process_read(handle, timeout = 1000), 'A')
})


test_that("read from standard error output", {
  on.exit(process_kill(handle))
  handle <- R_child()

  process_write(handle, 'cat("A", file = stderr())\n')
  expect_equal(process_read(handle, 'stderr', timeout = 1000), 'A')
  expect_equal(process_read(handle), character())
})


test_that("write returns the number of characters", {
  on.exit(process_kill(handle))
  handle <- R_child()
  
  expect_equal(process_write(handle, 'cat("A")\n'), 9)
})


test_that("error when no executable", {
  expect_error(spawn_process("xxx"))
})

test_that("can expand paths", {
  if(is_windows()){
    proc1 <- tempfile(fileext = ".bat", tmpdir = "~")
    cat("ping -n 120 127.0.0.1 >nul", file = proc1)
  }else{
    proc1 <- tempfile(fileext = ".sh", tmpdir = "~")
    cat("#!/bin/sh\n", file = proc1)
    cat("sleep 120\n", file = proc1, append = TRUE)
  }
  on.exit(unlink(proc1))
  Sys.chmod(proc1, "700")
  expect_silent(handle1 <- spawn_process(proc1))
  expect_silent(process_kill(handle1))
})
