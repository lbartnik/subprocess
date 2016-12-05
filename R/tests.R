
#' Run tests implemented in C.
#' 
#' If there is an error in 
#' 
#' @return A string \code{"All C tests passed!"} if there are no errors.
#' @export
#' @rdname tests
#' 
C_tests <- function ()
{
  ret <- .Call("test_consume_utf8");

  if (ret == 0) {  
    return("All C tests passed!")
  }
  
  paste0(ret, " error(s) encountered in C tests, see warnings() for details")
}
