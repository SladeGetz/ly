#include "dragonfail.h"
#include "termbox.h"

#include "inputs.h"
#include "utils.h"
#include "config.h"
#include "draw.h"

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

void draw_init(struct term_buf* buf)
{
	buf->width = tb_width();
	buf->height = tb_height();
	hostname(&buf->info_line);

	uint16_t len_login = strlen(lang.login);
	uint16_t len_password = strlen(lang.password);

	if (len_login > len_password)
	{
		buf->labels_max_len = len_login;
	}
	else
	{
		buf->labels_max_len = len_password;
	}

	buf->box_height = 7 + (2 * config.margin_box_v);
	buf->box_width =
		(2 * config.margin_box_h)
		+ (config.input_len + 1)
		+ buf->labels_max_len;

#if defined(__linux__) || defined(__FreeBSD__)
	buf->box_chars.left_up = 0x250c;
	buf->box_chars.left_down = 0x2514;
	buf->box_chars.right_up = 0x2510;
	buf->box_chars.right_down = 0x2518;
	buf->box_chars.top = 0x2500;
	buf->box_chars.bot = 0x2500;
	buf->box_chars.left = 0x2502;
	buf->box_chars.right = 0x2502;
#else
	buf->box_chars.left_up = '+';
	buf->box_chars.left_down = '+';
	buf->box_chars.right_up = '+';
	buf->box_chars.right_down= '+';
	buf->box_chars.top = '-';
	buf->box_chars.bot = '-';
	buf->box_chars.left = '|';
	buf->box_chars.right = '|';
#endif
}


void draw_box(struct term_buf* buf)
{
	uint16_t box_x = (buf->width - buf->box_width) / 2;
	uint16_t box_y = (buf->height - buf->box_height) / 2;
	uint16_t box_x2 = (buf->width + buf->box_width) / 2;
	uint16_t box_y2 = (buf->height + buf->box_height) / 2;
	buf->box_x = box_x;
	buf->box_y = box_y;

	if (!config.hide_borders)
	{
		// corners
		tb_change_cell(
			box_x - 1,
			box_y - 1,
			buf->box_chars.left_up,
			config.fg,
			config.bg);
		tb_change_cell(
			box_x2,
			box_y - 1,
			buf->box_chars.right_up,
			config.fg,
			config.bg);
		tb_change_cell(
			box_x - 1,
			box_y2,
			buf->box_chars.left_down,
			config.fg,
			config.bg);
		tb_change_cell(
			box_x2,
			box_y2,
			buf->box_chars.right_down,
			config.fg,
			config.bg);

		// top and bottom
		struct tb_cell c1 = {buf->box_chars.top, config.fg, config.bg};
		struct tb_cell c2 = {buf->box_chars.bot, config.fg, config.bg};

		for (uint16_t i = 0; i < buf->box_width; ++i)
		{
			tb_put_cell(
				box_x + i,
				box_y - 1,
				&c1);
			tb_put_cell(
				box_x + i,
				box_y2,
				&c2);
		}

		// left and right
		c1.ch = buf->box_chars.left;
		c2.ch = buf->box_chars.right;

		for (uint16_t i = 0; i < buf->box_height; ++i)
		{
			tb_put_cell(
				box_x - 1,
				box_y + i,
				&c1);

			tb_put_cell(
				box_x2,
				box_y + i,
				&c2);
		}
	}

	if (config.blank_box)
	{
		struct tb_cell blank = {' ', config.fg, config.bg};

		for (uint16_t i = 0; i < buf->box_height; ++i)
		{
			for (uint16_t k = 0; k < buf->box_width; ++k)
			{
				tb_put_cell(
					box_x + k,
					box_y + i,
					&blank);
			}
		}
	}
}

struct tb_cell* strn_cell(char* s, uint16_t len) // throws
{
	struct tb_cell* cells = malloc((sizeof (struct tb_cell)) * len);
	char* s2 = s;
	uint32_t c;

	if (cells != NULL)
	{
		for (uint16_t i = 0; i < len; ++i)
		{
			if ((s2 - s) >= len)
			{
				break;
			}

			s2 += utf8_char_to_unicode(&c, s2);

			cells[i].ch = c;
			cells[i].bg = config.bg;
			cells[i].fg = config.fg;
		}
	}
	else
	{
		dgn_throw(DGN_ALLOC);
	}

	return cells;
}

struct tb_cell* str_cell(char* s) // throws
{
	return strn_cell(s, strlen(s));
}

void draw_labels(struct term_buf* buf) // throws
{
	// login text
	struct tb_cell* login = str_cell(lang.login);

	if (dgn_catch())
	{
		dgn_reset();
	}
	else
	{
		tb_blit(
			buf->box_x + config.margin_box_h,
			buf->box_y + config.margin_box_v + 4,
			strlen(lang.login),
			1,
			login);
		free(login);
	}

	// password text
	struct tb_cell* password = str_cell(lang.password);

	if (dgn_catch())
	{
		dgn_reset();
	}
	else
	{
		tb_blit(
			buf->box_x + config.margin_box_h,
			buf->box_y + config.margin_box_v + 6,
			strlen(lang.password),
			1,
			password);
		free(password);
	}

	if (buf->info_line != NULL)
	{
		uint16_t len = strlen(buf->info_line);
		struct tb_cell* info_cell = str_cell(buf->info_line);

		if (dgn_catch())
		{
			dgn_reset();
		}
		else
		{
			tb_blit(
				buf->box_x + ((buf->box_width - len) / 2),
				buf->box_y + config.margin_box_v,
				len,
				1,
				info_cell);
			free(info_cell);
		}
	}
}

