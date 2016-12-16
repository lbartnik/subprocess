#include <R.h>
#include <Rdefines.h>


int is_single_string (SEXP _obj)
{
  return isString(_obj) && (LENGTH(_obj) == 1);
}


int is_nonempty_string (SEXP _obj)
{
  return is_single_string(_obj) && (strlen(translateChar(STRING_ELT(_obj, 0))) > 0);
}


int is_single_string_or_NULL (SEXP _obj)
{
  return is_single_string(_obj) || (_obj == R_NilValue);
}


int is_single_integer (SEXP _obj)
{
  return isInteger(_obj) && (LENGTH(_obj) == 1);
}
