#include "dragonfail.h"
#include "termbox.h"

#include "inputs.h"
#include "utils.h"
#include "config.h"
#include "draw.h"
#include "animations.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__DragonFly__) || defined(__FreeBSD__)
	#include <sys/kbio.h>
#else // linux
	#include <linux/kd.h>
#endif

/****************************/
/* Define Animation presets */
/* for easy extendability   */
/****************************/

#define FUNC_PTR(X) (&X)
#define ANIMATION_NUM 2
// function prototypes are necessary
static void doom_free(struct term_buf* buf);
static void doom(struct term_buf* term_buf);
static void doom_init(struct term_buf* buf);

static void matrix_free(struct term_buf* buf);
static void matrix(struct term_buf* term_buf);
static void matrix_init(struct term_buf* buf);

// create arrays for each type of function
static const void* ANIM_INITS[] = {
    FUNC_PTR(doom_init),
    FUNC_PTR(matrix_init)
};
static const void* ANIM_RUNS[] = {
    FUNC_PTR(doom),
    FUNC_PTR(matrix)
};
static const void* ANIM_FREES[] = {
    FUNC_PTR(doom_free),
    FUNC_PTR(matrix_free)
};

#define DOOM_STEPS 13


static void doom_init(struct term_buf* buf)
{
	buf->init_width = buf->width;
	buf->init_height = buf->height;
	buf->astate = (struct doom_state*)malloc(sizeof(struct doom_state));

	if (buf->astate == NULL)
	{
		dgn_throw(DGN_ALLOC);
	}

	uint16_t tmp_len = buf->width * buf->height;
    // cast the doom state to a pointer to the structure
    struct doom_state* d = (struct doom_state*)buf->astate;
	d->buf = malloc(tmp_len);
	tmp_len -= buf->width;

	if (d->buf == NULL)
	{
		dgn_throw(DGN_ALLOC);
	}

	memset(d->buf, 0, tmp_len);
	memset(d->buf + tmp_len, DOOM_STEPS - 1, buf->width);
}

static void doom_free(struct term_buf* buf)
{
    struct doom_state* d = (struct doom_state*)buf->astate;
	free(d->buf);
	free(d);
}

// Adapted from cmatrix
static void matrix_init(struct term_buf* buf)
{
	buf->init_width = buf->width;
	buf->init_height = buf->height;
	buf->astate = (struct matrix_state*)malloc(sizeof(struct matrix_state));
	struct matrix_state* s = buf->astate;

	if (s == NULL)
	{
		dgn_throw(DGN_ALLOC);
	}

	uint16_t len = buf->height + 1;
	s->grid = malloc(sizeof(struct matrix_dot*) * len);

	if (s->grid == NULL)
	{
		dgn_throw(DGN_ALLOC);
	}

	len = (buf->height + 1) * buf->width;
	(s->grid)[0] = malloc(sizeof(struct matrix_dot) * len);

	if ((s->grid)[0] == NULL)
	{
		dgn_throw(DGN_ALLOC);
	}

	for (int i = 1; i <= buf->height; ++i)
	{
		s->grid[i] = s->grid[i - 1] + buf->width;

		if (s->grid[i] == NULL)
		{
			dgn_throw(DGN_ALLOC);
		}
	}

	s->length = malloc(buf->width * sizeof(int));

	if (s->length == NULL)
	{
		dgn_throw(DGN_ALLOC);
	}

	s->spaces = malloc(buf->width * sizeof(int));

	if (s->spaces == NULL)
	{
		dgn_throw(DGN_ALLOC);
	}

	s->updates = malloc(buf->width * sizeof(int));

	if (s->updates == NULL)
	{
		dgn_throw(DGN_ALLOC);
	}

	// Initialize grid
	for (int i = 0; i <= buf->height; ++i)
	{
		for (int j = 0; j <= buf->width - 1; j += 2)
		{
			s->grid[i][j].val = -1;
		}
	}

	for (int j = 0; j < buf->width; j += 2)
	{
		s->spaces[j] = (int) rand() % buf->height + 1;
		s->length[j] = (int) rand() % (buf->height - 3) + 3;
		s->grid[1][j].val = ' ';
		s->updates[j] = (int) rand() % 3 + 1;
	}
}

static void matrix_free(struct term_buf* buf)
{
	struct matrix_state* matrix = buf->astate;
	free(matrix->grid[0]);
	free(matrix->grid);
	free(matrix->length);
	free(matrix->spaces);
	free(matrix->updates);
	free(matrix);
}

