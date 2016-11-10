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


test_that("environment can be specified", {
  on.exit(process_terminate(handle), add = TRUE)
  handle <- R_child(environment = "VAR=SOME_VALUE")
  
  process_write(handle, 'cat(Sys.getenv("VAR"))\n')
  expect_equal(process_read(handle, timeout = 1000), 'SOME_VALUE')
})
