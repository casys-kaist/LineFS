/***** includes *****/
#include "libshared_ansi_internal.h"





/****************************************************************************/
#pragma warning( disable : 4706 )

void libshared_ansi_strcpy( char *destination, char const *source )
{
  LFDS710_PAL_ASSERT( destination != NULL );
  LFDS710_PAL_ASSERT( source != NULL );

  while( (*destination++ = *source++) );

  return;
}

#pragma warning( default : 4706 )

