context("subprocess")

test_that("helper works", {
  expect_true(process_exists(Sys.getpid()))
  expect_false(process_exists(99999999))
})



test_that("a subprocess can be spawned", {
  binary <- R_binary()
  
  handle <- spawn_process(binary)
  expect_true('handle_ptr' %in% names(attributes(handle)))

  ptr <- attr(handle, 'handle_ptr')
  expect_equal(class(ptr), 'externalptr')
  
  expect_true(process_exists(handle))
})


test_that("a subprocess can be stopped", {
  
})