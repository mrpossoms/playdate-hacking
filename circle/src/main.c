#include <stdio.h>
#include <stdlib.h>

#include "pd_api.h"

static int update(void* userdata);

#ifdef _WINDLL
__declspec(dllexport)
#endif


int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
	(void)arg; // arg is currently only used for event = kEventKeyPressed

	if ( event == kEventInit )
	{
		const char* err;

		// Note: If you set an update callback in the kEventInit handler, the system assumes the game is pure C and doesn't run any Lua code in the game
		pd->system->setUpdateCallback(update, pd);
	
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
	
	circle(pd, count);
	
	count++;
	pd->system->drawFPS(0,0);

	return 1;
}

