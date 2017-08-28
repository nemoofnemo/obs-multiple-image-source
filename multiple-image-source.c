#include "multiple-image-source.h"

struct obs_source_info multiple_image_source_info = {
	.id = "multiple_image_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO |
		OBS_SOURCE_CUSTOM_DRAW |
		OBS_SOURCE_COMPOSITE,
	.get_name = mis_getname,
	.create = mis_create,
	.destroy = mis_destroy,
	.update = mis_update,
	.activate = mis_activate,
	.deactivate = mis_deactivate,
	.video_render = mis_video_render,
	.video_tick = mis_video_tick,
	.audio_render = mis_audio_render,
	.enum_active_sources = mis_enum_sources,
	.get_width = mis_width,
	.get_height = mis_height,
	.get_defaults = mis_defaults,
	.get_properties = mis_properties
};

static obs_source_t *get_transition(struct multiple_image_source *mis)
{
	obs_source_t *tr;

	pthread_mutex_lock(&mis->mutex);
	tr = mis->transition;
	obs_source_addref(tr);
	pthread_mutex_unlock(&mis->mutex);

	return tr;
}

static obs_source_t *create_source_from_file(const char *file)
{
	obs_data_t *settings = obs_data_create();
	obs_source_t *source;

	obs_data_set_string(settings, "file", file);
	obs_data_set_bool(settings, "unload", false);
	source = obs_source_create_private("image_source", NULL, settings);

	obs_data_release(settings);
	return source;
}

static void free_files(struct darray *array)
{
	DARRAY(struct image_file_data) files;
	files.da = *array;

	for (size_t i = 0; i < files.num; i++) {
		bfree(files.array[i].path);
	}

	da_free(files);
}

static const char *mis_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("multiple_image_source");
}

static void add_file(struct multiple_image_source *mis, struct darray *array, const char *path)
{
	DARRAY(struct image_file_data) new_files;
	struct image_file_data data;
	new_files.da = *array;
	if (path && *path){
		data.path = bstrdup(path);
		da_push_back(new_files, &data);
	}
	*array = new_files.da;
}

static inline bool item_valid(struct multiple_image_source *mis)
{
	return mis->files.num && mis->cur_item < mis->files.num;
}

static void do_transition(void *data, bool to_null)
{
	struct multiple_image_source *mis = data;
	bool valid = item_valid(mis);
	blog(LOG_DEBUG, "mis in do_transition: cur_item %d", mis->cur_item);

	if (mis->prev_source){
		obs_source_release(mis->prev_source);
		mis->prev_source = NULL;
	}

	if (valid && mis->use_cut){
		obs_source_t * temp = create_source_from_file(mis->files.array[mis->cur_item].path);
		mis->prev_source = temp;
		obs_transition_set(mis->transition,temp);
	}

	else if (valid && !to_null){
		obs_source_t * temp = create_source_from_file(mis->files.array[mis->cur_item].path);
		mis->prev_source = temp;
		obs_transition_start(mis->transition,OBS_TRANSITION_MODE_AUTO,mis->tr_speed,temp);
	}
	else{
		obs_transition_start(mis->transition,OBS_TRANSITION_MODE_AUTO,mis->tr_speed,NULL);
	}
		
}

