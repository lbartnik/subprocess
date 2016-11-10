context("child process parameters")


test_that("working directory can be set", {
  expect_false(identical(root_dir(), getwd()))
  
  on.exit(process_terminate(handle), add = TRUE)
  handle <- R_child()
  
  process_write(handle, "cat(getwd())\n")
  expect_equal(process_read(handle, timeout = 1000), getwd())
  
  on.exit(process_terminate(handle_2), add = TRUE)
  handle_2 <- R_child(workdir = root_dir())
  
  process_write(handle_2, "cat(getwd())\n")
  expect_equal(process_read(handle_2, timeout = 1000), root_dir())
})


test_that("environment", {
  on.exit(process_terminate(handle), add = TRUE)
  handle <- R_child(environment = "VAR=SOME_VALUE")
  
  process_write(handle, 'cat(Sys.getenv("VAR"))\n')
  expect_equal(process_read(handle, timeout = 1000), 'SOME_VALUE')
})


test_that("environment via named vector", {
  on.exit(process_terminate(handle), add = TRUE)
  handle <- R_child(environment = c(VAR="SOME_VALUE"))
  
  process_write(handle, 'cat(Sys.getenv("VAR"))\n')
  expect_equal(process_read(handle, timeout = 1000), 'SOME_VALUE')
})


test_that("environment via list", {
  on.exit(process_terminate(handle), add = TRUE)
  handle <- R_child(environment = list(VAR="SOME_VALUE"))
  
  process_write(handle, 'cat(Sys.getenv("VAR"))\n')
  expect_equal(process_read(handle, timeout = 1000), 'SOME_VALUE')
})


test_that("environment error checking", {
  expect_error(spawn_process(R_binary(), environment = list(A="B", "C")))
})
