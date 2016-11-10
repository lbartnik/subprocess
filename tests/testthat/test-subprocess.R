context("subprocess")

test_that("helper works", {
  expect_true(process_exists(Sys.getpid()))
  expect_false(process_exists(99999999))
})



test_that("a subprocess can be spawned and killed", {
  binary <- R_binary()
  
  handle <- spawn_process(binary, '--no-save')
  expect_true('handle_ptr' %in% names(attributes(handle)))

  ptr <- attr(handle, 'handle_ptr')
  expect_equal(class(ptr), 'externalptr')
  
  expect_true(process_exists(handle))
  
  process_terminate(handle)
  process_wait(handle, 1000)
  expect_false(process_exists(handle))
})


test_that("exchange data", {
  binary <- R_binary()
  
  on.exit(process_terminate(handle))
  handle <- spawn_process(binary, c('--slave'))
  
  expect_true(process_exists(handle))
  
  process_write(handle, 'cat("A")\n')
  expect_equal(process_read(handle, timeout = 1000), 'A')
})

