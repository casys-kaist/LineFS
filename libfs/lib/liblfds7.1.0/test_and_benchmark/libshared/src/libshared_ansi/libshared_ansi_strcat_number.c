/***** includes *****/
#include "libshared_ansi_internal.h"





/****************************************************************************/
void libshared_ansi_strcat_number( char *destination, lfds710_pal_uint_t number )
{
  lfds710_pal_uint_t
    digit,
    length = 0,
    original_number;

  LFDS710_PAL_ASSERT( destination != NULL );
  // TRD : number can be any value in its range

  // TRD : point destination at the end of the string
  while( *destination++ != '\0' );

  destination--;

  // TRD : figure out length of the number

  original_number = number;

  do
  {
    digit = number % 10;
    length++;
    number -= digit;
    number /= 10;
  }
  while( number > 0 );

  destination[length] = '\0';

  // TRD : copy over the number digits - note we get them the right way around

  number = original_number;

  do
  {
    digit = number % 10;
    destination[--length] = (char) ( digit + '0' );
    number -= digit;
    number /= 10;
  }
  while( number > 0 );

  return;
}





/****************************************************************************/
void libshared_ansi_strcat_number_with_leading_zeros( char *destination, lfds710_pal_uint_t number, lfds710_pal_uint_t minimum_width )
{
  lfds710_pal_uint_t
    digit,
    length = 0,
    loop,
    original_number;

  LFDS710_PAL_ASSERT( destination != NULL );
  // TRD : number can be any value in its range
  // TRD : minimum_width can be any value in its range

  // TRD : point destination at the end of the string
  while( *destination++ != '\0' );

  destination--;

  // TRD : figure out length of the number

  original_number = number;

  do
  {
    digit = number % 10;
    length++;
    number -= digit;
    number /= 10;
  }
  while( number > 0 );

  if( length < minimum_width )
    for( loop = 0 ; loop < minimum_width - length ; loop++ )
      *destination++ = '0';

  destination[length] = '\0';

  // TRD : copy over the number digits - note we get them the right way around

  number = original_number;

  do
  {
    digit = number % 10;
    destination[--length] = (char) ( digit + '0' );
    number -= digit;
    number /= 10;
  }
  while( number > 0 );

  return;
}

