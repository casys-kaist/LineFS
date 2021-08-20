/***** includes *****/
#include "libbenchmark_porting_abstraction_layer_internal.h"





/****************************************************************************/
#if( defined _WIN32 && !defined KERNEL_MODE && NTDDI_VERSION >= NTDDI_WINXPSP3 && NTDDI_VERSION < NTDDI_WIN7 )

  #ifdef LIBBENCHMARK_PAL_POPULATE_TOPOLOGY
    #error More than one porting abstraction layer matches current platform in "libbenchmark_porting_abstraction_layer_populate_topology.c".
  #endif

  #define LIBBENCHMARK_PAL_POPULATE_TOPOLOGY

  static void internal_populate_logical_processor_array_from_bitmask( struct libshared_memory_state *ms, struct libbenchmark_topology_node_state *tns, lfds710_pal_uint_t bitmask );

  int libbenchmark_porting_abstraction_layer_populate_topology( struct libbenchmark_topology_state *ts,
                                                                struct libshared_memory_state *ms )
  {
    BOOL
      brv;

    DWORD
      slpi_length = 0,
      number_slpi,
      loop;

    enum libbenchmark_topology_node_cache_type
      processor_cache_type_to_libbenchmark_topology_node_cache_type[3] = 
      {
        LIBBENCHMARK_TOPOLOGY_NODE_CACHE_TYPE_UNIFIED, LIBBENCHMARK_TOPOLOGY_NODE_CACHE_TYPE_INSTRUCTION, LIBBENCHMARK_TOPOLOGY_NODE_CACHE_TYPE_DATA
      };

    int
      rv = 1;

    struct libbenchmark_topology_node_state
      *tns;

    SYSTEM_LOGICAL_PROCESSOR_INFORMATION
      *slpi = NULL;

    ULONG_PTR
      mask;

    LFDS710_PAL_ASSERT( ts != NULL );
    LFDS710_PAL_ASSERT( ms != NULL );

    // TRD : obtain information from the OS
    brv = GetLogicalProcessorInformation( slpi, &slpi_length );
    slpi = libshared_memory_alloc_from_most_free_space_node( ms, slpi_length, sizeof(lfds710_pal_uint_t) );
    brv = GetLogicalProcessorInformation( slpi, &slpi_length );
    number_slpi = slpi_length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

    /* TRD : we loop twice over the topology information
             first time we form up the system node
             and add that
             second time, we do everything else
    */

    libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );

    for( loop = 0 ; loop < number_slpi ; loop++ )
      if( (slpi+loop)->Relationship == RelationNumaNode )
        internal_populate_logical_processor_array_from_bitmask( ms, tns, (lfds710_pal_uint_t) (slpi+loop)->ProcessorMask );

    libbenchmark_misc_pal_helper_add_system_node_to_topology_tree( ts, tns );

    for( loop = 0 ; loop < number_slpi ; loop++ )
    {
      if( (slpi+loop)->Relationship == RelationNumaNode )
      {
        libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
        internal_populate_logical_processor_array_from_bitmask( ms, tns, (lfds710_pal_uint_t) ((slpi+loop)->ProcessorMask) );
        libbenchmark_misc_pal_helper_add_numa_node_to_topology_tree( ts, tns, (lfds710_pal_uint_t) (slpi+loop)->NumaNode.NodeNumber );

        // TRD : add each LP as an individual LP node
        for( mask = 1 ; mask != 0 ; mask <<= 1 )
          if( ((slpi+loop)->ProcessorMask & mask) == mask )
            libbenchmark_misc_pal_helper_add_logical_processor_node_to_topology_tree( ts, ms, (lfds710_pal_uint_t) ((slpi+loop)->ProcessorMask & mask), LOWERED, 0 );
      }

      if( (slpi+loop)->Relationship == RelationProcessorPackage )
      {
        libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
        internal_populate_logical_processor_array_from_bitmask( ms, tns, (lfds710_pal_uint_t) ((slpi+loop)->ProcessorMask) );
        libbenchmark_misc_pal_helper_add_socket_node_to_topology_tree( ts, tns );
      }

      if( (slpi+loop)->Relationship == RelationProcessorCore )
      {
        libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
        internal_populate_logical_processor_array_from_bitmask( ms, tns, (lfds710_pal_uint_t) ((slpi+loop)->ProcessorMask) );
        libbenchmark_misc_pal_helper_add_physical_processor_node_to_topology_tree( ts, tns );
      }

      if( (slpi+loop)->Relationship == RelationCache )
      {
        if( (slpi+loop)->Cache.Type == CacheUnified or (slpi+loop)->Cache.Type == CacheInstruction or (slpi+loop)->Cache.Type == CacheData )
        {
          libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
          internal_populate_logical_processor_array_from_bitmask( ms, tns, (lfds710_pal_uint_t) (slpi+loop)->ProcessorMask );
          libbenchmark_misc_pal_helper_add_cache_node_to_topology_tree( ts, tns, (lfds710_pal_uint_t) (slpi+loop)->Cache.Level, processor_cache_type_to_libbenchmark_topology_node_cache_type[(slpi+loop)->Cache.Type] );
        }
      }
    }

    return rv;
  }

  /****************************************************************************/
  static void internal_populate_logical_processor_array_from_bitmask( struct libshared_memory_state *ms,
                                                                      struct libbenchmark_topology_node_state *tns,
                                                                      lfds710_pal_uint_t bitmask )
  {
    lfds710_pal_uint_t
      logical_processor_number = 1;

    struct libbenchmark_topology_node_state
      *tns_temp;

    LFDS710_PAL_ASSERT( ms != NULL );
    LFDS710_PAL_ASSERT( tns != NULL );
    // TRD : bitmask can be any value in its range

    /* TRD : iterate over the bits in the bitmask
             each is a LP number
             add every LP to *tns
    */

    while( bitmask != 0 )
    {
      if( bitmask & 0x1 )
        libbenchmark_misc_pal_helper_add_logical_processor_to_topology_node( tns, ms, logical_processor_number, LOWERED, 0 );

      bitmask >>= 1;
      logical_processor_number++;
    }

    return;
  }

