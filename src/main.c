#include <pebble.h>

#define COLORS       true
#define ANTIALIASING true

#define HAND_MARGIN  45
#define FINAL_RADIUS 55

#define ANIMATION_DURATION 500
#define ANIMATION_DELAY    600

#define NUMERAL_BUFFER_SIZE 100

typedef struct {
	int hours;
	int minutes;
} Time;

static Window *s_main_window;
static Layer *s_canvas_layer;

static GPoint s_center;
static Time s_last_time, s_anim_time;
static int s_radius = 0, s_anim_hours_60 = 0, s_color_channels[3];
static bool s_animating = false;

static GFont s_custom_font_24;
static GFont s_numerals_font;

static TextLayer *s_font_layer;
static TextLayer *s_numeral_font_layer;

static Layer *s_background_image_layer;
static GBitmap *s_background_image;

static Layer *s_soulstone_image_layer;
static GBitmap *s_soulstone_image;

static char* s_numeral_char_buffer;

/*************************** AnimationImplementation **************************/

static void animation_started(Animation *anim, void *context) {
	s_animating = true;
}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
	s_animating = false;
}

static void animate(int duration, int delay, AnimationImplementation *implementation, bool handlers) {
	Animation *anim = animation_create();
	animation_set_duration(anim, duration);
	animation_set_delay(anim, delay);
	animation_set_curve(anim, AnimationCurveEaseInOut);
	animation_set_implementation(anim, implementation);
	if (handlers) {
		animation_set_handlers(anim, (AnimationHandlers) {
			.started = animation_started,
				.stopped = animation_stopped
		}, NULL);
	}
	animation_schedule(anim);
}

/************************************ UI **************************************/

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
	// Store time
	s_last_time.hours = tick_time->tm_hour;
	s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;
	s_last_time.minutes = tick_time->tm_min;

	for (int i = 0; i < 3; i++) {
		s_color_channels[i] = rand() % 256;
	}

	// Redraw
	if (s_canvas_layer) {
		layer_mark_dirty(s_canvas_layer);
	}
}

static void int_to_roman_numerals(int input, char* out, int buffer_size)
{
	int i = 0;

	while (input > 0 && i < buffer_size - 1)
	{
		// APP_LOG(APP_LOG_LEVEL_DEBUG, "int_to_roman_numerals index now %d input is %d", i, input);
		if (input == 9 || input == 4)
		{
			out[i++] = 'I';
			input += 1;
		}
		else if (input >= 10)
		{
			out[i++] = 'X';
			input -= 10;
		}
		else if (input >= 5)
		{
			out[i++] = 'V';
			input -= 5;
		}
		else
		{
			out[i++] = 'I';
			input -= 1;
		}
	}

	out[i] = '\0';
}

static int hours_to_minutes(int hours_out_of_12) {
	return (int)(float)(((float)hours_out_of_12 / 12.0F) * 60.0F);
}

static void update_proc(Layer *layer, GContext *ctx) {
	// Color background?
	GRect bounds = layer_get_bounds(layer);
	if (COLORS) {
		graphics_context_set_fill_color(ctx, GColorFromRGB(s_color_channels[0], s_color_channels[1], s_color_channels[2]));
		graphics_fill_rect(ctx, bounds, 0, GCornerNone);
	}

	graphics_context_set_stroke_color(ctx, GColorBlack);
	graphics_context_set_stroke_width(ctx, 4);

	graphics_context_set_antialiased(ctx, ANTIALIASING);

	const uint8_t offset = PBL_IF_ROUND_ELSE(17, 0);
	// GSize image_size = gbitmap_get_bounds(s_background_image).size;
	// graphics_draw_bitmap_in_rect(ctx, s_background_image, GRect(5 + offset, 5 + offset, image_size.w,
	// image_size.h));
	graphics_draw_bitmap_in_rect(ctx, s_background_image, GRect(offset, offset, bounds.size.w,
		bounds.size.h));

	GSize soulstone_image_size = gbitmap_get_bounds(s_soulstone_image).size;
	graphics_context_set_compositing_mode(ctx, GCompOpSet);
	// bitmap_layer_set_background_color(s_soulstone_image_layer, GColorClear);
	//graphics_draw_bitmap_in_rect(ctx, s_soulstone_image, GRect(5 + offset, 5 + offset, soulstone_image_size.w,
	//  soulstone_image_size.h));
	GPoint minute_hand = (GPoint) {
		.x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * s_last_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.x,
			.y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * s_last_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.y,
	};
	graphics_draw_rotated_bitmap(ctx, s_soulstone_image, GPoint(soulstone_image_size.w / 2, soulstone_image_size.h),
		(int16_t)(TRIG_MAX_ANGLE * s_last_time.minutes / 60.0), minute_hand);

	//// White clockface
	//graphics_context_set_fill_color(ctx, GColorWhite);
	//graphics_fill_circle(ctx, s_center, s_radius);

	//// Draw outline
	//graphics_draw_circle(ctx, s_center, s_radius);

	//// Don't use current time while animating
	//Time mode_time = (s_animating) ? s_anim_time : s_last_time;

	//// Adjust for minutes through the hour
	//float minute_angle = TRIG_MAX_ANGLE * mode_time.minutes / 60;
	//float hour_angle;
	//if(s_animating) {
	//  // Hours out of 60 for smoothness
	//  hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 60;
	//} else {
	//  hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 12;
	//}
	//hour_angle += (minute_angle / TRIG_MAX_ANGLE) * (TRIG_MAX_ANGLE / 12);

	//// Plot hands
	//GPoint minute_hand = (GPoint) {
	//  .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.x,
	//  .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.y,
	//};
	//GPoint hour_hand = (GPoint) {
	//  .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.x,
	//  .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.y,
	//};

	//// Draw hands with positive length only
	//if(s_radius > 2 * HAND_MARGIN) {
	//  graphics_draw_line(ctx, s_center, hour_hand);
	//} 
	//if(s_radius > HAND_MARGIN) {
	//  graphics_draw_line(ctx, s_center, minute_hand);
	//}

	int_to_roman_numerals(s_last_time.hours, s_numeral_char_buffer, NUMERAL_BUFFER_SIZE);
	text_layer_set_text(s_numeral_font_layer, s_numeral_char_buffer);
}

