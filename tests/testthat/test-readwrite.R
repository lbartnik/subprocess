context("read-write")

test_that("output buffer is flushed", {
  on.exit(process_kill(handle))

  lines   <- 1000
  command <- paste0('cat(sep = "\\n", replicate(', lines,
                    ', paste(sample(letters, 60, TRUE), collapse = "")))')
  handle <- spawn_process(R_binary(), c('--slave', '-e', command))
  
  # give him a chance to start
  Sys.sleep(0.3)
  expect_true(process_exists(handle))

  # read everything
  output <- process_read(handle, 'stdout', TIMEOUT_INFINITE, flush = TRUE)

  expect_length(output, lines)
  expect_true(all(nchar(output) == 60))
})
