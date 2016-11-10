context("subprocess")

test_that("helper works", {
  expect_true(process_exists(Sys.getpid()))
  expect_false(process_exists(99999999))
})


test_that("a subprocess can be spawned and killed", {
  handle <- R_child()
  expect_true('handle_ptr' %in% names(attributes(handle)))

  ptr <- attr(handle, 'handle_ptr')
  expect_equal(class(ptr), 'externalptr')
  
  expect_true(process_exists(handle))
  
  process_terminate(handle)
  process_wait(handle, 1000)
  expect_false(process_exists(handle))
})


test_that("exchange data", {
  on.exit(process_terminate(handle))
  handle <- R_child()
  
  expect_true(process_exists(handle))
  
  process_write(handle, 'cat("A")\n')
  expect_equal(process_read(handle, timeout = 1000), 'A')
})


test_that("read from standard error output", {
  on.exit(process_terminate(handle))
  handle <- R_child()

  process_write(handle, 'cat("A", file = stderr())\n')
  expect_equal(process_read(handle, 'stderr', timeout = 1000), 'A')
  expect_equal(process_read(handle), character())
})


test_that("write returns the number of characters", {
  on.exit(process_terminate(handle))
  handle <- R_child()
  
  expect_equal(process_write(handle, 'cat("A")\n'), 9)
})


test_that("error when no executable", {
  expect_error(spawn_process("xxx"))
})
