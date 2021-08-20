/***** defines *****/
#define LIBBENCHMARK_PRNG_MAX   ( (lfds710_pal_uint_t) -1 )

// TRD : 32-bit SplitMix, derived from Sebastiano vigna's site, CC0 license, http://xorshift.di.unimi.it/splitmix64.c, and email with Dr. Vigna
#if( LFDS710_PAL_ALIGN_SINGLE_POINTER == 4 ) // TRD : any 32-bit platform
  // TRD : struct LIBBENCHMARK_prng_state prng_state, lfds710_pal_uint_t seed
  #define LIBBENCHMARK_PRNG_INIT( prng_state, seed )  (prng_state).entropy = (seed), (prng_state).entropy = ((prng_state).entropy ^ ((prng_state).entropy >> 16)) * 0x85ebca6bUL, (prng_state).entropy = ((prng_state).entropy ^ ((prng_state).entropy >> 13)) * 0xc2b2ae35UL, (prng_state).entropy = ((prng_state).entropy ^ ((prng_state).entropy >> 16))

  // TRD : struct libbenchmark_prng_state prng_state, LIBBENCHMARK_pal_atom_t random_value
  #define LIBBENCHMARK_PRNG_GENERATE( prng_state, random_value )                \
  {                                                                             \
    (random_value) = ( (prng_state).entropy += 0x9E3779B9UL );                  \
    (random_value) = ((random_value) ^ ((random_value) >> 16)) * 0x85ebca6bUL;  \
    (random_value) = ((random_value) ^ ((random_value) >> 13)) * 0xc2b2ae35UL;  \
    (random_value) = (random_value ^ (random_value >> 16));                     \
  }

  #define LIBBENCHMARK_PRNG_MURMURHASH3_MIXING_FUNCTION( random_value )  \
  {                                                                      \
  	(random_value) ^= ((random_value) >> 16);                            \
  	(random_value) *= 0x85ebca6b;                                        \
  	(random_value) ^= ((random_value) >> 13);                            \
  	(random_value) *= 0xc2b2ae35;                                        \
  	(random_value) ^= ((random_value) >> 16);                            \
  }
#endif

// TRD : 64-bit SplitMix, from Sebastiano vigna's site, CC0 license, http://xorshift.di.unimi.it/splitmix64.c
#if( LFDS710_PAL_ALIGN_SINGLE_POINTER == 8 ) // TRD : any 64-bit platform
  // TRD : struct LIBBENCHMARK_prng_state prng_state, LIBBENCHMARK_atom_uint_t seed
  #define LIBBENCHMARK_PRNG_INIT( prng_state, seed )  (prng_state).entropy = (seed), (prng_state).entropy = ((prng_state).entropy ^ ((prng_state).entropy >> 33)) * 0xff51afd7ed558ccdULL, (prng_state).entropy = ((prng_state).entropy ^ ((prng_state).entropy >> 33)) * 0xc4ceb9fe1a85ec53ULL, (prng_state).entropy = ((prng_state).entropy ^ ((prng_state).entropy >> 33))

  // TRD : struct libbenchmark_prng_state prng_state, lfds710_pal_uint_t random_value
  #define LIBBENCHMARK_PRNG_GENERATE( prng_state, random_value )                         \
  {                                                                                      \
    (random_value) = ( (prng_state).entropy += 0x9E3779B97F4A7C15ULL );                  \
    (random_value) = ((random_value) ^ ((random_value) >> 30)) * 0xBF58476D1CE4E5B9ULL;  \
    (random_value) = ((random_value) ^ ((random_value) >> 27)) * 0x94D049BB133111EBULL;  \
    (random_value) = (random_value ^ (random_value >> 31));                              \
  }

  #define LIBBENCHMARK_PRNG_MURMURHASH3_MIXING_FUNCTION( random_value )  \
  {                                                                      \
  	(random_value) ^= (random_value) >> 33;                              \
  	(random_value) *= 0xff51afd7ed558ccdULL;                             \
  	(random_value) ^= (random_value) >> 33;                              \
  	(random_value) *= 0xc4ceb9fe1a85ec53ULL;                             \
  	(random_value) ^= (random_value) >> 33;                              \
  }
#endif

/***** enums *****/

/***** structs *****/
struct libbenchmark_prng_state
{
  lfds710_pal_uint_t
    entropy;
};

/***** externs *****/

/***** public prototypes *****/

