context("package")

test_that("onLoad is correct", {
  known <- subprocess:::known_signals()
  expect_true(is.integer(known))

  # do not mess with the actual namespace
  dotOnLoad  <- subprocess:::.onLoad
  environment(dotOnLoad) <- new.env(parent = asNamespace("subprocess"))
  environment(dotOnLoad)$signals <- list()

  # intercept assignments
  assignMock <- mock(T, cycle = TRUE)
  mockery::stub(dotOnLoad, 'assign', assignMock)
  dotOnLoad('libname', 'subprocess')

  expect_called(assignMock, length(known))
})
