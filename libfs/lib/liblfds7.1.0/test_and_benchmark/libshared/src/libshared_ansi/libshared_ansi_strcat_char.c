/***** includes *****/
#include "libshared_ansi_internal.h"





/****************************************************************************/
void libshared_ansi_strcat_char( char *destination, char const source )
{
  LFDS710_PAL_ASSERT( destination != NULL );
  // TRD : source can be any value in its range

  while( *destination++ != '\0' );

  *(destination-1) = source;
  *destination = '\0';

  return;
}

