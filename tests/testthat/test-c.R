context("C tests")

test_that("C tests pass", {
  expect_equal(C_tests(), "All C tests passed!")
})