static void mis_update(void *data, obs_data_t *settings)
{
	DARRAY(struct image_file_data) new_files;
	DARRAY(struct image_file_data) old_files;
	obs_source_t *new_tr = NULL;
	obs_source_t *old_tr = NULL;
	struct multiple_image_source *mis = data;
	obs_data_array_t *array;
	const char *tr_name;
	uint32_t new_duration;
	uint32_t new_speed;
	uint32_t cx = 0;
	uint32_t cy = 0;
	size_t count;
	const char *behavior;
	const char *mode;

	/* ------------------------------------- */
	/* get settings data */

	da_init(new_files);

	behavior = obs_data_get_string(settings, S_BEHAVIOR);

	if (astrcmpi(behavior, S_BEHAVIOR_PAUSE_UNPAUSE) == 0)
		mis->behavior = BEHAVIOR_PAUSE_UNPAUSE;
	else if (astrcmpi(behavior, S_BEHAVIOR_ALWAYS_PLAY) == 0)
		mis->behavior = BEHAVIOR_ALWAYS_PLAY;
	else /* S_BEHAVIOR_STOP_RESTART */
		mis->behavior = BEHAVIOR_STOP_RESTART;

	mode = obs_data_get_string(settings, S_MODE);

	mis->manual = (astrcmpi(mode, S_MODE_MANUAL) == 0);

	tr_name = obs_data_get_string(settings, S_TRANSITION);
	if (astrcmpi(tr_name, TR_CUT) == 0)
		tr_name = "cut_transition";
	else if (astrcmpi(tr_name, TR_SWIPE) == 0)
		tr_name = "swipe_transition";
	else if (astrcmpi(tr_name, TR_SLIDE) == 0)
		tr_name = "slide_transition";
	else
		tr_name = "fade_transition";

	mis->loop = obs_data_get_bool(settings, S_LOOP);
	mis->hide = obs_data_get_bool(settings, S_HIDE);

	if (!mis->tr_name || strcmp(tr_name, mis->tr_name) != 0)
		new_tr = obs_source_create_private(tr_name, NULL, NULL);

	new_duration = (uint32_t)obs_data_get_int(settings, S_SLIDE_TIME);
	new_speed = (uint32_t)obs_data_get_int(settings, S_TR_SPEED);

	array = obs_data_get_array(settings, S_FILES);
	count = obs_data_array_count(array);

	/* ------------------------------------- */
	/* create new list of sources */

	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(array, i);
		const char *path = obs_data_get_string(item, "value");
		add_file(mis, &new_files.da, path);
		obs_data_release(item);
	}

	/* ------------------------------------- */
	/* update settings data */

	pthread_mutex_lock(&mis->mutex);

	old_files.da = mis->files.da;
	mis->files.da = new_files.da;
	if (new_tr) {
		old_tr = mis->transition;
		mis->transition = new_tr;
	}

	if (new_duration < 50)
		new_duration = 50;
	if (new_speed > (new_duration - 50))
		new_speed = new_duration - 50;

	mis->tr_speed = new_speed;
	mis->tr_name = tr_name;
	mis->slide_time = (float)new_duration / 1000.0f;

	pthread_mutex_unlock(&mis->mutex);

	mis_destroy_pages(&mis->pages);
	mis_init_pages(&mis->pages);
	for (int i = 0; i < new_files.num; ++i){
		mis_push_new_page(&mis->pages);
	}

	/* ------------------------------------- */
	/* clean up and restart transition */

	if (old_tr)
		obs_source_release(old_tr);
	free_files(&old_files.da);

	/* ------------------------- */

	mis->cur_item = 0;
	mis->elapsed = 0.0f;

	struct obs_video_info ovi;
	obs_get_video_info(&ovi);
	cx = (int)ovi.base_width;
	cy = (int)ovi.base_height;
	mis->cx = cx;
	mis->cy = cy;
	obs_transition_set_size(mis->transition, cx, cy);
	obs_transition_set_alignment(mis->transition, OBS_ALIGN_CENTER);
	obs_transition_set_scale_type(mis->transition, OBS_TRANSITION_SCALE_ASPECT);

	if (new_tr)
		obs_source_add_active_child(mis->source, new_tr);
	if (mis->files.num)
		do_transition(mis, false);

	obs_data_array_release(array);

	/* ------------------------ */

}

static void mis_play_pause(void *data)
{
	struct multiple_image_source *mis = data;

	mis->paused = !mis->paused;
	mis->manual = mis->paused;
}

static void mis_restart(void *data)
{
	struct multiple_image_source *mis = data;

	mis->elapsed = 0.0f;
	mis->cur_item = 0;

	do_transition(mis, false);

	mis->stop = false;
	mis->paused = false;
}

static void mis_stop(void *data)
{
	struct multiple_image_source *mis = data;

	mis->elapsed = 0.0f;
	mis->cur_item = 0;

	do_transition(mis, true);
	mis->stop = true;
	mis->paused = false;
}

