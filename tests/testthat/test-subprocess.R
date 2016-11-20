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
  expect_equal(process_poll(handle, TIMEOUT_INFINITE), "terminated")
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
  normalizePathMock <- mock('/full/path/to/local/executable')
  dotCallMock <- mock(1)
  
  stub(spawn_process, 'normalizePath', normalizePathMock)
  stub(spawn_process, '.Call', dotCallMock)
  handle <- spawn_process("~/local/executable")

  expect_no_calls(normalizePathMock, 1)
  expect_no_calls(dotCallMock, 1)
})
