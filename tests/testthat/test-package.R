context("package")

test_that("onLoad is correct", {
  known <- subprocess:::known_signals()
  expect_true(is.integer(known))

  assignMock <- mock(T, cycle = TRUE)
  with_mock(assign = assignMock, {
    .onLoad('libname', 'subprocess')
  })

  expect_no_calls(assignMock, length(known))
})