#endif





/****************************************************************************/
#if( defined _WIN32 && !defined KERNEL_MODE && NTDDI_VERSION >= NTDDI_WIN7 )

  #ifdef LIBBENCHMARK_PAL_POPULATE_TOPOLOGY
    #error More than one porting abstraction layer matches current platform in "libbenchmark_porting_abstraction_layer_populate_topology.c".
  #endif

  #define LIBBENCHMARK_PAL_POPULATE_TOPOLOGY

  static int numa_node_id_to_numa_node_id_compare_function( void const *new_key, void const *existing_key );
  static void nna_cleanup( struct lfds710_btree_au_state *baus, struct lfds710_btree_au_element *baue );
  static void internal_populate_logical_processor_array_from_bitmask( struct libshared_memory_state *ms, struct libbenchmark_topology_node_state *tns, lfds710_pal_uint_t windows_processor_group_number, lfds710_pal_uint_t bitmask );

  int libbenchmark_porting_abstraction_layer_populate_topology( struct libbenchmark_topology_state *ts,
                                                                struct libshared_memory_state *ms )
  {
    BOOL
      brv;

    DWORD
      offset = 0,
      slpie_length = 0,
      subloop;

    /*
    enum libbenchmark_topology_node_cache_type
      processor_cache_type_to_libbenchmark_topology_node_cache_type[3] = 
      {
        LIBBENCHMARK_TOPOLOGY_NODE_CACHE_TYPE_UNIFIED, LIBBENCHMARK_TOPOLOGY_NODE_CACHE_TYPE_INSTRUCTION, LIBBENCHMARK_TOPOLOGY_NODE_CACHE_TYPE_DATA
      };
    */

    int
      rv = 1;

    KAFFINITY
      bitmask;

    lfds710_pal_uint_t
      logical_processor_number;

    struct lfds710_btree_au_element
      *baue;

    struct lfds710_btree_au_state
      nna_tree_state;

    struct libbenchmark_topology_node_state
      *tns;

    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX
      *slpie,
      *slpie_buffer = NULL;

    LFDS710_PAL_ASSERT( ts != NULL );
    LFDS710_PAL_ASSERT( ms != NULL );

    // TRD : obtain information from the OS
    brv = GetLogicalProcessorInformationEx( RelationAll, slpie_buffer, &slpie_length );
    slpie_buffer = libshared_memory_alloc_from_most_free_space_node( ms, slpie_length, sizeof(lfds710_pal_uint_t) );
    brv = GetLogicalProcessorInformationEx( RelationAll, slpie_buffer, &slpie_length );

    /* TRD : this API from MS is absolutely bloody appalling
             staggeringly and completely needlessly complex and inadequately documented
             I think I've found at least one design flaw
             and I'm inferring from the C structures a good deal of what's presumably going on
             where the docs just don't say

             (addendum - I've just found another huge fucking issue which has wasted two fucking days of my time
              the original non-Ex() API returns an actual C array, where the elements are structs, which contain
              a union, but in C the struct is sized to the max size of the union, so you can iterate over the array

              the NEW version, in the docs still says "array", but it actually returns a PACKED "array" (not an
              array, because you can't iterate over it) where the each element now has a Size member - you need
              to move your pointer by the number of bytes in Size - this is NOT in the docs, there is NO example
              code, and the ONLY WAY YOU CAN GUESS IS TO NOTICE THERE IS A SIZE MEMBER IN THE NEW STRUCT)

             (for example, just found a one-liner buried in the note on a particular structure
              returned for a particular node type;

              "If the PROCESSOR_RELATIONSHIP structure represents a processor core, the GroupCount member is always 1."

              this *implies* that a physical core is never split across groups
              this is a very important fact, if you're trying to work with this fucking API
              but it's not actually SPECIFICALLY STATED
              it's only implied - and so I do not feel confident in it
              and the appalling design and appallingly low quality of the docs in general hardly gives me confidence
              to just go ahead and believe in anything I find written - let alone something which is, offfhand, just
              implies, buried in some structure notes somewhere
              this is how it is all the way across this entire bloody API
              another example is that LPs are not actually returned by the API
              I'm *inferring* I can get the full list by taking the LP masks presented by the NUMA nodes
              it's *not* documented - i.e. it's not documented HOW TO GET THE LIST OF LOGICAL PROCESSORS IN THE SYSTEM
              fucking christ...!)

             I'm absolutely 100% certain my use of the API is not fully correct
             but I have no way to find out
             MS are bloody idiots - the "processor group" concept is absolutely and utterly crazy
             and it complicates *everything* by a power of 2
             rather than simply iterating over the records provided,
             where just about any entity in the system (NUMA node, processor socket, etc)
             can have multiple records being returned, I have in fact to iterate over the whole
             record set, accumulating the multiple records, so I can FINALLY find out the full
             logical processor set for any given entity, so I can THEN, FINALLY, insert the entity
             into the toplogy tree
             i.e. for any given node, you have to fully iterate the list of records provided by 
             the OS, to actually know you know all the LPs for that node
             there is no single-entity/single-record lookup or relationship
             MS -> you are bloody idiots; this is appalling, OBVIOUSLY appalling, and whoever
             designed it, and ESPECIALLY whoever APPROVED It, needs not only to be fired, but SHOT

             as ever with MS, something that takes a few minutes in Linux takes bloody hours with MS

             note due to aforementioned design flaw, it is not possible to collect cache information
             the problem is that if we have a cache which spans multiple processor groups, there will
             be mutiple records (or I presume there will be - I'm inferring), BUT, looking at the
             structures, it's not possible to know these are *the same cache*

             so, this mess;

             1. RelationNumaNode
                - we need to loop over the full list of records to accumulate the full set of LPs for each NUMA node
                  then we can add the record to tbe btree
             2. RelationGroup
                - really REALLY don't care - with prejudice
             3. RelationProcessorPackage
                - bizarrely, actually does the right thing (as far as it can be right in this sorry mess) and contains
                  the full list of group IDs it belongs to, and the full list of LP IDs within each group
                  so we can iterate once over the full set of records and insert this record type directly
             4. RelationProcessorCore
                - same as RelationProcessorPackage
             5. RelationCache
                - seems fubared; provide a single processor group and single mask of LPs, and so if a cache spans
                  multiple processor groups, we'll get multiple records for it - problem is, we've no way of knowing
                  *its the same cache*
                  we get away with this with NUMA because each node has an ID
                  the next best thing is going to be record the details of the cache from the structure
                  (level, associativity, etc) and match based on that
                  God I hate Microsoft
    */

    // TRD : iterate once for system node
    libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );

    while( offset < slpie_length )
    {
      slpie = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *) ( (char unsigned *) slpie_buffer + offset );

      offset += slpie->Size;

      if( slpie->Relationship == RelationNumaNode )
        internal_populate_logical_processor_array_from_bitmask( ms, tns, (lfds710_pal_uint_t) (slpie->NumaNode.GroupMask.Group), (lfds710_pal_uint_t) (slpie->NumaNode.GroupMask.Mask) );
    }

    libbenchmark_misc_pal_helper_add_system_node_to_topology_tree( ts, tns );

    // TRD : iterate again for everything else
    lfds710_btree_au_init_valid_on_current_logical_core( &nna_tree_state, numa_node_id_to_numa_node_id_compare_function, LFDS710_BTREE_AU_INSERT_RESULT_FAILURE_EXISTING_KEY, ts );

    offset = 0;

    while( offset < slpie_length )
    {
      slpie = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *) ( (char unsigned *) slpie_buffer + offset );

      offset += slpie->Size;

      if( slpie->Relationship == RelationNumaNode )
      {
        /* TRD : now for the first madness - accumulate the NUMA node records

                 first, try to find this node in nna_tree_state
                 if it's there, we use it - it not, we make it and add it, and use it
                 once we've got a node to work with, we add the current list of LPs to that node
        */

        rv = lfds710_btree_au_get_by_key( &nna_tree_state, NULL, (void *) &slpie->NumaNode.NodeNumber, &baue );

        if( rv == 0 )
        {
          libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
          baue = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct lfds710_btree_au_element), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
          LFDS710_BTREE_AU_SET_KEY_IN_ELEMENT( *baue, (void *) &slpie->NumaNode.NodeNumber );
          LFDS710_BTREE_AU_SET_VALUE_IN_ELEMENT( *baue, tns );
          lfds710_btree_au_insert( &nna_tree_state, baue, NULL );
        }

        // TRD : baue now points at the correct node
        tns = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *baue );
        internal_populate_logical_processor_array_from_bitmask( ms, tns, (lfds710_pal_uint_t) slpie->NumaNode.GroupMask.Group, (lfds710_pal_uint_t) slpie->NumaNode.GroupMask.Mask );

        // TRD : now all all LPs from this NUMA node to tree
        logical_processor_number = 0;
        bitmask = slpie->NumaNode.GroupMask.Mask;

        while( bitmask != 0 )
        {
          if( bitmask & 0x1 )
            libbenchmark_misc_pal_helper_add_logical_processor_node_to_topology_tree( ts, ms, logical_processor_number, RAISED, (slpie->NumaNode.GroupMask.Group) );

          bitmask >>= 1;
          logical_processor_number++;
        }
      }

      if( slpie->Relationship == RelationGroup )
      {
        // TRD : we don't care about this - actually, we do care, we really REALLY hate this
      }

      if( slpie->Relationship == RelationProcessorPackage )
      {
        libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
        for( subloop = 0 ; subloop < slpie->Processor.GroupCount ; subloop++ )
          internal_populate_logical_processor_array_from_bitmask( ms, tns, (lfds710_pal_uint_t) slpie->Processor.GroupMask[subloop].Group, (lfds710_pal_uint_t) slpie->Processor.GroupMask[subloop].Mask );
        libbenchmark_misc_pal_helper_add_socket_node_to_topology_tree( ts, tns );
      }

      if( slpie->Relationship == RelationProcessorCore )
      {
        libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
        for( subloop = 0 ; subloop < slpie->Processor.GroupCount ; subloop++ )
          internal_populate_logical_processor_array_from_bitmask( ms, tns, (lfds710_pal_uint_t) slpie->Processor.GroupMask[subloop].Group, (lfds710_pal_uint_t) slpie->Processor.GroupMask[subloop].Mask );
        libbenchmark_misc_pal_helper_add_physical_processor_node_to_topology_tree( ts, tns );
      }

      /*
      if( slpie->Relationship == RelationCache )
      {
        if( slpie->Cache.Type == CacheUnified or slpie->Cache.Type == CacheInstruction or slpie->Cache.Type == CacheData )
        {
          libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
          internal_populate_logical_processor_array_from_bitmask( ms, tns, (lfds710_pal_uint_t) slpie->ProcessorMask );
          libbenchmark_misc_pal_helper_add_cache_node_to_topology_tree( ts, tns, (lfds710_pal_uint_t) slpie->Cache.Level, processor_cache_type_to_libbenchmark_topology_node_cache_type[slpie->Cache.Type] );
        }
      }
      */
    }

    /* TRD : now finally insert the built-up NUMA and cache records
             we call cleanup() on the accumulator tree - it's safe to re-use the nodes as they're emitted to the cleanup function
             so we then throw them into the topology_state tree
    */

    lfds710_btree_au_cleanup( &nna_tree_state, nna_cleanup );

    return rv;
  }

  /****************************************************************************/
  static int numa_node_id_to_numa_node_id_compare_function( void const *new_key, void const *existing_key )
  {
    int
      cr = 0;

    DWORD
      numa_node_id_existing,
      numa_node_id_new;

    LFDS710_PAL_ASSERT( new_key != NULL );
    LFDS710_PAL_ASSERT( existing_key != NULL );

    numa_node_id_new = *(DWORD *) new_key;
    numa_node_id_existing = *(DWORD *) existing_key;

    if( numa_node_id_new < numa_node_id_existing )
      cr = -1;

    if( numa_node_id_new > numa_node_id_existing )
      cr = 1;

    return cr;
  }

  /****************************************************************************/
  static void nna_cleanup( struct lfds710_btree_au_state *baus, struct lfds710_btree_au_element *baue )
  {
    DWORD
      *numa_node_id;

    struct libbenchmark_topology_node_state
      *tns;

    struct libbenchmark_topology_state
      *ts;

    LFDS710_PAL_ASSERT( baus != NULL );
    LFDS710_PAL_ASSERT( baue != NULL );

    ts = LFDS710_BTREE_AU_GET_USER_STATE_FROM_STATE( *baus );
    numa_node_id = LFDS710_BTREE_AU_GET_KEY_FROM_ELEMENT( *baue );
    tns = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *baue );

    libbenchmark_misc_pal_helper_add_numa_node_to_topology_tree( ts, tns, (lfds710_pal_uint_t) *numa_node_id );

    return;
  }

  /****************************************************************************/
  static void internal_populate_logical_processor_array_from_bitmask( struct libshared_memory_state *ms,
                                                                      struct libbenchmark_topology_node_state *tns,
                                                                      lfds710_pal_uint_t windows_processor_group_number,
                                                                      lfds710_pal_uint_t bitmask )
  {
    lfds710_pal_uint_t
      logical_processor_number = 0;

    LFDS710_PAL_ASSERT( ms != NULL );
    LFDS710_PAL_ASSERT( tns != NULL );
    // TRD : windows_processor_group_number can be any value in its range
    // TRD : bitmask can be any value in its range

    /* TRD : iterate over the bits in the bitmask
             each is a LP number
             add every LP to *tns
    */

    while( bitmask != 0 )
    {
      if( bitmask & 0x1 )
        libbenchmark_misc_pal_helper_add_logical_processor_to_topology_node( tns, ms, logical_processor_number, RAISED, windows_processor_group_number );

      bitmask >>= 1;
      logical_processor_number++;
    }

    return;
  }

