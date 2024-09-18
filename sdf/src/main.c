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
// typedef float mat4_t __attribute__ ((vector_size (64)));
typedef vec4_t mat4_t[4];

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

static inline vec4_t unit4(const vec4_t v)
{
	return v / magnitude(v);
}

static inline float dot4(const vec4_t a, const vec4_t b)
{
	vec4_t c = a * b;
	return c[0] + c[1] + c[2] + c[3];
}

static inline vec4_t abs4(const vec4_t v)
{
	return (vec4_t){ fabs(v[0]), fabs(v[1]), fabs(v[2]), fabs(v[3]) };
}

static void mat4_T(mat4_t r, const mat4_t m)
{
	for (int i = 0; i < 4; i++)
	{
		r[i] = (vec4_t){ m[0][i], m[1][i], m[2][i], m[3][i] };
	}
}

static void mat4_mul(mat4_t r, const mat4_t a, const mat4_t b)
{
	mat4_t bt;
	mat4_T(bt, b);
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			r[i][j] = dot4(a[i], bt[j]);
		}
	}
}

static vec4_t mat4_mul_vec4(const mat4_t m, const vec4_t v)
{
	vec4_t result = { 0 };
	for (int i = 0; i < 4; i++)
	{
		result[i] = dot4(m[i], v);
	}
	return result;
}


static void mat4_translate(mat4_t r, const vec4_t v)
{
	r[0] = (vec4_t){ 1, 0, 0, v[0] };
	r[1] = (vec4_t){ 0, 1, 0, v[1] };
	r[2] = (vec4_t){ 0, 0, 1, v[2] };
	r[3] = (vec4_t){ 0, 0, 0, 1 };
}

static void mat4_rot_y(mat4_t r, float theta)
{
	float c = cosf(theta);
	float s = sinf(theta);
	r[0] = (vec4_t){ c, 0, s, 0 };
	r[1] = (vec4_t){ 0, 1, 0, 0 };
	r[2] = (vec4_t){-s, 0, c, 0 };
	r[3] = (vec4_t){ 0, 0, 0, 1 };
}


static void mat4_inv(mat4_t r, const mat4_t M)
{
	float s[6];
	float c[6];
	s[0] = M[0][0]*M[1][1] - M[1][0]*M[0][1];
	s[1] = M[0][0]*M[1][2] - M[1][0]*M[0][2];
	s[2] = M[0][0]*M[1][3] - M[1][0]*M[0][3];
	s[3] = M[0][1]*M[1][2] - M[1][1]*M[0][2];
	s[4] = M[0][1]*M[1][3] - M[1][1]*M[0][3];
	s[5] = M[0][2]*M[1][3] - M[1][2]*M[0][3];

	c[0] = M[2][0]*M[3][1] - M[3][0]*M[2][1];
	c[1] = M[2][0]*M[3][2] - M[3][0]*M[2][2];
	c[2] = M[2][0]*M[3][3] - M[3][0]*M[2][3];
	c[3] = M[2][1]*M[3][2] - M[3][1]*M[2][2];
	c[4] = M[2][1]*M[3][3] - M[3][1]*M[2][3];
	c[5] = M[2][2]*M[3][3] - M[3][2]*M[2][3];

	/* Assumes it is invertible */
	float idet = 1.0f/( s[0]*c[5]-s[1]*c[4]+s[2]*c[3]+s[3]*c[2]-s[4]*c[1]+s[5]*c[0] );

	r[0][0] = ( M[1][1] * c[5] - M[1][2] * c[4] + M[1][3] * c[3]) * idet;
	r[0][1] = (-M[0][1] * c[5] + M[0][2] * c[4] - M[0][3] * c[3]) * idet;
	r[0][2] = ( M[3][1] * s[5] - M[3][2] * s[4] + M[3][3] * s[3]) * idet;
	r[0][3] = (-M[2][1] * s[5] + M[2][2] * s[4] - M[2][3] * s[3]) * idet;

	r[1][0] = (-M[1][0] * c[5] + M[1][2] * c[2] - M[1][3] * c[1]) * idet;
	r[1][1] = ( M[0][0] * c[5] - M[0][2] * c[2] + M[0][3] * c[1]) * idet;
	r[1][2] = (-M[3][0] * s[5] + M[3][2] * s[2] - M[3][3] * s[1]) * idet;
	r[1][3] = ( M[2][0] * s[5] - M[2][2] * s[2] + M[2][3] * s[1]) * idet;

	r[2][0] = ( M[1][0] * c[4] - M[1][1] * c[2] + M[1][3] * c[0]) * idet;
	r[2][1] = (-M[0][0] * c[4] + M[0][1] * c[2] - M[0][3] * c[0]) * idet;
	r[2][2] = ( M[3][0] * s[4] - M[3][1] * s[2] + M[3][3] * s[0]) * idet;
	r[2][3] = (-M[2][0] * s[4] + M[2][1] * s[2] - M[2][3] * s[0]) * idet;

	r[3][0] = (-M[1][0] * c[3] + M[1][1] * c[1] - M[1][2] * c[0]) * idet;
	r[3][1] = ( M[0][0] * c[3] - M[0][1] * c[1] + M[0][2] * c[0]) * idet;
	r[3][2] = (-M[3][0] * s[3] + M[3][1] * s[1] - M[3][2] * s[0]) * idet;
	r[3][3] = ( M[2][0] * s[3] - M[2][1] * s[1] + M[2][2] * s[0]) * idet;
}

