/***** includes *****/
#include "libbenchmark_topology_internal.h"

/***** defines *****/
#define NUMBER_KEY_LINES 8

/***** structs *****/
struct line
{
  char
    *string;

  enum libbenchmark_topology_node_type
    type;

  struct lfds710_list_asu_element
    lasue;

  union libbenchmark_topology_node_extended_info
    extended_node_info;
};

/***** private prototypes *****/
static int key_compare_function( void const *new_key, void const *existing_key );
static void strcat_spaces( char *string, lfds710_pal_uint_t number_width );





/****************************************************************************/
char *libbenchmark_topology_generate_string( struct libbenchmark_topology_state *ts, struct libshared_memory_state *ms, enum libbenchmark_topology_string_format format )
{
  char const
    cache_type_enum_to_string_lookup[LIBBENCHMARK_TOPOLOGY_NODE_CACHE_TYPE_COUNT] = 
    {
      'D', 'I', 'U'
    },
    *const key_strings[NUMBER_KEY_LINES] = 
    {
      "R   = Notional system root     ",
      "N   = NUMA node                ",
      "S   = Socket (physical package)",
      "LnU = Level n unified cache    ",
      "LnD = Level n data cache       ",
      "LnI = Level n instruction cache",
      "P   = Physical core            ",
      "nnn = Logical core             ",
    },
    *const empty_key_string = "                               ";

  char
    *topology_string;

  int
    final_length = 0,
    half_space_width,
    loop,
    space_width;

  lfds710_pal_uint_t
    key_line,
    lp_count,
    number_topology_lines,
    number_key_and_topology_lines;

  struct lfds710_btree_au_element
    *baue;

  struct line
    *line;

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct lfds710_list_asu_state
    lines;

  struct libbenchmark_topology_node_state
    *tns;

  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  // TRD : format can be any value in its range

  lfds710_list_asu_init_valid_on_current_logical_core( &lines, NULL );

  baue = NULL;

  while( lfds710_btree_au_get_by_absolute_position_and_then_by_relative_position(&ts->topology_tree, &baue, LFDS710_BTREE_AU_ABSOLUTE_POSITION_LARGEST_IN_TREE, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_SMALLER_ELEMENT_IN_ENTIRE_TREE) )
  {
    tns = LFDS710_BTREE_AU_GET_KEY_FROM_ELEMENT( *baue );

    /* TRD : look for this node type in the list of lines
             if it's not there, add it to the end
             if it is there, use it
    */

    if( 0 == lfds710_list_asu_get_by_key(&lines, key_compare_function, tns, &lasue) )
    {
      line = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct line), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      line->type = tns->type;
      line->extended_node_info = tns->extended_node_info;
      // TRD : +2 for trailing space and for trailing NULL
      line->string = libshared_memory_alloc_from_most_free_space_node( ms, ts->line_width+2, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      *line->string = '\0';
      LFDS710_LIST_ASU_SET_KEY_IN_ELEMENT( line->lasue, line );
      LFDS710_LIST_ASU_SET_VALUE_IN_ELEMENT( line->lasue, line );
      lfds710_list_asu_insert_at_end( &lines, &line->lasue );
    }
    else
      line = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    switch( tns->type )
    {
      case LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR:
        // libshared_ansi_strcat( line->string, " L " );
        libshared_ansi_strcat_number_with_leading_zeros( line->string, LIBBENCHMARK_TOPOLOGY_NODE_GET_LOGICAL_PROCESSOR_NUMBER(*tns), 3 );
      break;

      case LIBBENCHMARK_TOPOLOGY_NODE_TYPE_CACHE:
      {
        lp_count = count_of_logical_processors_below_node( baue );
        space_width = (int) ( lp_count * 3 + lp_count - 3 );
        half_space_width = space_width / 2;

        strcat_spaces( line->string, half_space_width );
        libshared_ansi_strcat( line->string, "L" );
        libshared_ansi_strcat_number( line->string, tns->extended_node_info.cache.level );
        libshared_ansi_strcat_char( line->string, cache_type_enum_to_string_lookup[tns->extended_node_info.cache.type] );
        strcat_spaces( line->string, half_space_width );
      }
      break;

      case LIBBENCHMARK_TOPOLOGY_NODE_TYPE_PHYSICAL_PROCESSOR:
        lp_count = count_of_logical_processors_below_node( baue );
        space_width = (int) ( lp_count * 3 + lp_count - 1 );
        half_space_width = space_width / 2;
        strcat_spaces( line->string, half_space_width );
        libshared_ansi_strcat( line->string, "P" );
        strcat_spaces( line->string, half_space_width );
      break;

      case LIBBENCHMARK_TOPOLOGY_NODE_TYPE_SOCKET:
        lp_count = count_of_logical_processors_below_node( baue );
        space_width = (int) ( lp_count * 3 + lp_count - 1 );
        half_space_width = space_width / 2;
        strcat_spaces( line->string, half_space_width );
        libshared_ansi_strcat( line->string, "S" );
        strcat_spaces( line->string, half_space_width );
      break;

      case LIBBENCHMARK_TOPOLOGY_NODE_TYPE_NUMA:
        lp_count = count_of_logical_processors_below_node( baue );
        space_width = (int) ( lp_count * 3 + lp_count - 1 );
        half_space_width = space_width / 2;
        strcat_spaces( line->string, half_space_width );
        libshared_ansi_strcat( line->string, "N" );
        strcat_spaces( line->string, half_space_width );
      break;

      case LIBBENCHMARK_TOPOLOGY_NODE_TYPE_SYSTEM:
        /* TRD : count the number of LPs below this node
                 assume for now each has caches so it three letters side
                 compute the space_width and print R in the middle
        */

        lp_count = count_of_logical_processors_below_node( baue );
        space_width = (int) ( lp_count * 3 + lp_count - 1 );
        half_space_width = space_width / 2;
        strcat_spaces( line->string, half_space_width );
        libshared_ansi_strcat( line->string, "R" );
        strcat_spaces( line->string, half_space_width );
      break;
    }

    libshared_ansi_strcat( line->string, " " );
  }

  /* TRD : so, we have the topology
           but we also want to print the key on the same lines
           the topology may have more or less lines than the key
           if we run out of topology lines, we need to print white spaces to keep the justification correct
           same for running out of key lines

           first we compute the length of the final string, so we can allocate the topology string
           then we actually form up the string
  */

  lfds710_list_asu_query( &lines, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void *) &number_topology_lines );

  if( number_topology_lines < NUMBER_KEY_LINES )
    number_key_and_topology_lines = NUMBER_KEY_LINES;
  else
    number_key_and_topology_lines = number_topology_lines;

  // TRD : +1 for one space, +31 for the text, +1 for final newline, +5 for gnuplot stuff ("\\n\\", which we won't need if stdout)
  final_length = (int) ( (ts->line_width + 39) * number_key_and_topology_lines );

  // TRD : and a trailing NULL
  topology_string = libshared_memory_alloc_from_most_free_space_node( ms, final_length+1, 1 );
  *topology_string = '\0';

  // TRD : now all the fun of the faire - time to compose the string

  key_line = 0;

  lasue = LFDS710_LIST_ASU_GET_START( lines );

  while( lasue != NULL or key_line < NUMBER_KEY_LINES )
  {
    // TRD : copy in a blank topology line
    if( lasue == NULL )
      for( loop = 0 ; loop < ts->line_width+1 ; loop++ )
        libshared_ansi_strcat( topology_string, " " );

    if( lasue != NULL )
    {
      line = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
      libshared_ansi_strcat( topology_string, line->string );
      lasue = LFDS710_LIST_ASU_GET_NEXT( *lasue );
    }

    // TRD : copy in a blank key line
    if( key_line == NUMBER_KEY_LINES )
      libshared_ansi_strcat( topology_string, empty_key_string );

    if( key_line < NUMBER_KEY_LINES )
      libshared_ansi_strcat( topology_string, key_strings[key_line++] );

    switch( format )
    {
      case LIBBENCHMARK_TOPOLOGY_STRING_FORMAT_STDOUT:
        libshared_ansi_strcat( topology_string, "\n" );
      break;

      case LIBBENCHMARK_TOPOLOGY_STRING_FORMAT_GNUPLOT:
        libshared_ansi_strcat( topology_string, "\\n\\\n" );
      break;
    }
  }

  return topology_string;
}