static void doom(struct term_buf* term_buf)
{
	static struct tb_cell fire[DOOM_STEPS] =
	{
		{' ', 9, 0}, // default
		{0x2591, 2, 0}, // red
		{0x2592, 2, 0}, // red
		{0x2593, 2, 0}, // red
		{0x2588, 2, 0}, // red
		{0x2591, 4, 2}, // yellow
		{0x2592, 4, 2}, // yellow
		{0x2593, 4, 2}, // yellow
		{0x2588, 4, 2}, // yellow
		{0x2591, 8, 4}, // white
		{0x2592, 8, 4}, // white
		{0x2593, 8, 4}, // white
		{0x2588, 8, 4}, // white
	};

	uint16_t src;
	uint16_t random;
	uint16_t dst;

	uint16_t w = term_buf->init_width;
	uint8_t* tmp = ((struct doom_state*)term_buf->astate)->buf;

	if ((term_buf->width != term_buf->init_width) || (term_buf->height != term_buf->init_height))
	{
		return;
	}

	struct tb_cell* buf = tb_cell_buffer();

	for (uint16_t x = 0; x < w; ++x)
	{
		for (uint16_t y = 1; y < term_buf->init_height; ++y)
		{
			src = y * w + x;
			random = ((rand() % 7) & 3);
			dst = src - random + 1;

			if (w > dst)
			{
				dst = 0;
			}
			else
			{
				dst -= w;
			}

			tmp[dst] = tmp[src] - (random & 1);

			if (tmp[dst] > 12)
			{
				tmp[dst] = 0;
			}

			buf[dst] = fire[tmp[dst]];
			buf[src] = fire[tmp[src]];
		}
	}
}

// Adapted from cmatrix
static void matrix(struct term_buf* buf)
{
	static int frame = 3;
	const int frame_delay = 8;
	static int count = 0;
	bool first_col;
	struct matrix_state* s = buf->astate;

	// Allowed codepoints
	const int randmin = 33;
	const int randnum = 123 - randmin;
	// Chars change mid-scroll
	const bool changes = true;

	if ((buf->width != buf->init_width) || (buf->height != buf->init_height))
	{
		return;
	}

	count += 1;
	if (count > frame_delay)
    {
		frame += 1;
		if (frame > 4) frame = 1;
		count = 0;

		for (int j = 0; j < buf->width; j += 2)
		{
			int tail;
			if (frame > s->updates[j])
			{
				if (s->grid[0][j].val == -1 && s->grid[1][j].val == ' ')
				{
					if (s->spaces[j] > 0)
					{
						s->spaces[j]--;
					} else {
						s->length[j] = (int) rand() % (buf->height - 3) + 3;
						s->grid[0][j].val = (int) rand() % randnum + randmin;
						s->spaces[j] = (int) rand() % buf->height + 1;
					}
				}

				int i = 0, seg_len = 0;
				first_col = 1;
				while (i <= buf->height)
				{
					// Skip over spaces
					while (i <= buf->height
							&& (s->grid[i][j].val == ' ' || s->grid[i][j].val == -1))
					{
						i++;
					}

					if (i > buf->height) break;

					// Find the head of this col
					tail = i;
					seg_len = 0;
					while (i <= buf->height
							&& (s->grid[i][j].val != ' ' && s->grid[i][j].val != -1))
					{
						s->grid[i][j].is_head = false;
						if (changes)
						{
							if (rand() % 8 == 0)
								s->grid[i][j].val = (int) rand() % randnum + randmin;
						}
						i++;
						seg_len++;
					}

					// Head's down offscreen
					if (i > buf->height)
					{
						s->grid[tail][j].val = ' ';
						continue;
					}

					s->grid[i][j].val = (int) rand() % randnum + randmin;
					s->grid[i][j].is_head = true;

					if (seg_len > s->length[j] || !first_col) {
						s->grid[tail][j].val = ' ';
						s->grid[0][j].val = -1;
					}
					first_col = 0;
					i++;
				}
			}
		}
	}

	uint32_t blank;
	utf8_char_to_unicode(&blank, " ");

	for (int j = 0; j < buf->width; j += 2)
    {
		for (int i = 1; i <= buf->height; ++i)
		{
			uint32_t c;
			int fg = TB_GREEN;
			int bg = TB_DEFAULT;

			if (s->grid[i][j].val == -1 || s->grid[i][j].val == ' ')
			{
				tb_change_cell(j, i - 1, blank, fg, bg);
				continue;
			}

			char tmp[2];
			tmp[0] = s->grid[i][j].val;
			tmp[1] = '\0';
			if(utf8_char_to_unicode(&c, tmp))
			{
				if (s->grid[i][j].is_head)
				{
					fg = TB_WHITE | TB_BOLD;
				}
				tb_change_cell(j, i - 1, c, fg, bg);
			}
		}
	}
}




void animate_free(struct term_buf* buf)
{
	if (config.animate)
	{
        int i = config.animation;
        // make sure the animation chosen is valid
        if (i < ANIMATION_NUM)
        {
            // load appropriate free function and execute
            void (*fun_ptr)(struct term_buf*);
            fun_ptr = ANIM_FREES[i];
            (*fun_ptr)(buf);
        }
	}
}

void animate(struct term_buf* buf)
{
	buf->width = tb_width();
	buf->height = tb_height();

	if (config.animate)
	{
        int i = config.animation;
        // make sure the animation chosen is valid
        if (i < ANIMATION_NUM)
        {
            // runs corresponding animation function
            void (*fun_ptr)(struct term_buf*);
            fun_ptr = ANIM_RUNS[i];
            (*fun_ptr)(buf);
        }
	}
}

void animate_init(struct term_buf* buf)
{
	if (config.animate)
	{
        int i = config.animation;
        // make sure the animation chosen is valid
        if (i < ANIMATION_NUM)
        {
            // grab the corresponding animation
            // initialize function and execute
            void (*fun_ptr)(struct term_buf*);
            fun_ptr = ANIM_INITS[i];
            (*fun_ptr)(buf);
        }
	}
}