static void window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect window_bounds = layer_get_bounds(window_layer);

	s_center = grect_center_point(&window_bounds);

	s_canvas_layer = layer_create(window_bounds);
	layer_set_update_proc(s_canvas_layer, update_proc);
	layer_add_child(window_layer, s_canvas_layer);

	// Font business
	// s_font_layer = text_layer_create(GRect(0, 50, 144, 50));
	// text_layer_set_background_color(s_font_layer, GColorClear);
	// text_layer_set_text(s_font_layer, "TextLayer");

	// s_custom_font_24 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_EXOCET_24));
	// text_layer_set_font(s_font_layer, s_custom_font_24);
	// layer_add_child(window_layer, text_layer_get_layer(s_font_layer));

	s_numeral_char_buffer = malloc(sizeof(char) * NUMERAL_BUFFER_SIZE);
	s_numeral_font_layer = text_layer_create(GRect(0, 50, 144, 50));
	text_layer_set_background_color(s_numeral_font_layer, GColorClear);
	text_layer_set_text(s_numeral_font_layer, "");

	s_background_image = gbitmap_create_with_resource(RESOURCE_ID_BACKGROUND_DIABLO);
	s_background_image_layer = layer_create(window_bounds);
	layer_add_child(window_layer, s_background_image_layer);


	s_soulstone_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SOULSTONE);
	s_soulstone_image_layer = layer_create(window_bounds);
	layer_add_child(window_layer, s_soulstone_image_layer);

	s_numerals_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_EXOCET_48));
	text_layer_set_font(s_numeral_font_layer, s_numerals_font);
	text_layer_set_text_alignment(s_numeral_font_layer, GTextAlignmentCenter);
	text_layer_set_text_color(s_numeral_font_layer, GColorChromeYellow);
	layer_add_child(window_layer, text_layer_get_layer(s_numeral_font_layer));

}

static void window_unload(Window *window) {
	layer_destroy(s_canvas_layer);

	gbitmap_destroy(s_background_image);
	layer_destroy(s_background_image_layer);

	gbitmap_destroy(s_soulstone_image);
	layer_destroy(s_soulstone_image_layer);

	// fonts_unload_custom_font(s_custom_font_24);
	// text_layer_destroy(s_font_layer);
	fonts_unload_custom_font(s_numerals_font);
	text_layer_destroy(s_numeral_font_layer);

	free(s_numeral_char_buffer);
}

/*********************************** App **************************************/

static int anim_percentage(AnimationProgress dist_normalized, int max) {
	return (int)(float)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) * (float)max);
}

static void radius_update(Animation *anim, AnimationProgress dist_normalized) {
	s_radius = anim_percentage(dist_normalized, FINAL_RADIUS);

	layer_mark_dirty(s_canvas_layer);
}

static void hands_update(Animation *anim, AnimationProgress dist_normalized) {
	s_anim_time.hours = anim_percentage(dist_normalized, hours_to_minutes(s_last_time.hours));
	s_anim_time.minutes = anim_percentage(dist_normalized, s_last_time.minutes);

	layer_mark_dirty(s_canvas_layer);
}


static void init() {
	srand(time(NULL));

	time_t t = time(NULL);
	struct tm *time_now = localtime(&t);
	tick_handler(time_now, MINUTE_UNIT);

	s_main_window = window_create();
	window_set_window_handlers(s_main_window, (WindowHandlers) {
		.load = window_load,
			.unload = window_unload,
	});
	window_stack_push(s_main_window, true);

	tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

	// Prepare animations
	AnimationImplementation radius_impl = {
		.update = radius_update
	};
	animate(ANIMATION_DURATION, ANIMATION_DELAY, &radius_impl, false);

	AnimationImplementation hands_impl = {
		.update = hands_update
	};
	animate(2 * ANIMATION_DURATION, ANIMATION_DELAY, &hands_impl, true);
}

static void deinit() {
	window_destroy(s_main_window);
}

int main() {
	init();
	app_event_loop();
	deinit();
}