static void mat4_I(mat4_t r)
{
	memset(r, 0, sizeof(mat4_t));
	for (int i = 0; i < 4; i++)
	{
		r[i][i] = 1;
	}
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

static inline float sphere(const vec4_t p, const vec4_t origin, float radius)
{
	vec4_t delta = origin - p;
	return magnitude(delta) - radius;
}

static inline float octahedron(vec4_t p, const vec4_t origin, float s)
{
	p = abs4(p - origin);
	return (p[0] + p[1] + p[2] - s) * 0.57735027f;
}

vec4_t sphere_center = { 0, 0, 5, 0 };
mat4_t world = {
	{ 1, 0, 0, 0 },
	{ 0, 1, 0, 0 },
	{ 0, 0, 1, 0 },
	{ 0, 0, 0, 1 }
};
mat4_t world_inv;

float scene(const vec4_t p)
{
	// vec4_t p_p = mat4_mul_vec4(world_inv, p);

	// return sphere(p, sphere_center, 1);
	return octahedron(p, sphere_center, 4);
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

	return unit4(normal);
}

static int update(void* userdata)
{
	PlaydateAPI* pd = userdata;
	static int count = 0;
	pd->graphics->clear(kColorBlack);

	vec4_t origins[LCD_ROWS * LCD_COLUMNS] = {};

	float t = count / 50.0;
	vec4_t light_dir = { cos(t), sin(t), 0, 0 };

	// memset(rb, 0, sizeof(rb));

	// mat4_I(world);
	// mat4_translate(world, (vec4_t){ 0, 0, -5, 0 });
	// mat4_inv(world_inv, world);

	for (int r = 0; r < LCD_ROWS; r++)
	{
		for (int c = 0; c < LCD_COLUMNS; c++)
		for (int step = 0; step < 10; step++)
		{
			int i = r * LCD_COLUMNS + c;
			float dist = scene(origins[i]);

			if (dist < 0.01f)
			{
				vec4_t n = numerical_normal(dist, origins[i], 0.01f);

				float ndl = dot4(n,light_dir);
				rb[r][c] = (uint8_t)(((ndl >= 0) * ndl) * 255);

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
			row[c>>3] |= ((rb[r][c] > (fast_rand())) << ((7-c) & 7));
		}
	}

	count++;
	pd->system->drawFPS(0,0);

	return 1;
}

