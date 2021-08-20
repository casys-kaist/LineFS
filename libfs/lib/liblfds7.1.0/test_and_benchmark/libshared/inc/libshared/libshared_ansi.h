/***** defines *****/

/***** enums *****/

/***** structs *****/

/***** public prototypes *****/
lfds710_pal_uint_t libshared_ansi_strlen( char const * const string );
void libshared_ansi_strcpy( char *destination, char const *source );
void libshared_ansi_strcat( char *destination, char const * const source );
void libshared_ansi_strcat_number( char *destination, lfds710_pal_uint_t number );
void libshared_ansi_strcat_number_with_leading_zeros( char *destination, lfds710_pal_uint_t number, lfds710_pal_uint_t minimum_width );
void libshared_ansi_strcat_char( char *destination, char const source );

