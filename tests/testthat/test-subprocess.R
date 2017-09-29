context("subprocess")

killed_exit_code <- ifelse(is_windows(), 127, 9)

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
  
  # we need to clean-up 'manually'
  process_write(handle, "cat(tempdir())\n")
  path <- process_read(handle, PIPE_STDOUT, TIMEOUT_INFINITE)
  expect_true(dir.exists(path))
  on.exit(unlink(path, TRUE, TRUE), add = TRUE)

  process_kill(handle)
  expect_equal(process_wait(handle, TIMEOUT_INFINITE), killed_exit_code)
  expect_equal(process_state(handle), "terminated")
  expect_true(wait_until_exits(handle))
})


test_that("waiting for a child to exit", {
  on.exit(process_terminate(handle))
  handle <- R_child()

  process_wait(handle, TIMEOUT_IMMEDIATE)
  expect_equal(process_state(handle), "running")
  
  # we need to clean-up 'manually'
  process_write(handle, "cat(tempdir())\n")
  path <- process_read(handle, PIPE_STDOUT, TIMEOUT_INFINITE)
  expect_true(dir.exists(path))
  on.exit(unlink(path, TRUE, TRUE), add = TRUE)
  
  process_kill(handle)

  expect_equal(process_wait(handle, TIMEOUT_INFINITE), killed_exit_code)
  expect_equal(process_state(handle), "terminated")
  expect_equal(process_return_code(handle), killed_exit_code)
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

  expect_called(normalizePathMock, 1)
  expect_called(dotCallMock, 1)
})


test_that("handle can be printed", {
  on.exit(terminate_gracefully(handle))
  handle <- R_child()
  
  path <- gsub("\\\\", "\\\\\\\\", normalizePath(R_binary()))
  expect_output(print(handle),
                paste0("Process Handle\n",
                       "command   : ", path, " --slave\n",
                       "system id : [0-9]*\n",
                       "state     : running"))
})