void draw_f_commands()
{
	struct tb_cell* f1 = str_cell(lang.f1);

	if (dgn_catch())
	{
		dgn_reset();
	}
	else
	{
		tb_blit(0, 0, strlen(lang.f1), 1, f1);
		free(f1);
	}

	struct tb_cell* f2 = str_cell(lang.f2);

	if (dgn_catch())
	{
		dgn_reset();
	}
	else
	{
		tb_blit(strlen(lang.f1) + 1, 0, strlen(lang.f2), 1, f2);
		free(f2);
	}
}

void draw_lock_state(struct term_buf* buf)
{
	// get values
	int fd = open(config.console_dev, O_RDONLY);

	if (fd < 0)
	{
		buf->info_line = lang.err_console_dev;
		return;
	}

	bool numlock_on;
	bool capslock_on;

#if defined(__DragonFly__) || defined(__FreeBSD__)
	int led;
	ioctl(fd, KDGETLED, &led);
	numlock_on = led & LED_NUM;
	capslock_on = led & LED_CAP;
#else // linux
	char led;
	ioctl(fd, KDGKBLED, &led);
	numlock_on = led & K_NUMLOCK;
	capslock_on = led & K_CAPSLOCK;
#endif

	close(fd);

	// print text
	uint16_t pos_x = buf->width - strlen(lang.numlock);

	if (numlock_on)
	{
		struct tb_cell* numlock = str_cell(lang.numlock);

		if (dgn_catch())
		{
			dgn_reset();
		}
		else
		{
			tb_blit(pos_x, 0, strlen(lang.numlock), 1, numlock);
			free(numlock);
		}
	}

	pos_x -= strlen(lang.capslock) + 1;

	if (capslock_on)
	{
		struct tb_cell* capslock = str_cell(lang.capslock);

		if (dgn_catch())
		{
			dgn_reset();
		}
		else
		{
			tb_blit(pos_x, 0, strlen(lang.capslock), 1, capslock);
			free(capslock);
		}
	}
}

void draw_desktop(struct desktop* target)
{
	uint16_t len = strlen(target->list[target->cur]);

	if (len > (target->visible_len - 3))
	{
		len = target->visible_len - 3;
	}

	tb_change_cell(
		target->x,
		target->y,
		'<',
		config.fg,
		config.bg);

	tb_change_cell(
		target->x + target->visible_len - 1,
		target->y,
		'>',
		config.fg,
		config.bg);

	for (uint16_t i = 0; i < len; ++ i)
	{
		tb_change_cell(
			target->x + i + 2,
			target->y,
			target->list[target->cur][i],
			config.fg,
			config.bg);
	}
}

void draw_input(struct text* input)
{
	uint16_t len = strlen(input->text);
	uint16_t visible_len = input->visible_len;

	if (len > visible_len)
	{
		len = visible_len;
	}

	struct tb_cell* cells = strn_cell(input->visible_start, len);

	if (dgn_catch())
	{
		dgn_reset();
	}
	else
	{
		tb_blit(input->x, input->y, len, 1, cells);
		free(cells);

		struct tb_cell c1 = {' ', config.fg, config.bg};

		for (uint16_t i = input->end - input->visible_start; i < visible_len; ++i)
		{
			tb_put_cell(
				input->x + i,
				input->y,
				&c1);
		}
	}
}

void draw_input_mask(struct text* input)
{
	uint16_t len = strlen(input->text);
	uint16_t visible_len = input->visible_len;

	if (len > visible_len)
	{
		len = visible_len;
	}

	struct tb_cell c1 = {config.asterisk, config.fg, config.bg};
	struct tb_cell c2 = {' ', config.fg, config.bg};

	for (uint16_t i = 0; i < visible_len; ++i)
	{
		if (input->visible_start + i < input->end)
		{
			tb_put_cell(
				input->x + i,
				input->y,
				&c1);
		}
		else
		{
			tb_put_cell(
				input->x + i,
				input->y,
				&c2);
		}
	}
}

void position_input(
	struct term_buf* buf,
	struct desktop* desktop,
	struct text* login,
	struct text* password)
{
	uint16_t x = buf->box_x + config.margin_box_h + buf->labels_max_len + 1;
	int32_t len = buf->box_x + buf->box_width - config.margin_box_h - x;

	if (len < 0)
	{
		return;
	}

	desktop->x = x;
	desktop->y = buf->box_y + config.margin_box_v + 2;
	desktop->visible_len = len;

	login->x = x;
	login->y = buf->box_y + config.margin_box_v + 4;
	login->visible_len = len;

	password->x = x;
	password->y = buf->box_y + config.margin_box_v + 6;
	password->visible_len = len;
}


bool cascade(struct term_buf* term_buf, uint8_t* fails)
{
	uint16_t width = term_buf->width;
	uint16_t height = term_buf->height;

	struct tb_cell* buf = tb_cell_buffer();
	bool changes = false;
	char c_under;
	char c;

	for (int i = height - 2; i >= 0; --i)
	{
		for (int k = 0; k < width; ++k)
		{
			c = buf[i * width + k].ch;

			if (isspace(c))
			{
				continue;
			}

			c_under = buf[(i + 1) * width + k].ch;
			
			if (!isspace(c_under))
			{
				continue;
			}

			if (!changes)
			{
				changes = true;
			}

			if ((rand() % 10) > 7)
			{
				continue;
			}

			buf[(i + 1) * width + k] = buf[i * width + k];
			buf[i * width + k].ch = ' ';
		}
	}

	// stop force-updating 
	if (!changes)
	{
		sleep(7);
		*fails = 0;

		return false;
	}

	// force-update
	return true;
}