#endif





/****************************************************************************/
#if( defined _WIN32 && defined KERNEL_MODE && NTDDI_VERSION >= NTDDI_WINXP && NTDDI_VERSION < NTDDI_WIN7 )

  #ifdef LIBBENCHMARK_PAL_POPULATE_TOPOLOGY
    #error More than one porting abstraction layer matches current platform in "libbenchmark_porting_abstraction_layer_populate_topology.c".
  #endif

  #define LIBBENCHMARK_PAL_POPULATE_TOPOLOGY

  int libbenchmark_porting_abstraction_layer_populate_topology( struct libbenchmark_topology_state *ts,
                                                                struct libshared_memory_state *ms )
  {
    CCHAR
      loop;

    struct libbenchmark_topology_node_state
      *tns;

    LFDS710_PAL_ASSERT( ts != NULL );
    LFDS710_PAL_ASSERT( ms != NULL );

    /* TRD : prior to Windows 7 there is no way to enumerate CPU topology
             all that is available is a count of the number of logical cores, KeNumberProcessors
             this is in fact only available *up to Vista SP1*... Windows 7 provides full functionality to get topology,
             so it's not clear what should be done on Vista SP1...

             as such to get the topology actually right, the user has to hardcode it

             the best general solution seems to be to take the number of logical cores
             assumes they're all on one processor and there's one NUMA node
    */

    // TRD : create the system node, populate and insert
    libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
    for( loop = 0 ; loop < KeNumberProcessors ; loop++ )
      libbenchmark_misc_pal_helper_add_logical_processor_to_topology_node( tns, ms, loop, LOWERED, 0 );
    libbenchmark_misc_pal_helper_add_system_node_to_topology_tree( ts, tns );

    // TRD : create the NUMA node, populate and insert
    libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
    for( loop = 0 ; loop < KeNumberProcessors ; loop++ )
      libbenchmark_misc_pal_helper_add_logical_processor_to_topology_node( tns, ms, loop, LOWERED, 0 );
    libbenchmark_misc_pal_helper_add_numa_node_to_topology_tree( ts, tns, 0 );

    // TRD : create the socket node, populate and insert
    libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
    for( loop = 0 ; loop < KeNumberProcessors ; loop++ )
      libbenchmark_misc_pal_helper_add_logical_processor_to_topology_node( tns, ms, loop, LOWERED, 0 );
    libbenchmark_misc_pal_helper_add_socket_node_to_topology_tree( ts, tns );

    // TRD : create the physical processor node, populate and insert
    libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
    for( loop = 0 ; loop < KeNumberProcessors ; loop++ )
      libbenchmark_misc_pal_helper_add_logical_processor_to_topology_node( tns, ms, loop, LOWERED, 0 );
    libbenchmark_misc_pal_helper_add_physical_processor_node_to_topology_tree( ts, tns );

    // TRD : create the logical processor nodes, populate and insert
    for( loop = 0 ; loop < KeNumberProcessors ; loop++ )
      libbenchmark_misc_pal_helper_add_logical_processor_node_to_topology_tree( ts, ms, loop, LOWERED, 0 );

    return 1;
  }