static void mis_next_slide(void *data)
{
	struct multiple_image_source *mis = data;

	if (!mis->files.num)
		return;

	if (++mis->cur_item >= mis->files.num)
		mis->cur_item = 0;
	
	do_transition(mis, false);
}

static void mis_previous_slide(void *data)
{
	struct multiple_image_source *mis = data;

	if (!mis->files.num)
		return;

	if (mis->cur_item == 0)
		mis->cur_item = mis->files.num - 1;
	else
		--mis->cur_item;
	
	do_transition(mis, false);
}

static void mis_destroy(void *data)
{
	struct multiple_image_source *mis = data;
	if (mis->prev_source){
		obs_source_release(mis->prev_source);
		mis->prev_source = NULL;
	}
	obs_source_release(mis->transition);
	free_files(&mis->files.da);
	pthread_mutex_destroy(&mis->mutex);
	mis_destroy_pages(&mis->pages);
	bfree(mis);
}

static void *mis_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);
	struct multiple_image_source *mis = bzalloc(sizeof(*mis));

	mis->source = source;
	mis->prev_source = NULL;
	mis->manual = false;
	mis->paused = false;
	mis->stop = false;

	pthread_mutex_init_value(&mis->mutex);
	if (pthread_mutex_init(&mis->mutex, NULL) != 0)
		goto error;

	obs_source_update(source, NULL);

	//init paint pages
	mis_init_pages(&mis->pages);

	/*
	//test code.
	mis_line_t * line = mis_create_shape(LINE);
	line->x1 = 1;
	line->y1 = 1;
	line->x2 = 2;
	line->y2 = 2;
	line->width = 8;
	line->rgba = mis_set_rgba(127, 127, 127, 128);
	mis_setup_line(line);
	
	mis_rectangle_t * rect = mis_create_shape(RECTANGLE);
	rect->x = 300;
	rect->y = 400;
	rect->width = 200;
	rect->height = 100;
	rect->line_width = 10;
	rect->rgba = mis_set_rgba(255, 0, 0, 128);
	mis_setup_rectangle(rect);

	mis_polyline_t * polyline = mis_create_shape(POLYLINE);
	polyline->width = 4;
	polyline->rgba = mis_set_rgba(255, 0, 0, 255);
	mis_push_back_polyline_node(polyline, 120, 110);
	mis_push_back_polyline_node(polyline, 150, 130);
	mis_push_back_polyline_node(polyline, 190, 210);
	mis_push_back_polyline_node(polyline, 160, 103);
	mis_push_back_polyline_node(polyline, 10, 21);
	mis_setup_polyline(polyline);
	
	mis_push_shape_array(mis_get_page(&mis->pages, 0), LINE, line);
	mis_push_shape_array(mis_get_page(&mis->pages, 0), RECTANGLE, rect);
	mis_push_shape_array(mis_get_page(&mis->pages, 0), POLYLINE, polyline);*/

	return mis;

error:
	mis_destroy(mis);
	return NULL;
}

static void mis_video_render(void *data, gs_effect_t *effect)
{
	struct multiple_image_source *mis = data;
	obs_source_t *transition = get_transition(mis);

	if (transition) {
		obs_source_video_render(transition);
		obs_source_release(transition);
	}
	mis_paint(mis);
	UNUSED_PARAMETER(effect);
}

static void mis_video_tick(void *data, float seconds)
{
	struct multiple_image_source *mis = data;
	mis->elapsed += seconds;

	if (!mis->transition || !mis->slide_time)
		return;

	if (mis->restart_on_activate && mis->use_cut) {
		mis->elapsed = 0.0f;
		mis->cur_item = 0;
		do_transition(mis, false);
		mis->restart_on_activate = false;
		mis->use_cut = false;
		mis->stop = false;
		return;
	}

	if (mis->pause_on_deactivate || mis->stop || mis->paused)
		return;

	if (mis->manual){
		obs_data_t * settings = obs_source_get_settings(mis->source);
		const char * direction = obs_data_get_string(settings, S_DIRECTION);
		if (direction && *direction){
			if (strcmp(direction, S_DIRECTION_NEXT) == 0){
				mis_next_slide(mis);
				mis_pages_next(&mis->pages);
				mis_clear_paint_event(mis);
			}
			else if (strcmp(direction, S_DIRECTION_PREV) == 0){
				mis_previous_slide(mis);
				mis_pages_prev(&mis->pages);
				mis_clear_paint_event(mis);
			}
			obs_data_set_string(settings, S_DIRECTION, "");
		}
		mis_process_paint_event(mis, settings);
		obs_data_release(settings);
		return;
	}

	if (mis->elapsed > mis->slide_time) {
		mis->elapsed -= mis->slide_time;

		if (!mis->loop && mis->cur_item == mis->files.num - 1) {
			if (mis->hide)
				do_transition(mis, true);
			else
				do_transition(mis, false);

			return;
		}

		if (++mis->cur_item >= mis->files.num) {
			mis->cur_item = 0;
		}

		if (mis->files.num)
			do_transition(mis, false);
	}
}

