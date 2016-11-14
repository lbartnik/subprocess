context("readwrite")

test_that("canReadStdOutThatFillsBuffer", {
  proc1 <- tempfile(fileext = ".sh")
  on.exit(unlink(proc1))
  cat("#!/bin/sh\n", file = proc1)
  randomStrings <- 
    replicate(100, {function()paste(sample(letters, 60, TRUE), 
                                    collapse = "")}())
  write(paste("echo", randomStrings), file = proc1, append = TRUE)
  Sys.chmod(proc1, "700")
  
  handle1 <- spawn_process(proc1)
  on.exit(process_kill(handle1), add = TRUE)
  firstread <- process_read(handle1, timeout = 1000L)
  expect_identical(length(firstread), 100L)
  expect_identical(firstread, randomStrings)
  secondread <- process_read(handle1, timeout = 1000L)
  expect_identical(secondread, character(0))
})

test_that("canReadStdErrThatFillsBuffer", {
  proc1 <- tempfile(fileext = ".sh")
  on.exit(unlink(proc1))
  cat("#!/bin/sh\n", file = proc1)
  randomStrings <- 
    replicate(100, {function()paste(sample(letters, 60, TRUE), 
                                    collapse = "")}())
  write(paste("echo", randomStrings, ">&2"), file = proc1, append = TRUE)
  Sys.chmod(proc1, "700")
  
  handle1 <- spawn_process(proc1)
  on.exit(process_kill(handle1), add = TRUE)
  firstread <- process_read(handle1, pipe = "stderr", timeout = 1000L)
  expect_identical(length(firstread), 100L)
  expect_identical(firstread, randomStrings)
  secondread <- process_read(handle1, pipe = "stderr", timeout = 1000L)
  expect_identical(secondread, character(0))
})