#endif





/****************************************************************************/
#if( defined _WIN32 && defined KERNEL_MODE && NTDDI_VERSION >= NTDDI_WIN7 )

  #ifdef LIBBENCHMARK_PAL_POPULATE_TOPOLOGY
    #error More than one porting abstraction layer matches current platform in "libbenchmark_porting_abstraction_layer_populate_topology.c".
  #endif

  #define LIBBENCHMARK_PAL_POPULATE_TOPOLOGY

  static int numa_node_id_to_numa_node_id_compare_function( void const *new_key, void const *existing_key );
  static void nna_cleanup( struct lfds710_btree_au_state *baus, struct lfds710_btree_au_element *baue );
  static void internal_populate_logical_processor_array_from_bitmask( struct libshared_memory_state *ms, struct libbenchmark_topology_node_state *tns, lfds710_pal_uint_t windows_processor_group_number, lfds710_pal_uint_t bitmask );

  int libbenchmark_porting_abstraction_layer_populate_topology( struct libbenchmark_topology_state *ts,
                                                                struct libshared_memory_state *ms )
  {
    /*
    enum libbenchmark_topology_node_cache_type
      processor_cache_type_to_libbenchmark_topology_node_cache_type[3] = 
      {
        LIBBENCHMARK_TOPOLOGY_NODE_CACHE_TYPE_UNIFIED, LIBBENCHMARK_TOPOLOGY_NODE_CACHE_TYPE_INSTRUCTION, LIBBENCHMARK_TOPOLOGY_NODE_CACHE_TYPE_DATA
      };
    */

    int
      rv = 1;

    KAFFINITY
      bitmask;

    lfds710_pal_uint_t
      logical_processor_number;

    NTSTATUS
      brv;

    struct lfds710_btree_au_element
      *baue;

    struct lfds710_btree_au_state
      nna_tree_state;

    struct libbenchmark_topology_node_state
      *tns;

    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX
      *slpie,
      *slpie_buffer = NULL;

    ULONG
      offset = 0,
      slpie_length = 0,
      subloop;

    LFDS710_PAL_ASSERT( ts != NULL );
    LFDS710_PAL_ASSERT( ms != NULL );

    // TRD : obtain information from the OS
    brv = KeQueryLogicalProcessorRelationship( NULL, RelationAll, slpie_buffer, &slpie_length );
    slpie_buffer = libshared_memory_alloc_from_most_free_space_node( ms, slpie_length, sizeof(lfds710_pal_uint_t) );
    brv = KeQueryLogicalProcessorRelationship( NULL, RelationAll, slpie_buffer, &slpie_length );

    /* TRD : this API from MS is absolutely bloody appalling
             staggeringly and completely needlessly complex and inadequately documented
             I think I've found at least one design flaw
             and I'm inferring from the C structures a good deal of what's presumably going on
             where the docs just don't say

             (addendum - I've just found another huge fucking issue which has wasted two fucking days of my time
              the original non-Ex() API returns an actual C array, where the elements are structs, which contain
              a union, but in C the struct is sized to the max size of the union, so you can iterate over the array

              the NEW version, in the docs still says "array", but it actually returns a PACKED "array" (not an
              array, because you can't iterate over it) where the each element now has a Size member - you need
              to move your pointer by the number of bytes in Size - this is NOT in the docs, there is NO example
              code, and the ONLY WAY YOU CAN GUESS IS TO NOTICE THERE IS A SIZE MEMBER IN THE NEW STRUCT)

             (for example, just found a one-liner buried in the note on a particular structure
              returned for a particular node type;

              "If the PROCESSOR_RELATIONSHIP structure represents a processor core, the GroupCount member is always 1."

              this *implies* that a physical core is never split across groups
              this is a very important fact, if you're trying to work with this fucking API
              but it's not actually SPECIFICALLY STATED
              it's only implied - and so I do not feel confident in it
              and the appalling design and appallingly low quality of the docs in general hardly gives me confidence
              to just go ahead and believe in anything I find written - let alone something which is, offfhand, just
              implies, buried in some structure notes somewhere
              this is how it is all the way across this entire bloody API
              another example is that LPs are not actually returned by the API
              I'm *inferring* I can get the full list by taking the LP masks presented by the NUMA nodes
              it's *not* documented - i.e. it's not documented HOW TO GET THE LIST OF LOGICAL PROCESSORS IN THE SYSTEM
              fucking christ...!)

             I'm absolutely 100% certain my use of the API is not fully correct
             but I have no way to find out
             MS are bloody idiots - the "processor group" concept is absolutely and utterly crazy
             and it complicates *everything* by a power of 2
             rather than simply iterating over the records provided,
             where just about any entity in the system (NUMA node, processor socket, etc)
             can have multiple records being returned, I have in fact to iterate over the whole
             record set, accumulating the multiple records, so I can FINALLY find out the full
             logical processor set for any given entity, so I can THEN, FINALLY, insert the entity
             into the toplogy tree
             i.e. for any given node, you have to fully iterate the list of records provided by 
             the OS, to actually know you know all the LPs for that node
             there is no single-entity/single-record lookup or relationship
             MS -> you are bloody idiots; this is appalling, OBVIOUSLY appalling, and whoever
             designed it, and ESPECIALLY whoever APPROVED It, needs not only to be fired, but SHOT

             as ever with MS, something that takes a few minutes in Linux takes bloody hours with MS

             note due to aforementioned design flaw, it is not possible to collect cache information
             the problem is that if we have a cache which spans multiple processor groups, there will
             be mutiple records (or I presume there will be - I'm inferring), BUT, looking at the
             structures, it's not possible to know these are *the same cache*

             so, this mess;

             1. RelationNumaNode
                - we need to loop over the full list of records to accumulate the full set of LPs for each NUMA node
                  then we can add the record to tbe btree
             2. RelationGroup
                - really REALLY don't care - with prejudice
             3. RelationProcessorPackage
                - bizarrely, actually does the right thing (as far as it can be right in this sorry mess) and contains
                  the full list of group IDs it belongs to, and the full list of LP IDs within each group
                  so we can iterate once over the full set of records and insert this record type directly
             4. RelationProcessorCore
                - same as RelationProcessorPackage
             5. RelationCache
                - seems fubared; provide a single processor group and single mask of LPs, and so if a cache spans
                  multiple processor groups, we'll get multiple records for it - problem is, we've no way of knowing
                  *its the same cache*
                  we get away with this with NUMA because each node has an ID
                  the next best thing is going to be record the details of the cache from the structure
                  (level, associativity, etc) and match based on that
                  God I hate Microsoft
    */

    // TRD : iterate once for system node
    libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );

    while( offset < slpie_length )
    {
      slpie = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *) ( (char unsigned *) slpie_buffer + offset );

      offset += slpie->Size;

      if( slpie->Relationship == RelationNumaNode )
        internal_populate_logical_processor_array_from_bitmask( ms, tns, (lfds710_pal_uint_t) (slpie->NumaNode.GroupMask.Group), (lfds710_pal_uint_t) (slpie->NumaNode.GroupMask.Mask) );
    }

    libbenchmark_misc_pal_helper_add_system_node_to_topology_tree( ts, tns );

    // TRD : iterate again for everything else
    lfds710_btree_au_init_valid_on_current_logical_core( &nna_tree_state, numa_node_id_to_numa_node_id_compare_function, LFDS710_BTREE_AU_INSERT_RESULT_FAILURE_EXISTING_KEY, ts );

    offset = 0;

    while( offset < slpie_length )
    {
      slpie = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *) ( (char unsigned *) slpie_buffer + offset );

      offset += slpie->Size;

      if( slpie->Relationship == RelationNumaNode )
      {
        /* TRD : now for the first madness - accumulate the NUMA node records

                 first, try to find this node in nna_tree_state
                 if it's there, we use it - it not, we make it and add it, and use it
                 once we've got a node to work with, we add the current list of LPs to that node
        */

        rv = lfds710_btree_au_get_by_key( &nna_tree_state, NULL, (void *) &slpie->NumaNode.NodeNumber, &baue );

        if( rv == 0 )
        {
          libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
          baue = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct lfds710_btree_au_element), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
          LFDS710_BTREE_AU_SET_KEY_IN_ELEMENT( *baue, (void *) &slpie->NumaNode.NodeNumber );
          LFDS710_BTREE_AU_SET_VALUE_IN_ELEMENT( *baue, tns );
          lfds710_btree_au_insert( &nna_tree_state, baue, NULL );
        }

        // TRD : baue now points at the correct node
        tns = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *baue );
        internal_populate_logical_processor_array_from_bitmask( ms, tns, (lfds710_pal_uint_t) slpie->NumaNode.GroupMask.Group, (lfds710_pal_uint_t) slpie->NumaNode.GroupMask.Mask );

        // TRD : now all all LPs from this NUMA node to tree
        logical_processor_number = 0;
        bitmask = slpie->NumaNode.GroupMask.Mask;

        while( bitmask != 0 )
        {
          if( bitmask & 0x1 )
            libbenchmark_misc_pal_helper_add_logical_processor_node_to_topology_tree( ts, ms, logical_processor_number, RAISED, (slpie->NumaNode.GroupMask.Group) );

          bitmask >>= 1;
          logical_processor_number++;
        }
      }

      if( slpie->Relationship == RelationGroup )
      {
        // TRD : we don't care about this - actually, we do care, we really REALLY hate this
      }

      if( slpie->Relationship == RelationProcessorPackage )
      {
        libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
        for( subloop = 0 ; subloop < slpie->Processor.GroupCount ; subloop++ )
          internal_populate_logical_processor_array_from_bitmask( ms, tns, (lfds710_pal_uint_t) slpie->Processor.GroupMask[subloop].Group, (lfds710_pal_uint_t) slpie->Processor.GroupMask[subloop].Mask );
        libbenchmark_misc_pal_helper_add_socket_node_to_topology_tree( ts, tns );
      }

      if( slpie->Relationship == RelationProcessorCore )
      {
        libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
        for( subloop = 0 ; subloop < slpie->Processor.GroupCount ; subloop++ )
          internal_populate_logical_processor_array_from_bitmask( ms, tns, (lfds710_pal_uint_t) slpie->Processor.GroupMask[subloop].Group, (lfds710_pal_uint_t) slpie->Processor.GroupMask[subloop].Mask );
        libbenchmark_misc_pal_helper_add_physical_processor_node_to_topology_tree( ts, tns );
      }

      /*
      if( slpie->Relationship == RelationCache )
      {
        if( slpie->Cache.Type == CacheUnified or slpie->Cache.Type == CacheInstruction or slpie->Cache.Type == CacheData )
        {
          libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
          internal_populate_logical_processor_array_from_bitmask( ms, tns, (lfds710_pal_uint_t) slpie->ProcessorMask );
          libbenchmark_misc_pal_helper_add_cache_node_to_topology_tree( ts, tns, (lfds710_pal_uint_t) slpie->Cache.Level, processor_cache_type_to_libbenchmark_topology_node_cache_type[slpie->Cache.Type] );
        }
      }
      */
    }

    /* TRD : now finally insert the built-up NUMA and cache records
             we call cleanup() on the accumulator tree - it's safe to re-use the nodes as they're emitted to the cleanup function
             so we then throw them into the topology_state tree
    */

    lfds710_btree_au_cleanup( &nna_tree_state, nna_cleanup );

    return rv;
  }

  /****************************************************************************/
  static int numa_node_id_to_numa_node_id_compare_function( void const *new_key, void const *existing_key )
  {
    int
      cr = 0;

    ULONG
      numa_node_id_existing,
      numa_node_id_new;

    LFDS710_PAL_ASSERT( new_key != NULL );
    LFDS710_PAL_ASSERT( existing_key != NULL );

    numa_node_id_new = *(ULONG *) new_key;
    numa_node_id_existing = *(ULONG *) existing_key;

    if( numa_node_id_new < numa_node_id_existing )
      cr = -1;

    if( numa_node_id_new > numa_node_id_existing )
      cr = 1;

    return cr;
  }

  /****************************************************************************/
  static void nna_cleanup( struct lfds710_btree_au_state *baus, struct lfds710_btree_au_element *baue )
  {
    ULONG
      *numa_node_id;

    struct libbenchmark_topology_node_state
      *tns;

    struct libbenchmark_topology_state
      *ts;

    LFDS710_PAL_ASSERT( baus != NULL );
    LFDS710_PAL_ASSERT( baue != NULL );

    ts = LFDS710_BTREE_AU_GET_USER_STATE_FROM_STATE( *baus );
    numa_node_id = LFDS710_BTREE_AU_GET_KEY_FROM_ELEMENT( *baue );
    tns = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *baue );

    libbenchmark_misc_pal_helper_add_numa_node_to_topology_tree( ts, tns, (lfds710_pal_uint_t) *numa_node_id );

    return;
  }

  /****************************************************************************/
  static void internal_populate_logical_processor_array_from_bitmask( struct libshared_memory_state *ms,
                                                                      struct libbenchmark_topology_node_state *tns,
                                                                      lfds710_pal_uint_t windows_processor_group_number,
                                                                      lfds710_pal_uint_t bitmask )
  {
    lfds710_pal_uint_t
      logical_processor_number = 0;

    LFDS710_PAL_ASSERT( ms != NULL );
    LFDS710_PAL_ASSERT( tns != NULL );
    // TRD : windows_processor_group_number can be any value in its range
    // TRD : bitmask can be any value in its range

    /* TRD : iterate over the bits in the bitmask
             each is a LP number
             add every LP to *tns
    */

    while( bitmask != 0 )
    {
      if( bitmask & 0x1 )
        libbenchmark_misc_pal_helper_add_logical_processor_to_topology_node( tns, ms, logical_processor_number, RAISED, windows_processor_group_number );

      bitmask >>= 1;
      logical_processor_number++;
    }

    return;
  }

