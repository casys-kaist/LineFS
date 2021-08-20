/***** defines *****/
#define LIBBENCHMARK_GNUPLOT_OPTIONS_INIT( gpo )                               (gpo).y_axis_scale_type = LIBBENCHMARK_GNUPLOT_Y_AXIS_SCALE_TYPE_LINEAR, (gpo).width_in_pixels_set_flag = LOWERED, (gpo).width_in_pixels_set_flag = LOWERED
#define LIBBENCHMARK_GNUPLOT_OPTIONS_SET_Y_AXIS_SCALE_TYPE_LOGARITHMIC( gpo )  (gpo).y_axis_scale_type = LIBBENCHMARK_GNUPLOT_Y_AXIS_SCALE_TYPE_LOGARITHMIC;
#define LIBBENCHMARK_GNUPLOT_OPTIONS_SET_WIDTH_IN_PIXELS( gpo, wip )           (gpo).width_in_pixels = wip, (gpo).width_in_pixels_set_flag = RAISED
#define LIBBENCHMARK_GNUPLOT_OPTIONS_SET_HEIGHT_IN_PIXELS( gpo, wip )          (gpo).height_in_pixels = wip, (gpo).height_in_pixels_set_flag = RAISED

/***** enums *****/
enum libbenchmark_gnuplot_y_axis_scale_type
{
  LIBBENCHMARK_GNUPLOT_Y_AXIS_SCALE_TYPE_LINEAR,
  LIBBENCHMARK_GNUPLOT_Y_AXIS_SCALE_TYPE_LOGARITHMIC
};

/***** structs *****/
struct libbenchmark_gnuplot_options
{
  enum flag
    width_in_pixels_set_flag,
    height_in_pixels_set_flag;

  enum libbenchmark_gnuplot_y_axis_scale_type
    y_axis_scale_type;

  lfds710_pal_uint_t
    width_in_pixels,
    height_in_pixels;
};

/***** public prototypes *****/