static void mis_process_paint_event(multiple_image_source_t * mis, obs_data_t * settings){
	bool need_update = obs_data_get_bool(settings, "need_update");
	if (need_update){
		obs_data_set_bool(settings, "need_update", false);
		int x = obs_data_get_int(settings, "mouse_x");
		int y = obs_data_get_int(settings, "mouse_y");
		obs_data_set_int(settings, "mouse_x", 0);
		obs_data_set_int(settings, "mouse_y", 0);

		const char * create = obs_data_get_string(settings, "create_shape");
		if (create && *create){
			if (strcmp(create, "polyline") == 0){
				mis_polyline_t * polyline = mis_create_shape(POLYLINE);
				polyline->width = 4;
				polyline->rgba = mis_set_rgba(255, 0, 0, 255);
				mis_push_back_polyline_node(polyline, x, y);
				mis_setup_polyline(polyline);
				mis_push_shape_array(
					mis_get_page(&mis->pages, mis->pages.cur_page), 
					POLYLINE, 
					polyline);
			}
		}
		else if (create && *create == '\0'){
			mis_node_t * last = mis_get_last_from_array(
				mis_get_page(&mis->pages, mis->pages.cur_page));
			if (last){
				if (last->shape == POLYLINE){
					mis_polyline_t * polyline = last->data;
					mis_push_back_polyline_node(polyline, x, y);
					mis_setup_polyline(polyline);
				}
			}
		}
		obs_data_set_string(settings, "create_shape", "");
	}
}

static void mis_clear_paint_event(multiple_image_source_t * mis){
	obs_data_t * settings = obs_source_get_settings(mis->source);
	obs_data_set_bool(settings, "need_update", false);
	obs_data_set_int(settings, "mouse_x", 0);
	obs_data_set_int(settings, "mouse_y", 0);
	obs_data_set_string(settings, "create_shape", "");
	obs_data_release(settings);
}

static inline bool mis_audio_render_(obs_source_t *transition, uint64_t *ts_out,
		struct obs_source_audio_mix *audio_output,
		uint32_t mixers, size_t channels, size_t sample_rate)
{
	struct obs_source_audio_mix child_audio;
	uint64_t source_ts;

	if (obs_source_audio_pending(transition))
		return false;

	source_ts = obs_source_get_audio_timestamp(transition);
	if (!source_ts)
		return false;

	obs_source_get_audio_mix(transition, &child_audio);
	for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
		if ((mixers & (1 << mix)) == 0)
			continue;

		for (size_t ch = 0; ch < channels; ch++) {
			float *out = audio_output->output[mix].data[ch];
			float *in = child_audio.output[mix].data[ch];

			memcpy(out, in, AUDIO_OUTPUT_FRAMES *
					MAX_AUDIO_CHANNELS * sizeof(float));
		}
	}

	*ts_out = source_ts;

	UNUSED_PARAMETER(sample_rate);
	return true;
}

static bool mis_audio_render(void *data, uint64_t *ts_out,
		struct obs_source_audio_mix *audio_output,
		uint32_t mixers, size_t channels, size_t sample_rate)
{
	struct multiple_image_source *mis = data;
	obs_source_t *transition = get_transition(mis);
	bool success;

	if (!transition)
		return false;

	success = mis_audio_render_(transition, ts_out, audio_output, mixers,
			channels, sample_rate);

	obs_source_release(transition);
	return success;
}

