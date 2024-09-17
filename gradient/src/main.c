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

static int update(void* userdata);

#ifdef _WINDLL
__declspec(dllexport)
#endif

uint8_t rb[LCD_ROWS][LCD_COLUMNS];

int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
	(void)arg; // arg is currently only used for event = kEventKeyPressed

	if ( event == kEventInit )
	{
		// Note: If you set an update callback in the kEventInit handler, the system assumes the game is pure C and doesn't run any Lua code in the game
		pd->system->setUpdateCallback(update, pd);
	
		// write a verticle gradient into rb
		for (int r = 0; r < (LCD_ROWS); r++)
		{
			memset(rb[r], r, LCD_COLUMNS);
		}
	}
	
	return 0;
}

static int update(void* userdata)
{
	PlaydateAPI* pd = userdata;
	static int count = 0;
	pd->graphics->clear(kColorBlack);
	
	// Draw the gradient
	uint8_t* frame = pd->graphics->getFrame();
	for (int r = 0; r < LCD_ROWS; r++)
	{
		uint8_t* row = frame + (r * 52);
		for (int c = 0; c < LCD_COLUMNS; c++)
		{
			// The idea here is to use rng to create a dithered gradient temporally
			// We treat each value in rb as the probability of the pixel being on
			row[c>>3] |= ((rb[r][c] < (rand() & 0xFF)) << ((7-c) & 7));
		}
	}

	count++;
	pd->system->drawFPS(0,0);

	return 1;
}

