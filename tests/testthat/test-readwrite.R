context("read-write")

test_that("output buffer is flushed", {
  lines   <- 1000
  command <- paste0('cat(sep = "\\n", replicate(', lines,
                    ', paste(sample(letters, 60, TRUE), collapse = "")))')

  on.exit(terminate_gracefully(handle))
  handle <- R_child()
  expect_true(process_exists(handle))

  # send the command and give the process a moment to produce the output
  process_write(handle, paste(command, "\n"))
  Sys.sleep(3)
  
  # read everything
  output <- process_read(handle, PIPE_STDOUT, TIMEOUT_INFINITE, flush = TRUE)

  expect_length(output, lines)
  expect_true(all(nchar(output) == 60))
})


test_that("exchange data", {
  on.exit(terminate_gracefully(handle))
  handle <- R_child()
  
  expect_true(process_exists(handle))
  
  process_write(handle, 'cat("A")\n')
  output <- process_read(handle, timeout = 1000)

  expect_named(output, c('stdout', 'stderr'))
  expect_equal(output$stdout, 'A')
  expect_equal(output$stderr, character())
})


test_that("read from standard error output", {
  on.exit(terminate_gracefully(handle))
  handle <- R_child()
  
  process_write(handle, 'cat("A", file = stderr())\n')
  output <- process_read(handle, PIPE_STDERR, timeout = 1000)

  expect_true(is.character(output))
  expect_equal(output, 'A')
})


test_that("write returns the number of characters", {
  on.exit(terminate_gracefully(handle))
  handle <- R_child()
  
  expect_equal(process_write(handle, 'cat("A")\n'), 9)
})


test_that("non-blocking read", {
  on.exit(terminate_gracefully(handle))
  
  handle <- R_child()
  expect_true(process_exists(handle))

  expect_equal(process_read(handle, PIPE_STDOUT), character(0))
  expect_equal(process_read(handle, PIPE_STDERR), character(0))
  expect_equal(process_read(handle, PIPE_BOTH), list(stdout = character(0),
                                                     stderr = character(0)))
})