static void mis_enum_sources(void *data, obs_source_enum_proc_t cb, void *param)
{
	struct multiple_image_source *mis = data;

	pthread_mutex_lock(&mis->mutex);
	if (mis->transition)
		cb(mis->source, mis->transition, param);
	pthread_mutex_unlock(&mis->mutex);
}

static uint32_t mis_width(void *data)
{
	struct multiple_image_source *mis = data;
	return mis->transition ? mis->cx : 0;
}

static uint32_t mis_height(void *data)
{
	struct multiple_image_source *mis = data;
	return mis->transition ? mis->cy : 0;
}

static void mis_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, S_TRANSITION, "cut");
	obs_data_set_default_int(settings, S_SLIDE_TIME, 6000);
	obs_data_set_default_int(settings, S_TR_SPEED, 750);
	
	obs_data_set_default_string(settings, S_BEHAVIOR, S_BEHAVIOR_ALWAYS_PLAY);
	obs_data_set_default_string(settings, S_MODE, S_MODE_MANUAL);
	obs_data_set_default_bool(settings, S_LOOP, true);
}

static obs_properties_t *mis_properties(void *data)
{
	obs_properties_t *ppts = obs_properties_create();
	return ppts;
}

static void mis_activate(void *data)
{
	struct multiple_image_source *mis = data;

	if (mis->behavior == BEHAVIOR_STOP_RESTART) {
		mis->restart_on_activate = true;
		mis->use_cut = true;
	} else if (mis->behavior == BEHAVIOR_PAUSE_UNPAUSE) {
		mis->pause_on_deactivate = false;
	}
}

static void mis_deactivate(void *data)
{
	struct multiple_image_source *mis = data;

	if (mis->behavior == BEHAVIOR_PAUSE_UNPAUSE)
		mis->pause_on_deactivate = true;
}

/* ------------------------------------------------------------------------- */

static void mis_setup_line(mis_line_t * line){
	if (line->width <= 0)
		return;

	if (line->buf){
		obs_enter_graphics();
		gs_vertexbuffer_destroy(line->buf);
		obs_leave_graphics();
	}

	line->buf = NULL;
	bool is_x_equal = line->x1 == line->x2;
	bool is_y_equal = line->y1 == line->y2;

	if (is_x_equal == 0 && is_y_equal == 0){
		float k1 = ((float)line->y1 - (float)line->y2) / ((float)line->x1 - (float)line->x2);
		float k2 = -1 / k1;
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
		}
		else{
			x_start = line->x1;
			x_end = line->x2;
			y_start = line->y1;
			y_end = line->y2;
		}
		b2 = y_start - k2 * x_start;

		int x_offset = x_end - x_start;
		int y_offset = y_end - y_start;

		float a = (float)line->width * cos(atanf(k1));
		float y1 = y_start - a / 2;
		float y2 = y_start + a - a / 2;

		if (y1 > y2){
			mis_swapf(&y1, &y2);
		}
		float x1 = (float)(y1 - b2) / k2;
		float x2 = (float)(y2 - b2) / k2;

		obs_enter_graphics();
		gs_render_start(true);
		gs_vertex2f(x2, y2);
		gs_vertex2f(x1, y1);
		gs_vertex2f(x2 + x_offset, y2 + y_offset);
		gs_vertex2f(x1 + x_offset, y1 + y_offset);
		line->buf = gs_render_save();
		obs_leave_graphics();
	}
	else if (is_x_equal && is_y_equal){
		obs_enter_graphics();
		gs_render_start(true);
		gs_vertex2f(line->x1, line->y1);
		line->buf = gs_render_save();
		obs_leave_graphics();
	}
	else if (is_x_equal){
		float x_start = (float)line->x1 - (float)line->width / 2;
		float x_end = (float)x_start + (float)line->width;
		float y_start = line->y1;
		float y_end = line->y2;
		if (y_start > y_end)
			mis_swapf(&y_start, &y_end);

		obs_enter_graphics();
		gs_render_start(true);
		gs_vertex2f(x_start, y_end);
		gs_vertex2f(x_start, y_start);
		gs_vertex2f(x_end, y_end);
		gs_vertex2f(x_end, y_start);
		line->buf = gs_render_save();
		obs_leave_graphics();
	}
	else if (is_y_equal){
		float x_start = line->x1;
		float x_end = line->x2;
		float y_start = (float)line->y1 - (float)line->width / 2;
		float y_end = y_start + (float)line->width;
		if (x_start > x_end)
			mis_swapf(&x_start, &x_end);

		obs_enter_graphics();
		gs_render_start(true);
		gs_vertex2f(x_start, y_end);
		gs_vertex2f(x_start, y_start);
		gs_vertex2f(x_end, y_end);
		gs_vertex2f(x_end, y_start);

		line->buf = gs_render_save();
		obs_leave_graphics();
	}
}