/****************************************************************************/
char *libbenchmark_topology_generate_lpset_string( struct libbenchmark_topology_state *ts, struct libshared_memory_state *ms, struct lfds710_list_aso_state *lpset )
{
  char
    *lpset_string = NULL;

  int
    loop;

  struct libbenchmark_topology_lp_printing_offset
    *tlpo;

  struct libbenchmark_topology_node_state
    *tns;

  struct lfds710_btree_au_element
    *baue;

  struct lfds710_list_aso_element
    *lasoe = NULL;

  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( lpset != NULL );

  lpset_string = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(char) * (ts->line_width+1), sizeof(char) );

  for( loop = 0 ; loop < ts->line_width ; loop++ )
    lpset_string[loop] = ' ';
  lpset_string[loop] = '\0';

  while( LFDS710_LIST_ASO_GET_START_AND_THEN_NEXT(*lpset,lasoe) )
  {
    tns = LFDS710_LIST_ASO_GET_VALUE_FROM_ELEMENT( *lasoe );

    lfds710_btree_au_get_by_key( &ts->lp_printing_offset_lookup_tree, libbenchmark_topology_compare_node_against_lp_printing_offset_function, tns, &baue );
    tlpo = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *baue );

    lpset_string[tlpo->offset+1] = '1';
  }

  return lpset_string;
}





