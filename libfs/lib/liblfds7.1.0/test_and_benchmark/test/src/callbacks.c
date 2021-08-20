/***** includes *****/
#include "internal.h"





/****************************************************************************/
void callback_test_start( char *test_name )
{
  assert( test_name != NULL );

  printf( "%s...", test_name );
  fflush( stdout );

  return;
}





/****************************************************************************/
void callback_test_finish( char *result )
{
  assert( result != NULL );

  printf( "%s\n", result );

  return;
}

