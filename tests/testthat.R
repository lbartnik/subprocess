library(testthat)
library(subprocess)

# See for more details:
# https://github.com/hadley/testthat/issues/144
# https://github.com/hadley/testthat/issues/86
# https://github.com/luckyrandom/cmaker/commit/b85813ac2b7aef69932eca8fbb4fa0ec225e0af0
#
# When R CMD check runs tests, it sets R_TESTS. When the tests
# themeselves run R CMD xxxx, as is the case with the tests in
# devtools, having R_TESTS set causes errors because it confuses
# the R subprocesses. Unsetting it here avoids those problems.
#
# We need this because we spawn child R processes and they get very
# confused if this variable (R_TESTS) is present in their environment.
Sys.setenv("R_TESTS" = "")

test_check("subprocess")
