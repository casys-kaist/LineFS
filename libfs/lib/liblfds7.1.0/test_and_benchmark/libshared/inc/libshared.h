#ifndef LIBSHARED_H

  /***** defines *****/
  #define LIBSHARED_H

  /***** enums *****/
  enum flag
  {
    LOWERED,
    RAISED
  };

  /***** platform includes *****/
  #include "libshared/libshared_porting_abstraction_layer_operating_system.h"

  /***** extermal includes *****/
  #include "../../../liblfds710/inc/liblfds710.h"

  /***** includes *****/
  #include "libshared/libshared_ansi.h"
  #include "libshared/libshared_memory.h"
  #include "libshared/libshared_misc.h"
  #include "libshared/libshared_porting_abstraction_layer.h"

#endif

