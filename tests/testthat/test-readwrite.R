context("read-write")

test_that("output buffer is flushed", {
  on.exit(process_kill(handle))

  lines   <- 1000
  command <- paste0('cat(sep = "\\n", replicate(', lines,
                    ', paste(sample(letters, 60, TRUE), collapse = "")))')
  handle <- R_child()
  expect_true(process_exists(handle))

  # send the command and give the process a moment to produce the output
  process_write(handle, paste(command, "\n"))
  Sys.sleep(.3)
  
  # read everything
  output <- process_read(handle, 'stdout', TIMEOUT_INFINITE, flush = TRUE)

  expect_length(output, lines)
  expect_true(all(nchar(output) == 60))
})
