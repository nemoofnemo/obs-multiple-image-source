#pragma once
#include <obs-module.h>
#include <util/threading.h>
#include <util/platform.h>
#include <util/darray.h>
#include <util/dstr.h>
#include <graphics/graphics.h>
#include <graphics/vec4.h>

//this obs plugin is based on obs slideimage.
//yuanyi 2017/08/18

#define do_log(level, format, ...) \
	blog(level, "[multiple_image_source: '%s'] " format, \
			obs_source_get_name(mis->source), ##__VA_ARGS__)
#define debug(format, ...) \
	blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...) \
	blog(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) \
	blog(LOG_WARNING, format, ##__VA_ARGS__)


#define S_TR_SPEED                     "transition_speed"
#define S_SLIDE_TIME                   "slide_time"
#define S_TRANSITION                   "transition"
#define S_LOOP                         "loop"
#define S_HIDE                         "hide"
#define S_FILES                        "files"
#define S_BEHAVIOR                     "playback_behavior"
#define S_BEHAVIOR_STOP_RESTART        "stop_restart"
#define S_BEHAVIOR_PAUSE_UNPAUSE       "pause_unpause"
#define S_BEHAVIOR_ALWAYS_PLAY         "always_play"
#define S_MODE                         "slide_mode"
#define S_MODE_AUTO                    "mode_auto"
#define S_MODE_MANUAL                  "mode_manual"
#define S_DIRECTION					   "direction"
#define S_DIRECTION_PREV			   "prev"
#define S_DIRECTION_NEXT			   "next"

#define TR_CUT                         "cut"
#define TR_FADE                        "fade"
#define TR_SWIPE                       "swipe"
#define TR_SLIDE                       "slide"

#ifndef da_get
#define da_get(v, idx, type) ((type*)darray_item(sizeof(type), &v.da, idx))
#endif

/* ------------------------------------------------------------------------- */

struct image_file_data {
	char *path;
	//obs_source_t *source;
};

enum behavior {
	BEHAVIOR_STOP_RESTART,
	BEHAVIOR_PAUSE_UNPAUSE,
	BEHAVIOR_ALWAYS_PLAY,
};

struct mis_pixel_arg{
	int x;
	int y;
	uint32_t value;
};
typedef struct mis_pixel_arg mis_pixel_arg_t;

struct mis_rect{
	int x;
	int y;
	int width;
	int height;
};
typedef struct mis_rect mis_rect_t;

enum mis_shape { NOTHING, CIRCLE_POINT, LINE, POLYLINE, RECTANGLE };
typedef enum mis_shape mis_shape_t;

struct mis_circle_point{
	int x;
	int y;
	int radius;
	uint32_t rgba;
};
typedef struct mis_circle_point mis_circle_point_t;

struct mis_line{
	int x1;
	int y1;
	int x2;
	int y2;
	int width;
	uint32_t rgba;
	gs_vertbuffer_t * buf;
};
typedef struct mis_line mis_line_t;

typedef struct mis_polyline_node {
	int x;
	int y;
	gs_vertbuffer_t * buf;
} mis_polyline_node_t;

struct mis_polyline{
	DARRAY(mis_polyline_node_t) node_arr;
	int width;
	uint32_t rgba;
};
typedef struct mis_polyline mis_polyline_t;

struct mis_rectangle{
	int x;
	int y;
	int width;
	int height;
	int line_width;
	uint32_t rgba;
	gs_vertbuffer_t *buf_arr[4];
};
typedef struct mis_rectangle mis_rectangle_t;

struct mis_node{
	mis_shape_t shape;
	mis_rect_t rect;
	void * data;
};
typedef struct mis_node mis_node_t;

struct mis_shape_array{
	DARRAY(mis_node_t) shape_array;
};
typedef struct mis_shape_array mis_shape_array_t;

struct mis_pages {
	DARRAY(mis_shape_array_t) pages;
	int cur_page;
	mis_shape_t paint_status;
};
typedef struct mis_pages mis_pages_t;

struct multiple_image_source {
	obs_source_t *source;

	bool loop;
	bool restart_on_activate;
	bool pause_on_deactivate;
	bool restart;
	bool manual;
	bool hide;
	bool use_cut;
	bool paused;
	bool stop;
	float slide_time;
	uint32_t tr_speed;
	const char *tr_name;
	obs_source_t *transition;
	obs_source_t *prev_source;

	float elapsed;
	size_t cur_item;

	uint32_t cx;
	uint32_t cy;

	pthread_mutex_t mutex;
	DARRAY(struct image_file_data) files;

	enum behavior behavior;

	uint32_t width;
	uint32_t height;
	mis_pages_t pages;
};
typedef struct multiple_image_source multiple_image_source_t;

static uint32_t mis_set_rgba(unsigned char r, unsigned char g, unsigned char b, unsigned char a){
	uint32_t ret = 0;
	uint32_t _r = r;
	uint32_t _g = g;
	uint32_t _b = b;
	uint32_t _a = a;
	ret += _a << 24;
	ret += _b << 16;
	ret += _g << 8;
	ret += _r;
	return ret;
}

#define mis_get_rgba_r(rgba) ((uint32_t)(rgba)&(uint32_t)0xff)
#define mis_get_rgba_g(rgba) (((uint32_t)(rgba)&(uint32_t)0xff00) >> 8)
#define mis_get_rgba_b(rgba) (((uint32_t)(rgba)&(uint32_t)0xff0000) >> 16)
#define mis_get_rgba_a(rgba) (((uint32_t)(rgba)&(uint32_t)0xff000000) >> 24)

static void mis_swapf(float * a, float * b){
	float temp;
	temp = *a;
	*a = *b;
	*b = temp;
}

static bool mis_update_rect(mis_rect_t * rect, int x, int y){
	if (x < rect->x){
		rect->width += rect->x - x;
		rect->x = x;
	}
	else if (x >= rect->x + rect->width){
		rect->width += x - rect->x;
	}

	if (y < rect->y){
		rect->height += rect->y - y;
		rect->y = y;
	}
	else if (y >= rect->y + rect->height){
		rect->height += y - rect->y;
	}
}

/* ------------------------------------------------------------------------- */

//line
static void mis_setup_line(mis_line_t * line);

static void mis_destroy_line(mis_line_t * line);

static void mis_update_line(mis_line_t * line);

static void mis_paint_line(mis_line_t * line);


//rectangele
static void mis_setup_rectangle(mis_rectangle_t * rect);

static void mis_destroy_rectangle(mis_rectangle_t * rect);

static void mis_update_rectangle(mis_rectangle_t * rect);

static void mis_paint_rectangle(mis_rectangle_t * rect);


//polyline
static void mis_init_polyline(mis_polyline_t * polyline);

static void mis_push_back_polyline_node(mis_polyline_t * polyline, int x, int y);

static void mis_setup_polyline(mis_polyline_t * polyline);

static void mis_destroy_polyline(mis_polyline_t * polyline);

static void mis_update_polyline(mis_polyline_t * polyline);

static void mis_paint_polyline(mis_polyline_t * polyline);


//shape array
static void * mis_create_shape(mis_shape_t shape);

static void mis_delete_shape(mis_shape_t shape, void * data);

static void mis_init_shape_array(mis_shape_array_t * arr);

static void mis_destroy_shape_array(mis_shape_array_t * arr);
//shape must be setup first.
static void mis_push_shape_array(mis_shape_array_t * arr, mis_shape_t shape, void * data);

static mis_node_t * mis_get_from_array(mis_shape_array_t * arr, size_t idx);

static mis_node_t * mis_get_last_from_array(mis_shape_array_t * arr);

static void mis_paint_shape_array(mis_shape_array_t * arr);


//pages
static void mis_init_pages(mis_pages_t * pages);

static void mis_destroy_pages(mis_pages_t * pages);
//create and push a new page to mis_pages.
static void mis_push_new_page(mis_pages_t * pages);

static mis_shape_array_t * mis_get_page(mis_pages_t * pages, size_t idx);

//static void mis_push_shape_to_page(mis_pages_t * pages, size_t idx, mis_node_t * shape);

//static void mis_remove_shape_from_page();

//static void mis_remove_page(mis_pages_t * pages, size_t idx);

static void mis_pages_prev(mis_pages_t * pages);

static void mis_pages_next(mis_pages_t * pages);

static void mis_paint_pages(mis_pages_t * pages);


/* ------------------------------------------------------------------------- */

static void mis_paint(multiple_image_source_t * mis);

static obs_source_t *get_transition(struct multiple_image_source *mis);

static obs_source_t *create_source_from_file(const char *file);

static void free_files(struct darray *array);

static const char *mis_getname(void *unused);

static void add_file(struct multiple_image_source *mis, struct darray *array, const char *path);

static inline bool item_valid(struct multiple_image_source *mis);

static void do_transition(void *data, bool to_null);

static void mis_update(void *data, obs_data_t *settings);

static void mis_play_pause(void *data);

static void mis_restart(void *data);

static void mis_stop(void *data);

static void mis_next_slide(void *data);

static void mis_previous_slide(void *data);

static void mis_destroy(void *data);

static void *mis_create(obs_data_t *settings, obs_source_t *source);

static void mis_video_render(void *data, gs_effect_t *effect);

static void mis_clear_paint_event(multiple_image_source_t * mis);

static void mis_process_paint_event(multiple_image_source_t * mis, obs_data_t * settings);

static void mis_video_tick(void *data, float seconds);

static inline bool mis_audio_render_(obs_source_t *transition, uint64_t *ts_out,
	struct obs_source_audio_mix *audio_output,
	uint32_t mixers, size_t channels, size_t sample_rate);

static bool mis_audio_render(void *data, uint64_t *ts_out,
	struct obs_source_audio_mix *audio_output,
	uint32_t mixers, size_t channels, size_t sample_rate);

static void mis_enum_sources(void *data, obs_source_enum_proc_t cb, void *param);

static uint32_t mis_width(void *data);

static uint32_t mis_height(void *data);

static void mis_defaults(obs_data_t *settings);

static obs_properties_t *mis_properties(void *data);

static void mis_activate(void *data);

static void mis_deactivate(void *data);