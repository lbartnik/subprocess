
#' Run UTF8 tests implemented in C.
#' 
#' If there is no error in those tests a simple string message
#' is returned. If there is at least one error, another message
#' is returned.
#' 
#' @return A string `"All C tests passed!"` if there are no errors.
#' @export
#' @rdname tests
#' 
C_tests_utf8 <- function ()
{
  ret <- .Call("test_consume_utf8");

  if (ret == 0) {  
    return("All C tests passed!")
  }
  
  paste0(ret, " error(s) encountered in C tests, see warnings() for details")
}

