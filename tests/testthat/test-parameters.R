context("child process parameters")


test_that("working directory can be set", {
  print_wd <- 'cat(normalizePath(getwd()))\n'

  work_dir <- normalizePath(tempdir())
  norm_wd  <- normalizePath(getwd())
  expect_false(identical(work_dir, getwd()))
  
  on.exit(process_kill(handle), add = TRUE)
  handle <- R_child()
  
  process_write(handle, print_wd)
  expect_equal(normalizePath(process_read(handle, timeout = 1000)$stdout), norm_wd)
  expect_true(terminate_gracefully(handle))
  
  on.exit(process_kill(handle_2), add = TRUE)
  handle_2 <- R_child(workdir = work_dir)
  
  process_write(handle_2, print_wd)
  expect_equal(normalizePath(process_read(handle_2, timeout = 1000)$stdout), work_dir)
  expect_true(terminate_gracefully(handle_2))
})


# --- new environment --------------------------------------------------

test_that("inherits environment from parent", {
  on.exit(Sys.unsetenv("PARENT_VAR"), add = TRUE)
  Sys.setenv(PARENT_VAR="PARENT_VAL")
  
  on.exit(process_terminate(handle), add = TRUE)
  handle <- R_child(c("--slave", "-e", "cat(Sys.getenv('PARENT_VAR'))"))
  
  expect_equal(process_read(handle, timeout = TIMEOUT_INFINITE)$stdout, 'PARENT_VAL')
  expect_equal(process_wait(handle, timeout = TIMEOUT_INFINITE), 0)
  expect_equal(process_state(handle), "exited")
})


test_that("passing new environment", {
  on.exit(terminate_gracefully(handle), add = TRUE)
  handle <- R_child(environment = "VAR=SOME_VALUE")
  
  process_write(handle, 'cat(Sys.getenv("VAR"))\n')
  expect_equal(process_read(handle, timeout = 1000)$stdout, 'SOME_VALUE')
})


test_that("new environment via named vector", {
  on.exit(terminate_gracefully(handle), add = TRUE)
  handle <- R_child(environment = c(VAR="SOME_VALUE"))
  
  process_write(handle, 'cat(Sys.getenv("VAR"))\n')
  expect_equal(process_read(handle, timeout = 1000)$stdout, 'SOME_VALUE')
})


test_that("new environment via list", {
  on.exit(terminate_gracefully(handle), add = TRUE)
  handle <- R_child(environment = list(VAR="SOME_VALUE"))
  
  process_write(handle, 'cat(Sys.getenv("VAR"))\n')
  expect_equal(process_read(handle, timeout = 1000)$stdout, 'SOME_VALUE')
})


test_that("environment error checking", {
  expect_error(spawn_process(R_binary(), environment = list(A="B", "C")))
})
