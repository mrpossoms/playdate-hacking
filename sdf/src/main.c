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
const char* fontpath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
LCDFont* font = NULL;

#ifdef _WINDLL
__declspec(dllexport)
#endif


#define TEXT_WIDTH 86
#define TEXT_HEIGHT 16

int x = (400-TEXT_WIDTH)/2;
int y = (240-TEXT_HEIGHT)/2;
int dx = 1;
int dy = 2;

uint8_t rb[LCD_ROWS][LCD_COLUMNS];
vec4_t rays[LCD_ROWS * LCD_COLUMNS];

typedef struct {
	float focal_length;
	struct {
		int rows, cols;
	} sensor;
} pinhole_t;

void pinhole_rays(const pinhole_t* pinhole, vec4_t* rays)
{
	float sensor_width = pinhole->sensor.cols;
	float sensor_height = pinhole->sensor.rows;
	float sensor_aspect = sensor_width / sensor_height;

	for (int r = 0; r < pinhole->sensor.rows; r++)
	{
		for (int c = 0; c < pinhole->sensor.cols; c++)
		{
			float x = (c - sensor_width / 2) / sensor_width;
			float y = (r - sensor_height / 2) / sensor_height;
			vec4_t ray = { x, y, -pinhole->focal_length, 0 };
			rays[r * pinhole->sensor.cols + c] = ray;
		}
	}
}

int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
	(void)arg; // arg is currently only used for event = kEventKeyPressed

	if ( event == kEventInit )
	{
		const char* err;
		font = pd->graphics->loadFont(fontpath, &err);
		
		if ( font == NULL )
			pd->system->error("%s:%i Couldn't load font %s: %s", __FILE__, __LINE__, fontpath, err);

		// Note: If you set an update callback in the kEventInit handler, the system assumes the game is pure C and doesn't run any Lua code in the game
		pd->system->setUpdateCallback(update, pd);
	
		for (int r = 0; r < (LCD_ROWS); r++)
		{
			memset(rb[r], r, LCD_COLUMNS);
		}

		pinhole_rays(&(pinhole_t){ 1.0, { LCD_ROWS, LCD_COLUMNS } }, rays);
	}
	
	return 0;
}

void circle(PlaydateAPI* pd, int t)
{
	int cy = LCD_ROWS >> 1;
	int cx = LCD_COLUMNS >> 1;
	int rad2 = sin(t / 100.0) * 1000 + 1000;
	uint8_t* frame = pd->graphics->getFrame();

	for (int r = 0; r < LCD_ROWS; r++)
	{
		uint8_t* row = frame + (r * 52);
		int dr2 = (r - cy) * (r - cy);
		for (int c = 0; c < LCD_COLUMNS; c++)
		{
			int dc2 = (c - cx) * (c - cx);
			int in_circle = (dr2 + dc2) < rad2;
			int bi = c >> 3;
			row[bi] |= (in_circle << ((7-c) & 7));
		}
	}
}

static int update(void* userdata)
{
	PlaydateAPI* pd = userdata;
	static int count = 0;
	pd->graphics->clear(kColorBlack);
	// pd->graphics->setFont(font);
	// pd->graphics->drawText("Hello World!", strlen("Hello World!"), kASCIIEncoding, x, y);

	

	// x += dx;
	// y += dy;
	
	// if ( x < 0 || x > LCD_COLUMNS - TEXT_WIDTH )
	// 	dx = -dx;
	
	// if ( y < 0 || y > LCD_ROWS - TEXT_HEIGHT )
	// 	dy = -dy;

	// memset(frame, count++ % 2, 52 * LCD_ROWS);
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

