//
//  main.c
//  Extension
//
//  Created by Dave Hayden on 7/30/14.
//  Copyright (c) 2014 Panic, Inc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "pd_api.h"

typedef float vec4_t __attribute__ ((vector_size (16)));
// typedef float mat4_t __attribute__ ((vector_size (64)));

static int update(void* userdata);

#ifdef _WINDLL
__declspec(dllexport)
#endif


uint8_t rb[LCD_ROWS][LCD_COLUMNS];
vec4_t rays[LCD_ROWS * LCD_COLUMNS];

typedef struct {
	float focal_length;
	struct {
		int rows, cols;
		float pixel_size;
	} sensor;
} pinhole_t;

static inline float magnitude(const vec4_t v)
{
	return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static inline vec4_t unit4f(const vec4_t v)
{
	return v / magnitude(v);
}

static inline float dot4f(const vec4_t a, const vec4_t b)
{
	vec4_t c = a * b;
	return c[0] + c[1] + c[2] + c[3];
}

static inline vec4_t abs4f(const vec4_t v)
{
	return (vec4_t){ fabs(v[0]), fabs(v[1]), fabs(v[2]), fabs(v[3]) };
}

uint8_t fast_rand()
{
	static uint8_t lut[256];
	static int init = 0;
	if (!init)
	{
		for (int i = 0; i < 256; i++)
		{
			lut[i] = rand() & 0xFF;
		}
	}

	return lut[init++ & 0xff];
}

void pinhole_rays(const pinhole_t* pinhole, vec4_t* rays)
{
	float sensor_width = pinhole->sensor.cols;
	float sensor_height = pinhole->sensor.rows;
	float sensor_aspect = sensor_width / sensor_height;

	for (int r = 0; r < pinhole->sensor.rows; r++)
	{
		for (int c = 0; c < pinhole->sensor.cols; c++)
		{
			float x = (c - sensor_width / 2) * pinhole->sensor.pixel_size;
			float y = (r - sensor_height / 2) * pinhole->sensor.pixel_size;
			vec4_t ray = { x, y, pinhole->focal_length, 0 };
			rays[r * pinhole->sensor.cols + c] = ray / magnitude(ray);
		}
	}
}

int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
	(void)arg; // arg is currently only used for event = kEventKeyPressed

	if ( event == kEventInit )
	{

		// Note: If you set an update callback in the kEventInit handler, the system assumes the game is pure C and doesn't run any Lua code in the game
		pd->system->setUpdateCallback(update, pd);
	
		pd->display->setRefreshRate(50);

		for (int r = 0; r < (LCD_ROWS); r++)
		{
			memset(rb[r], r, LCD_COLUMNS);
		}

		pinhole_rays(&(pinhole_t){ 
			.focal_length=1.0, 
			.sensor = { LCD_ROWS, LCD_COLUMNS, 0.01f } 
		}, rays);
	}
	
	return 0;
}

static inline float ground(const vec4_t p)
{
	return 1.f - p[1];
}

static inline float sphere(const vec4_t p, const vec4_t origin, float radius)
{
	vec4_t delta = origin - p;
	return magnitude(delta) - radius;
}

static inline float octahedron(vec4_t p, const vec4_t origin, float s, float y_rad)
{
	// \ a  b \ |x| ~ | a * x + b * y |
	// | c  d | |y| ~ | c * x + d * y |

	static float theta;
	static float cy;
	static float sy;

	if (y_rad != theta)
	{
		theta = y_rad;
		cy = cosf(y_rad);
		sy = sinf(y_rad);
	}

	float a = cy;
	float b = sy;
	float c = -sy;
	float d = cy;

	vec4_t dp = p - origin;
	p[0] = a * dp[0] + b * dp[2];
	p[2] = c * dp[0] + d * dp[2];
	p = abs4f(p);
	return (p[0] + p[1] + p[2] - s) * 0.57735027f;
}

vec4_t shape_center = { 0, 0, 2.5, 0 };

float t = 0;

float scene(const vec4_t p)
{
	// vec4_t p_p = mat4_mul_vec4(world_inv, p);

	// return sphere(p, shape_center, 1);
	// return fminf(ground(p), octahedron(p, shape_center, 1, t));
	return fminf(ground(p), sphere(p, shape_center, 1.f));
}

static vec4_t numerical_normal(const float d0, const vec4_t p, const float e)
{
	const vec4_t dx = { e, 0, 0, 0 };
	const vec4_t dy = { 0, e, 0, 0 };
	const vec4_t dz = { 0, 0, e, 0 };

	float dx_dd = scene(p + dx) - d0;
	float dy_dd = scene(p + dy) - d0;
	float dz_dd = scene(p + dz) - d0;

	vec4_t normal = { dx_dd, dy_dd, dz_dd, 0 };

	return unit4f(normal);
}

static int update(void* userdata)
{
	PlaydateAPI* pd = userdata;
	static int count = 0;
	pd->graphics->clear(kColorBlack);

	vec4_t origins[LCD_ROWS * LCD_COLUMNS] = {};

	t = count / 50.0;
	vec4_t light_dir = { cosf(pd->system->getCrankAngle()*(3.14159f/180.f)), -1.f, sinf(pd->system->getCrankAngle()*(3.14159f/180.f)), 0 };
	light_dir = unit4f(light_dir);

	// memset(rb, 0, sizeof(rb));

	// mat4_I(world);
	// mat4_translate(world, (vec4_t){ 0, 0, -5, 0 });
	// mat4_inv(world_inv, world);

	for (int r = 0; r < LCD_ROWS; r++)
	{
		for (int c = 0; c < LCD_COLUMNS; c++)
		{
			float t = 0;
			for (int step = 0; step < 30; step++)
			{
				int i = r * LCD_COLUMNS + c;
				vec4_t p_t = origins[i] + rays[i] * t;
				float dist = scene(p_t);

				if (dist <= 0.01f)
				{
					vec4_t n = numerical_normal(dist, p_t, 0.01f);
					float ndl = dot4f(n,light_dir);

					t = 0.001f;
					p_t += n * 0.01f;
					for (int shadow_step = 0; shadow_step < 10; shadow_step++)
					{
						float shadow_dist = scene(p_t + light_dir * t);
						if (shadow_dist < 0.001f)
						{
							ndl = 0.f;
							break;
						}
						t += shadow_dist;
					}
					rb[r][c] = (uint8_t)(((ndl >= 0) * ndl) * 255);

					break;
				}
				else
				{
					rb[r][c] = 0;
				}

				t += dist;
			}
		}
	}

	uint8_t* frame = pd->graphics->getFrame();
	for (int r = 0; r < LCD_ROWS; r++)
	{
		uint8_t* row = frame + (r * 52);
		for (int c = 0; c < LCD_COLUMNS; c++)
		{
			row[c>>3] |= ((rb[r][c] > (fast_rand())) << ((7-c) & 7));
		}
	}

	count++;
	pd->system->drawFPS(0,0);

	return 1;
}

