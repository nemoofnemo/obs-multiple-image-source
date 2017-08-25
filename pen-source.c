#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <util/darray.h>
#include <sys/stat.h>
#include <math.h>

//this obs-plugin is created by yuanyi.
//2017/08/21

#define blogex(log_level, format, ...) \
	blog(log_level, "[pen_source: '%s'] " format, \
			obs_source_get_name(context->source), ##__VA_ARGS__)

#define debug(format, ...) \
	blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...) \
	blog(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) \
	blog(LOG_WARNING, format, ##__VA_ARGS__)

struct pen_source {
	obs_source_t * source;
	gs_texture_t * tex;
	char * data;
	uint32_t width;
	uint32_t height;
	uint32_t color_rgba;
	float update_time_elapsed;
	
};
typedef struct pen_source pen_source_t;

//-------------------------paint------------------------//

struct pixel_arg{
	int x;
	int y;
	uint32_t value;
};
typedef struct pixel_arg pixel_arg_t;

struct pen_source_rect{
	int x;
	int y;
	int width;
	int height;
};
typedef struct pen_source_rect pen_source_rect_t;

enum pen_source_shape { NOTHING, CIRCLE_POINT, LINE, POLYLINE, RECTANGLE };
typedef enum pen_source_shape pen_source_shape_t;

struct ps_circle_point{
	int x;
	int y;
	int radius;
	uint32_t rgba;
};
typedef struct ps_circle_point ps_circle_point_t;

struct ps_line{
	int x1;
	int y1;
	int x2;
	int y2;
	int width;
	uint32_t rgba;
};
typedef struct ps_line ps_line_t;

typedef struct ps_polyline_node {
	int x;
	int y;
} ps_polyline_node_t;

struct ps_polyline{
	DARRAY(ps_polyline_node_t) node_arr;
	int width;
	uint32_t rgba;
};
typedef struct ps_polyline ps_polyline_t;

struct ps_rectangle{
	int x;
	int y;
	int width;
	int height;
	int line_width;
	uint32_t rgba;
};
typedef struct ps_rectangle ps_rectangle_t;

struct ps_node{
	pen_source_shape_t shape;
	pen_source_rect_t rect;
	void * data;
};
typedef struct ps_node ps_node_t;

struct ps_shape_array{
	DARRAY(ps_node_t) shape_array;
	pen_source_shape_t paint_status;
};
typedef struct ps_shape_array ps_shape_array_t;

#define ps_enter_paint_context(arr, shape) ((arr)->paint_status = shape)
#define ps_leave_paint_context(arr) ((arr)->paint_status = NOTHING)

static uint32_t set_rgba(unsigned char r, unsigned char g, unsigned char b, unsigned char a){ 
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

static void _set_pixel(char * data, int width, pixel_arg_t * pa){
	*((uint32_t*)data + width * pa->y + pa->x) = pa->value;
}

static void _get_pixel(char * data, int width, pixel_arg_t * pa){
	pa->value = *((uint32_t*)data + width * pa->y + pa->x);
}

static void set_pixel(char * data, int width, int height, pixel_arg_t * pa){
	if (data == NULL || width <= 0 || height <= 0 || pa == NULL){
		return;
	}
	if (pa->x >= width || pa->x < 0){
		return;
	}
	if (pa->y >= height || pa->y < 0){
		return;
	}
	_set_pixel(data, width, pa);
}

static bool get_pixel(char * data, int width, int height, pixel_arg_t * pa){
	if (data == NULL || width <= 0 || height <= 0 || pa == NULL){
		return false;
	}
	if (pa->x >= width || pa->x < 0){
		return false;
	}
	if (pa->y >= height || pa->y < 0){
		return false;
	}
	_get_pixel(data, width, pa);
	return true;
}

static void ps_swapf(float * a, float * b){
	float temp;
	temp = *a;
	*a = *b;
	*b = temp;
}

static void draw_circle_point(pen_source_t * context, ps_circle_point_t * point){

}

static void _draw_line(pen_source_t * context, ps_line_t * line){
	bool is_x_equal = line->x1 == line->x2;
	bool is_y_equal = line->y1 == line->y2;
	pixel_arg_t arg;

	if (is_x_equal == 0 && is_y_equal == 0){
		float k1 = ((float)(line->y1 - line->y2)) / (line->x1 - line->x2);
		float b = line->y2 - k1 * line->x2;
		int x_start;
		int x_end;

		if (line->x1 > line->x2){
			x_start = line->x2;
			x_end = line->x1;
		}
		else{
			x_start = line->x1;
			x_end = line->x2;
		}
		
		arg.value = line->rgba;
		for (int i = x_start; i <= x_end; ++i){
			arg.x = i;
			arg.y = k1 * i + b;
			set_pixel(context->data, context->width, context->height, &arg);
			arg.x++;
			set_pixel(context->data, context->width, context->height, &arg);
			arg.x--;
			arg.y++;
			set_pixel(context->data, context->width, context->height, &arg);
		}
		return;
	}

	if (is_x_equal && is_y_equal){
		arg.x = line->x1;
		arg.y = line->y1;
		arg.value = line->rgba;
		set_pixel(context->data, context->width, context->height, &arg);
	}
	else if (is_x_equal){
		int y_start;
		int y_end;
		if (line->y1 > line->y2){
			y_start = line->y2;
			y_end = line->y1;
		}
		else{
			y_start = line->y1;
			y_end = line->y2;
		}
		
		arg.x = line->x1;
		arg.value = line->rgba;
		for (int i = y_start; i <= y_end; ++i){
			arg.y = i;
			set_pixel(context->data, context->width, context->height, &arg);
		}
	}
	else if (is_y_equal){
		int x_start;
		int x_end;
		if (line->x1 > line->x2){
			x_start = line->x2;
			x_end = line->x1;
		}
		else{
			x_start = line->x1;
			x_end = line->x2;
		}
		arg.y = line->y1;
		arg.value = line->rgba;
		for (int i = x_start; i <= x_end; ++i){
			arg.x = i;
			set_pixel(context->data, context->width, context->height, &arg);
		}
	}
}

static void draw_line(pen_source_t * context, ps_line_t * line){
	if (context == NULL || line == NULL)
		return;
	if (line->width <= 0)
		return;
	
	bool is_x_equal = line->x1 == line->x2;
	bool is_y_equal = line->y1 == line->y2;

	if (is_x_equal == 0 && is_y_equal == 0){
		float k1 = ((float)(line->y1 - line->y2)) / (line->x1 - line->x2);
		float k2 = - 1 / k1;
		float b = line->y2 - k1 * line->x2;
		float b2;
		int x_start;
		int x_end;
		int y_start;
		int y_end;

		if (line->x1 > line->x2){
			x_start = line->x2;
			x_end = line->x1;
			y_start = line->y2;
			y_end = line->y1;
			b2 = line->y2 - k2 * x_start;
		}
		else{
			x_start = line->x1;
			x_end = line->x2;
			y_start = line->y1;
			y_end = line->y2;
			b2 = line->y1 - k2 * x_start;
		}
		int x_offset = x_end - x_start;
		int y_offset = y_end - y_start;

		float a = (float)line->width * cos(atanf(k2));
		float y1 = y_start - a / 2;
		float y2 = y_start + a - a / 2;
		if (fabs(y2 - y1) < 1.0f){
			if (y2 > y1){
				y2 = y1 + 1;
			}
			else{
				y1 =y2 + 1;
			}
		}
		
		if (y1 > y2){
			ps_swapf(&y1, &y2);
		}

		int limit = y2;
		ps_line_t l;
		l.rgba = line->rgba;
		for (int i = y1; i <= limit; ++i){
			l.x1 = (float)(i - b2) / k2;
			l.y1 = i;
			l.x2 = l.x1 + x_offset;
			l.y2 = l.y1 + y_offset;
			_draw_line(context, &l);
		}
	
		return;
	}

	ps_line_t temp;
	temp.rgba = line->rgba;
	if (is_x_equal && is_y_equal){
		_draw_line(context, line);
	}
	else if (is_x_equal){
		int x_start = line->x1 - line->width/2;
		temp.y1 = line->y1;
		temp.y2 = line->y2;
		for (int i = 0; i < line->width; ++i){
			temp.x1 = x_start + i;
			temp.x2 = temp.x1;
			_draw_line(context, &temp);
		}
	}
	else if (is_y_equal){
		int y_start = line->y1 - line->width / 2;
		temp.x1 = line->x1;
		temp.x2 = line->x2;
		for (int i = 0; i < line->width; ++i){
			temp.y1 = y_start + i;
			temp.y2 = temp.y1;
			_draw_line(context, &temp);
		}
	}

}

static void draw_rectangle(pen_source_t * context, ps_rectangle_t * rect){

}

//------------------------source------------------------//

static const char *pen_source_get_name(void *unused){
	UNUSED_PARAMETER(unused);
	return obs_module_text("PenInput");
}

static void pen_source_update(void *data, obs_data_t *settings){
	blog(LOG_DEBUG,"pen_source_update");
	struct pen_source *context = data;
}

static void pen_source_defaults(obs_data_t *settings){
	blog(LOG_DEBUG, "pen_source_defaults");
}

static void pen_source_show(void *data){
	blog(LOG_DEBUG, "pen_source_show");
}

static void pen_source_hide(void *data){
	blog(LOG_DEBUG, "pen_source_hide");
}

static void *pen_source_create(obs_data_t *settings, obs_source_t *source){
	struct pen_source *context = bzalloc(sizeof(struct pen_source));
	context->source = source;
	
	if (context->tex)
		gs_texture_destroy(context->tex);

	struct obs_video_info ovi;
	obs_get_video_info(&ovi);
	context->width = (int)ovi.base_width;
	context->height = (int)ovi.base_height;
	size_t len = sizeof(char) * context->width * context->height * 4;
	context->data = bzalloc(len);
	memset(context->data, 0, len);

	/*pixel_arg_t arg;
	arg.value = set_rgba(255,0,0,255);
	for (int i = 0; i < 10; ++i){
		for (int d = 0; d < 100; ++d){
			arg.y = context->height/2 + i;
			arg.x = context->width/2 + d;
			set_pixel(context->data, context->width, context->height, &arg);
		}
	}*/
	ps_line_t line;
	line.rgba = set_rgba(255, 0, 0, 255);
	line.x1 = 300;
	line.y1 = 300;
	line.x2 = 600;
	line.y2 = 600;
	line.width = 20;
	draw_line(context, &line);

	obs_enter_graphics();
	context->tex = gs_texture_create(context->width, context->height, GS_RGBA, 1, &context->data, GS_DYNAMIC);
	obs_leave_graphics();

	return context;
}

static void pen_source_destroy(void *data){
	struct pen_source *context = data;

	if (context->tex){
		obs_enter_graphics();
		gs_texture_destroy(context->tex);
		obs_leave_graphics();
	}

	if (context->data)
		bfree(context->data);

	bfree(context);
}

static uint32_t pen_source_getwidth(void *data){
	struct pen_source *context = data;
	return context->width;
}

static uint32_t pen_source_getheight(void *data){
	struct pen_source *context = data;
	return context->height;
}

static void pen_source_render(void *data, gs_effect_t *effect){
	struct pen_source *context = data;

	obs_enter_graphics();
	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), context->tex);
	gs_draw_sprite(context->tex, 0, context->width, context->height);	
	obs_leave_graphics();
}

static void pen_source_tick(void *data, float seconds)
{
	struct pen_source *context = data;
	context->update_time_elapsed += seconds;

}

static obs_properties_t *pen_source_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	return props;
}

struct obs_source_info pen_source_info = {
	.id = "pen_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = pen_source_get_name,
	.create = pen_source_create,
	.destroy = pen_source_destroy,
	.update = pen_source_update,
	.get_defaults = pen_source_defaults,
	.show = pen_source_show,
	.hide = pen_source_hide,
	.get_width = pen_source_getwidth,
	.get_height = pen_source_getheight,
	.video_render = pen_source_render,
	.video_tick = pen_source_tick,
	.get_properties = pen_source_properties
};
