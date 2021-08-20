#ifndef LIBTEST_H

  /***** defines *****/
  #define LIBTEST_H

  /***** enums *****/

  /***** porting includes *****/
  #include "libtest/libtest_porting_abstraction_layer_operating_system.h"
  #include "libtest/libtest_porting_abstraction_layer_compiler.h"

  /***** extermal includes *****/
  #include "../../../liblfds710/inc/liblfds710.h"
  #include "../../libshared/inc/libshared.h"

  /***** includes *****/
  #include "libtest/libtest_porting_abstraction_layer.h"
  #include "libtest/libtest_misc.h"
  #include "libtest/libtest_tests.h"
  #include "libtest/libtest_test.h"       // TRD : test depends on tests
  #include "libtest/libtest_results.h"    // TRD : results depends on tests
  #include "libtest/libtest_testsuite.h"
  #include "libtest/libtest_threadset.h"

#endif