static void mis_destroy_line(mis_line_t * line){
	if (line && line->buf){
		obs_enter_graphics();
		gs_vertexbuffer_destroy(line->buf);
		obs_leave_graphics();
	}
}

static void mis_update_line(mis_line_t * line){
	if (line && line->buf){
		obs_enter_graphics();
		gs_vertexbuffer_destroy(line->buf);
		obs_leave_graphics();
		mis_setup_line(line);
	}
}

static void mis_paint_line(mis_line_t * line){
	gs_effect_t    *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t    *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");
	
	struct vec4 colorVal;
	vec4_set(&colorVal, mis_get_rgba_r(line->rgba),
		mis_get_rgba_g(line->rgba),
		mis_get_rgba_b(line->rgba),
		(float)mis_get_rgba_a(line->rgba)/0xff);

	gs_effect_set_vec4(color, &colorVal);
	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	gs_load_vertexbuffer(line->buf);
	gs_draw(GS_TRISTRIP, 0, 0);
	gs_technique_end_pass(tech);
	gs_technique_end(tech);
	gs_load_vertexbuffer(NULL);
}

static void mis_setup_rectangle(mis_rectangle_t * rect){
	int width_ex = rect->line_width / 2;
	mis_line_t line;
	line.rgba = rect->rgba;
	line.width = rect->line_width;
	line.x1 = rect->x - width_ex;
	line.y1 = rect->y;
	line.x2 = rect->x + rect->width + width_ex;
	line.y2 = rect->y;
	line.buf = rect->buf_arr[0];
	mis_setup_line(&line);
	rect->buf_arr[0] = line.buf;
	line.x1 = rect->x + rect->width;
	line.y1 = rect->y + width_ex;
	line.x2 = line.x1;
	line.y2 = rect->y + rect->height - width_ex;
	line.buf = rect->buf_arr[1];
	mis_setup_line(&line);
	rect->buf_arr[1] = line.buf;
	line.x1 = rect->x + rect->width + width_ex;
	line.y1 = rect->y + rect->height;
	line.x2 = rect->x - width_ex;
	line.y2 = rect->y + rect->height;
	line.buf = rect->buf_arr[2];
	mis_setup_line(&line);
	rect->buf_arr[2] = line.buf;
	line.x1 = rect->x;
	line.y1 = rect->y + rect->height - width_ex;
	line.x2 = rect->x;
	line.y2 = rect->y + width_ex;
	line.buf = rect->buf_arr[3];
	mis_setup_line(&line);
	rect->buf_arr[3] = line.buf;
}

static void mis_destroy_rectangle(mis_rectangle_t * rect){
	obs_enter_graphics();
	for (int i = 0; i < 4; ++i){
		if (rect->buf_arr[i])
			gs_vertexbuffer_destroy(rect->buf_arr[i]);
	}
	obs_leave_graphics();
}

static void mis_update_rectangle(mis_rectangle_t * rect){
	if (rect && rect->buf_arr){
		obs_enter_graphics();
		for (int i = 0; i < 4; ++i){
			if (rect->buf_arr[i])
				gs_vertexbuffer_destroy(rect->buf_arr[i]);
		}
		obs_leave_graphics();
		mis_setup_rectangle(rect);
	}
}