/****************************************************************************/
lfds710_pal_uint_t count_of_logical_processors_below_node( struct lfds710_btree_au_element *baue )
{
  lfds710_pal_uint_t
    lp_count = 0;

  struct libbenchmark_topology_node_state
    *root_node,
    *tns;

  LFDS710_PAL_ASSERT( baue != NULL );

  tns = LFDS710_BTREE_AU_GET_KEY_FROM_ELEMENT( *baue );
  root_node = tns;

  while( lfds710_btree_au_get_by_relative_position(&baue, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_SMALLER_ELEMENT_IN_ENTIRE_TREE) )
  {
    tns = LFDS710_BTREE_AU_GET_KEY_FROM_ELEMENT( *baue );

    if( tns->type == root_node->type and tns->type != LIBBENCHMARK_TOPOLOGY_NODE_TYPE_CACHE )
      break;

    if( tns->type == root_node->type and tns->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_CACHE and tns->extended_node_info.cache.type == root_node->extended_node_info.cache.type and tns->extended_node_info.cache.level == root_node->extended_node_info.cache.level )
      break;

    if( tns->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR )
      lp_count++;
  }

  return lp_count;
}





/****************************************************************************/
static int key_compare_function( void const *new_key, void const *existing_key )
{
  struct libbenchmark_topology_node_state
    *tns;

  struct line
    *line;

  LFDS710_PAL_ASSERT( new_key != NULL );
  LFDS710_PAL_ASSERT( existing_key != NULL );

  tns = (struct libbenchmark_topology_node_state *) new_key;
  line = (struct line *) existing_key;

  if( tns->type > line->type )
    return 1;

  if( tns->type < line->type )
    return -1;

  if( tns->type == line->type )
    if( tns->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_CACHE and line->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_CACHE )
    {
      if( tns->extended_node_info.cache.level > line->extended_node_info.cache.level )
        return 1;

      if( tns->extended_node_info.cache.level < line->extended_node_info.cache.level )
        return -1;

      if( tns->extended_node_info.cache.level == line->extended_node_info.cache.level )
      {
        if( tns->extended_node_info.cache.type > line->extended_node_info.cache.type )
          return 1;

        if( tns->extended_node_info.cache.type < line->extended_node_info.cache.type )
          return -1;

        if( tns->extended_node_info.cache.type == line->extended_node_info.cache.type )
          return 0;
      }
    }

  return 0;
}





/****************************************************************************/
static void strcat_spaces( char *string, lfds710_pal_uint_t number_width )
{
  lfds710_pal_uint_t
    loop;

  LFDS710_PAL_ASSERT( string != NULL );
  // TRD : number_width can be any value in its range

  for( loop = 0 ; loop < number_width ; loop++ )
    libshared_ansi_strcat( string, " " );

  return;
}


