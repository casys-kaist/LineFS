/****************************************************************************/
#if( defined __GNUC__ )

  #ifdef LIBTEST_PAL_COMPILER
    #error More than one porting abstraction layer matches the current platform in "libtest_porting_abstraction_layer_compiler.h".
  #endif

  #define LIBTEST_PAL_COMPILER

  #if( defined __arm__ )
    // TRD : lfds710_pal_uint_t destination, lfds710_pal_uint_t *source
    #define LIBTEST_PAL_LOAD_LINKED( destination, source )  \
    {                                                       \
      __asm__ __volatile__                                  \
      (                                                     \
        "ldrex  %[alias_dst], [%[alias_src]];"              \
        : [alias_dst] "=r" (destination)                    \
        : [alias_src] "r" (source)                          \
      );                                                    \
    }

    // TRD : lfds710_pal_uint_t *destination, lfds710_pal_uint_t source, lfds710_pal_uint_t stored_flag
    #define LIBTEST_PAL_STORE_CONDITIONAL( destination, source, stored_flag )    \
    {                                                                            \
      __asm__ __volatile__                                                       \
      (                                                                          \
        "strex  %[alias_sf], %[alias_src], [%[alias_dst]];"                      \
        : "=m" (*destination),                                                   \
          [alias_sf] "=&r" (stored_flag)                                         \
        : [alias_src] "r" (source),                                              \
          [alias_dst] "r" (destination)                                          \
      );                                                                         \
    }
  #endif

#endif





/****************************************************************************/
#if( defined _WIN32 && defined KERNEL_MODE && defined _M_IX86 )

  #define LIBTEST_PAL_PROCESSOR

  // TRD : bloody x86 on MSVC...

  #define LIBTEST_PAL_STDLIB_CALLBACK_CALLING_CONVENTION  __cdecl

#endif





/****************************************************************************/
#if( !defined LIBTEST_PAL_PROCESSOR )

  #define LIBTEST_PAL_STDLIB_CALLBACK_CALLING_CONVENTION

#endif