static void mis_paint_rectangle(mis_rectangle_t * rect){
	gs_effect_t    *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t    *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	struct vec4 colorVal;
	vec4_set(&colorVal, mis_get_rgba_r(rect->rgba),
		mis_get_rgba_g(rect->rgba),
		mis_get_rgba_b(rect->rgba),
		(float)mis_get_rgba_a(rect->rgba)/0xff);

	gs_effect_set_vec4(color, &colorVal);
	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	for (int i = 0; i < 4; ++i){
		gs_load_vertexbuffer(rect->buf_arr[i]);
		gs_draw(GS_TRISTRIP, 0, 0);
		gs_load_vertexbuffer(NULL);
	}
	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

static void mis_init_polyline(mis_polyline_t * polyline){
	da_init(polyline->node_arr);
}

static void mis_push_back_polyline_node(mis_polyline_t * polyline, int x, int y){
	mis_polyline_node_t node;
	node.x = x;
	node.y = y;
	node.buf = NULL;
	da_push_back(polyline->node_arr, &node);
}

static void mis_setup_polyline(mis_polyline_t * polyline){
	if (polyline->node_arr.num > 1){
		size_t limit = polyline->node_arr.num - 1;
		mis_line_t line;
		mis_polyline_node_t * temp;
		line.rgba = polyline->rgba;
		line.width = polyline->width;
		for (size_t i = 0; i < limit; ++i){
			temp = da_get(polyline->node_arr, i + 1, mis_polyline_node_t);
			line.x1 = temp->x;
			line.y1 = temp->y;
			temp = da_get(polyline->node_arr, i, mis_polyline_node_t);
			line.x2 = temp->x;
			line.y2 = temp->y;
			line.buf = temp->buf;
			if (temp->buf == NULL){
				mis_setup_line(&line);
				temp->buf = line.buf;
			}
		}
	}
}

static void mis_destroy_polyline(mis_polyline_t * polyline){
	if (polyline->node_arr.num > 1){
		size_t limit = polyline->node_arr.num - 1;
		obs_enter_graphics();
		for (size_t i = 0; i < limit; ++i){
			gs_vertbuffer_t * temp =
				da_get(polyline->node_arr, i, mis_polyline_node_t)->buf;
			if (temp)
				gs_vertexbuffer_destroy(temp);
		}
		obs_leave_graphics();
	}
	da_free(polyline->node_arr);
}

static void mis_update_polyline(mis_polyline_t * polyline){
	if (polyline->node_arr.num > 1){
		size_t limit = polyline->node_arr.num - 1;
		obs_enter_graphics();
		for (size_t i = 0; i < limit; ++i){
			gs_vertbuffer_t * temp =
				da_get(polyline->node_arr, i, mis_polyline_node_t)->buf;
			if (temp)
				gs_vertexbuffer_destroy(temp);
		}
		obs_leave_graphics();
		mis_setup_polyline(polyline);
	}
}

static void mis_paint_polyline(mis_polyline_t * polyline){
	if (polyline->node_arr.num > 1){
		gs_effect_t    *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
		gs_eparam_t    *color = gs_effect_get_param_by_name(solid, "color");
		gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

		struct vec4 colorVal;
		vec4_set(&colorVal, mis_get_rgba_r(polyline->rgba),
			mis_get_rgba_g(polyline->rgba),
			mis_get_rgba_b(polyline->rgba),
			(float)mis_get_rgba_a(polyline->rgba) / 0xff);

		gs_effect_set_vec4(color, &colorVal);
		gs_technique_begin(tech);
		gs_technique_begin_pass(tech, 0);
		size_t limit = polyline->node_arr.num - 1;
		for (size_t i = 0; i < limit; ++i){
			gs_load_vertexbuffer(da_get(polyline->node_arr, i, mis_polyline_node_t)->buf);
			gs_draw(GS_TRISTRIP, 0, 0);
			gs_load_vertexbuffer(NULL);
		}
		gs_technique_end_pass(tech);
		gs_technique_end(tech);
	}
}

static void * mis_create_shape(mis_shape_t shape){
	void * ret = NULL;
	switch (shape){
	case NOTHING:
		break;
	case LINE:
		ret = bzalloc(sizeof(mis_line_t));
		memset(ret, 0, sizeof(mis_line_t));
		break;
	case RECTANGLE:
		ret = bzalloc(sizeof(mis_rectangle_t));
		memset(ret, 0, sizeof(mis_rectangle_t));
		break;
	case POLYLINE:
		ret = bzalloc(sizeof(mis_polyline_t));
		memset(ret, 0, sizeof(mis_polyline_t));
		mis_init_polyline(ret);
		break;
	default:
		break;
	}
	return ret;
}

static void mis_delete_shape(mis_shape_t shape, void * data){
	switch (shape){
	case NOTHING:
		break;
	case LINE:
		mis_destroy_line(data);
		break;
	case RECTANGLE:
		mis_destroy_rectangle(data);
		break;
	case POLYLINE:
		mis_destroy_polyline(data);
		break;
	default:
		break;
	}
	bfree(data);
}

static void mis_init_shape_array(mis_shape_array_t * arr){
	da_init(arr->shape_array);
}

static void mis_destroy_shape_array(mis_shape_array_t * arr){
	size_t limit = arr->shape_array.num;
	mis_node_t * temp;
	for (size_t i = 0; i < limit; ++i){
		temp = da_get(arr->shape_array, i, mis_node_t);
		mis_delete_shape(temp->shape, temp->data);
	}
	da_free(arr->shape_array);
}

static void mis_push_shape_array(mis_shape_array_t * arr, mis_shape_t shape, void * data){
	mis_node_t node;
	node.data = data;
	node.shape = shape;
	da_push_back(arr->shape_array, &node);
}

static mis_node_t * mis_get_from_array(mis_shape_array_t * arr, size_t idx){
	return da_get(arr->shape_array, idx, mis_node_t);
}

static mis_node_t * mis_get_last_from_array(mis_shape_array_t * arr){
	return mis_get_from_array(arr, arr->shape_array.num - 1);
}

static void mis_paint_shape_array(mis_shape_array_t * arr){
	size_t limit = arr->shape_array.num;
	mis_node_t * temp;
	for (size_t i = 0; i < limit; ++i){
		temp = da_get(arr->shape_array, i, mis_node_t);
		switch (temp->shape){
		case NOTHING:
			warn("in mis_paint_shape_array: nothing paint.");
			break;
		case LINE:
			mis_paint_line(temp->data);
			break;
		case RECTANGLE:
			mis_paint_rectangle(temp->data);
			break;
		case POLYLINE:
			mis_paint_polyline(temp->data);
			break;
		default:
			warn("in mis_paint_shape_array: invalid shape.");
			break;
		}
	}
}

static void mis_init_pages(mis_pages_t * pages){
	da_init(pages->pages);
	pages->cur_page = 0;
	pages->paint_status = NOTHING;
}

static void mis_destroy_pages(mis_pages_t * pages){
	size_t limit = pages->pages.num;
	for (size_t i = 0; i < limit; ++i){
		mis_destroy_shape_array(mis_get_page(pages, i));
	}
	da_free(pages->pages);
	pages->cur_page = 0;
	pages->paint_status = NOTHING;
}

static void mis_push_new_page(mis_pages_t * pages){
	mis_shape_array_t page;
	mis_init_shape_array(&page);
	da_push_back(pages->pages, &page);
}

static mis_shape_array_t * mis_get_page(mis_pages_t * pages, size_t idx){
	return da_get(pages->pages, idx, mis_shape_array_t);
}

static void mis_pages_prev(mis_pages_t * pages){
	if (pages->pages.num == 0)
		return;
	if (pages->cur_page == 0){
		pages->cur_page = pages->pages.num - 1;
	}
	else{
		pages->cur_page--;
	}
}

static void mis_pages_next(mis_pages_t * pages){
	if (pages->pages.num == 0)
		return;
	if (pages->cur_page == pages->pages.num - 1){
		pages->cur_page = 0;
	}
	else{
		pages->cur_page++;
	}
}

static void mis_paint_pages(mis_pages_t * pages){
	if (pages->pages.num > 0){
		mis_paint_shape_array(mis_get_page(pages, pages->cur_page));
	}
}

/* ------------------------------------------------------------------------- */

static void mis_paint(multiple_image_source_t * mis){
	mis_paint_pages(&mis->pages);
}