//
//  main.c
//  Extension
//
//  Created by Dave Hayden on 7/30/14.
//  Copyright (c) 2014 Panic, Inc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>

#include "pd_api.h"

typedef float vec4_t __attribute__ ((vector_size (16)));


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

float sphere(const vec4_t* p, const vec4_t* origin, float radius)
{
	return magnitude(*origin - *p) - radius;
}

static vec4_t numerical_normal(const float d0, const vec4_t p, const float e=0.0001)
{
	auto dx_dd = sphere(p + vec3{e,0,0}) - d0;
	auto dy_dd = sphere(p + vec3{0,e,0}) - d0;
	auto dz_dd = sphere(p + vec3{0,0,e}) - d0;

	// assert(dx_dd != 0 || dy_dd != 0 || dz_dd != 0);

	return vec3{dx_dd, dy_dd, dz_dd}.unit();
}

static int update(void* userdata)
{
	PlaydateAPI* pd = userdata;
	static int count = 0;
	pd->graphics->clear(kColorBlack);

	vec4_t origins[LCD_ROWS * LCD_COLUMNS] = {};
	vec4_t sphere_center = { 0, 0, 5, 0 };

	memset(rb, 0, sizeof(rb));

	for (int i = 0; i < LCD_ROWS * LCD_COLUMNS; i++)
	{
		for (int step = 0; step < 10; step++)
		{
			float dist = sphere(&origins[i], &sphere_center, 1);

			if (dist < 0.01f)
			{
				rb[i / LCD_COLUMNS][i % LCD_COLUMNS] = 255;
				break;
			}

			origins[i] += rays[i] * dist;
		}
	}

	uint8_t* frame = pd->graphics->getFrame();
	for (int r = 0; r < LCD_ROWS; r++)
	{
		uint8_t* row = frame + (r * 52);
		for (int c = 0; c < LCD_COLUMNS; c++)
		{
			row[c>>3] |= ((rb[r][c] < (rand() & 0xFF)) << ((7-c) & 7));
		}
	}

	count++;
	pd->system->drawFPS(0,0);

	return 1;
}

