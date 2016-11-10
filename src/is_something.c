#include <R.h>
#include <Rdefines.h>

int is_nonempty_string (SEXP _obj)
{
  return isString(_obj) && (LENGTH(_obj) == 1) &&
	  (strlen(translateChar(STRING_ELT(_obj, 0))) > 0);
}

int is_nonempty_string_or_NULL (SEXP _obj)
{
  return is_nonempty_string(_obj) || (_obj == R_NilValue);
}