#endif





/****************************************************************************/
#if( defined __linux__ && !defined KERNEL_MODE && defined __STDC__ && __STDC_HOSTED__ == 1 )

  #ifdef LIBBENCHMARK_PAL_POPULATE_TOPOLOGY
    #error More than one porting abstraction layer matches current platform in "libbenchmark_porting_abstraction_layer_populate_topology.c".
  #endif

  #define LIBBENCHMARK_PAL_POPULATE_TOPOLOGY

  static void internal_populate_logical_processor_array_from_path_to_csv_hex( struct libshared_memory_state *ms,
                                                                              struct libbenchmark_topology_node_state *tns,
                                                                              char *path_to_csv_hex );
  static int internal_verify_paths( lfds710_pal_uint_t number_paths, ... );
  static void internal_read_string_from_path( char *path, char *string );

  /****************************************************************************/
  int libbenchmark_porting_abstraction_layer_populate_topology( struct libbenchmark_topology_state *ts,
                                                                struct libshared_memory_state *ms )
  {
    char
      numa_node_path[128],
      thread_siblings_path[128],
      core_siblings_path[128],
      cache_level_path[128],
      cache_type_path[128],
      shared_cpu_map_path[128],
      cache_level_string[16],
      cache_type_string[16];

    int
      rv = 1,
      cache_type_string_to_type_enum_lookup[NUMBER_UPPERCASE_LETTERS_IN_LATIN_ALPHABET] = 
      {
        -1, -1, -1, LIBBENCHMARK_TOPOLOGY_NODE_CACHE_TYPE_DATA, -1, -1, -1, -1, LIBBENCHMARK_TOPOLOGY_NODE_CACHE_TYPE_INSTRUCTION, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, LIBBENCHMARK_TOPOLOGY_NODE_CACHE_TYPE_UNIFIED, -1, -1, -1, -1
      };

    int long long unsigned
      level_temp;

    lfds710_pal_uint_t
      numa_node = 0,
      cpu_number = 0,
      index_number,
      level,
      type;

    struct libbenchmark_topology_iterate_state
      tis;

    struct libbenchmark_topology_node_state
      *tns,
      *tns_lp;

    LFDS710_PAL_ASSERT( ts != NULL );
    LFDS710_PAL_ASSERT( ms != NULL );

    sprintf( numa_node_path, "/sys/devices/system/node/node%llu/cpumap", (int long long unsigned) numa_node );

    while( internal_verify_paths(1, numa_node_path) )
    {
      libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
      internal_populate_logical_processor_array_from_path_to_csv_hex( ms, tns, numa_node_path );
      libbenchmark_misc_pal_helper_add_numa_node_to_topology_tree( ts, tns, (lfds710_pal_uint_t) numa_node );
      sprintf( numa_node_path, "/sys/devices/system/node/node%llu/cpumap", (int long long unsigned) (++numa_node) );
    }

    sprintf( thread_siblings_path, "/sys/devices/system/cpu/cpu%llu/topology/thread_siblings", (int long long unsigned) cpu_number );
    sprintf( core_siblings_path, "/sys/devices/system/cpu/cpu%llu/topology/core_siblings", (int long long unsigned) cpu_number );

    while( internal_verify_paths(2, core_siblings_path, thread_siblings_path) )
    {
      libbenchmark_misc_pal_helper_add_logical_processor_node_to_topology_tree( ts, ms, cpu_number, LOWERED, 0 );

      libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
      internal_populate_logical_processor_array_from_path_to_csv_hex( ms, tns, thread_siblings_path );
      libbenchmark_misc_pal_helper_add_physical_processor_node_to_topology_tree( ts, tns );

      libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
      internal_populate_logical_processor_array_from_path_to_csv_hex( ms, tns, core_siblings_path );
      libbenchmark_misc_pal_helper_add_socket_node_to_topology_tree( ts, tns );

      index_number = 0;

      sprintf( cache_level_path, "/sys/devices/system/cpu/cpu%llu/cache/index%llu/level", (int long long unsigned) cpu_number, (int long long unsigned) index_number );
      sprintf( cache_type_path, "/sys/devices/system/cpu/cpu%llu/cache/index%llu/type", (int long long unsigned) cpu_number, (int long long unsigned) index_number );
      sprintf( shared_cpu_map_path, "/sys/devices/system/cpu/cpu%llu/cache/index%llu/shared_cpu_map", (int long long unsigned) cpu_number, (int long long unsigned) index_number );

      while( internal_verify_paths(3, cache_level_path, cache_type_path, shared_cpu_map_path) )
      {
        internal_read_string_from_path( cache_level_path, cache_level_string );
        sscanf( cache_level_string, "%llx", &level_temp );
        level = (lfds710_pal_uint_t) level_temp;

        internal_read_string_from_path( cache_type_path, cache_type_string );
        type = cache_type_string_to_type_enum_lookup[(int)(*cache_type_string - 'A')];

        libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
        internal_populate_logical_processor_array_from_path_to_csv_hex( ms, tns, shared_cpu_map_path );
        libbenchmark_misc_pal_helper_add_cache_node_to_topology_tree( ts, tns, level, type );

        index_number++;

        sprintf( cache_level_path, "/sys/devices/system/cpu/cpu%llu/cache/index%llu/level", (int long long unsigned) cpu_number, (int long long unsigned) index_number );
        sprintf( cache_type_path, "/sys/devices/system/cpu/cpu%llu/cache/index%llu/type", (int long long unsigned) cpu_number, (int long long unsigned) index_number );
        sprintf( shared_cpu_map_path, "/sys/devices/system/cpu/cpu%llu/cache/index%llu/shared_cpu_map", (int long long unsigned) cpu_number, (int long long unsigned) index_number );
      }

      cpu_number++;

      sprintf( thread_siblings_path, "/sys/devices/system/cpu/cpu%llu/topology/thread_siblings", (int long long unsigned) cpu_number );
      sprintf( core_siblings_path, "/sys/devices/system/cpu/cpu%llu/topology/core_siblings", (int long long unsigned) cpu_number );
    }

    // TRD : now make and populate the notional system node
    libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );
    libbenchmark_topology_iterate_init( &tis, LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR );
    while( libbenchmark_topology_iterate(ts, &tis, &tns_lp) )
      libbenchmark_misc_pal_helper_add_logical_processor_to_topology_node( tns, ms, LIBBENCHMARK_TOPOLOGY_NODE_GET_LOGICAL_PROCESSOR_NUMBER(*tns_lp), LOWERED, 0 );
    libbenchmark_misc_pal_helper_add_system_node_to_topology_tree( ts, tns );

    return rv;
  }

  /****************************************************************************/
  void libbenchmark_porting_abstraction_layer_topology_node_cleanup( struct libbenchmark_topology_node_state *tns )
  {
    LFDS710_PAL_ASSERT( tns != NULL );

    lfds710_list_aso_cleanup( &tns->logical_processor_children, NULL );

    return;
  }

  /****************************************************************************/
  static void internal_populate_logical_processor_array_from_path_to_csv_hex( struct libshared_memory_state *ms,
                                                                              struct libbenchmark_topology_node_state *tns,
                                                                              char *path_to_csv_hex )
  {
    char
      diskbuffer[BUFSIZ],
      string[1024];

    FILE
      *diskfile;

    int
      loop;

    int unsigned
      logical_processor_foursome,
      logical_processor_number = 0,
      subloop;

    lfds710_pal_uint_t
      length = 0;

    LFDS710_PAL_ASSERT( ms != NULL );
    LFDS710_PAL_ASSERT( tns != NULL );
    LFDS710_PAL_ASSERT( path_to_csv_hex != NULL );

    /* TRD : we're passed a format string and args, which comprise the path
             form up the string, open the file, read the string, parse the string
             the string consists of 32-bit bitmasks in hex separated by commas
             no leading or trailing commas
    */

    diskfile = fopen( path_to_csv_hex, "r" );
    setbuf( diskfile, diskbuffer );
    fgets( string, 1024, diskfile );
    fclose( diskfile );

    while( string[length++] != '\0' );

    length -= 2;

    for( loop = ((int)length)-1 ; loop > -1 ; loop-- )
    {
      if( string[loop] == ',' )
        continue;

      sscanf( &string[loop], "%1x", &logical_processor_foursome );

      for( subloop = 0 ; subloop < 4 ; subloop++ )
        if( ( (logical_processor_foursome >> subloop) & 0x1 ) == 0x1 )
          libbenchmark_misc_pal_helper_add_logical_processor_to_topology_node( tns, ms, logical_processor_number + subloop, LOWERED, 0 );

      logical_processor_number += 4;
    }

    return;
  }

  /****************************************************************************/
  static int internal_verify_paths( lfds710_pal_uint_t number_paths, ... )
  {
    FILE
      *diskfile;

    int
      rv = 1;

    lfds710_pal_uint_t
      count = 0;

    va_list
      va;

    // TRD : number_paths can be any value in its range

    va_start( va, number_paths );

    while( rv == 1 and count++ < number_paths )
      if( NULL == (diskfile = fopen(va_arg(va,char *), "r")) )
        rv = 0;
      else
        fclose( diskfile );

    va_end( va );

    return rv;
  }

  /****************************************************************************/
  static void internal_read_string_from_path( char *path, char *string )
  {
    char
      diskbuffer[BUFSIZ];

    FILE
      *diskfile;

    LFDS710_PAL_ASSERT( path != NULL );
    LFDS710_PAL_ASSERT( string != NULL );

    diskfile = fopen( path, "r" );
    setbuf( diskfile, diskbuffer );
    fscanf( diskfile, "%s", string );
    fclose( diskfile );

    return;
  }

#endif





/****************************************************************************/
#if( !defined LIBBENCHMARK_PAL_POPULATE_TOPOLOGY )

  #error No matching porting abstraction layer in "libbenchmark_porting_abstraction_layer_populate_topology.c".

#endif


