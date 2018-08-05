context("utf8")

test_that("C tests pass", {
  expect_equal(C_tests_utf8(), "All C tests passed!")
})


test_that("multi-byte can come in parts", {
  skip_if_not(is_linux() || is_mac() || is_solaris())
  skip_if_not(l10n_info()$MBCS)

  print_in_R <- function (handle, text) {
    process_write(handle, paste0("cat('", text, "')\n"))
  }

  on.exit(terminate_gracefully(handle1))
  handle1 <- R_child()

  print_in_R(handle1, "a\\xF0\\x90")
  expect_equal(process_read(handle1, timeout = TIMEOUT_INFINITE)$stdout, 'a')
  
  print_in_R(handle1, "\\x8D\\x88b")
  expect_equal(process_read(handle1, timeout = TIMEOUT_INFINITE)$stdout, '\xF0\x90\x8D\x88b')
})
