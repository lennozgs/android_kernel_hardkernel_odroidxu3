/*
 * Realtek RTL2832U SDR driver
 *
 * Copyright (C) 2013 Antti Palosaari <crope@iki.fi>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc.,
 *    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * GNU Radio plugin "gr-kernel" for device usage will be on:
 * http://git.linuxtv.org/anttip/gr-kernel.git
 *
 */

#ifndef RTL2832_SDR_H
#define RTL2832_SDR_H

#include <linux/i2c.h>
#include <media/v4l2-subdev.h>
#include "dvb_frontend.h"

struct rtl2832_sdr_platform_data {
	/*
	 * Clock frequency.
	 * Hz
	 * 4000000, 16000000, 25000000, 28800000
	 */
	u32 clk;

	/*
	 * Tuner.
	 * XXX: This list must be kept sync with dvb_usb_rtl28xxu USB IF driver.
	 */
#define RTL2832_SDR_TUNER_TUA9001   0x24
#define RTL2832_SDR_TUNER_FC0012    0x26
#define RTL2832_SDR_TUNER_E4000     0x27
#define RTL2832_SDR_TUNER_FC0013    0x29
#define RTL2832_SDR_TUNER_R820T     0x2a
#define RTL2832_SDR_TUNER_R828D     0x2b
	u8 tuner;

	struct i2c_client *i2c_client;
	int (*bulk_read)(struct i2c_client *, unsigned int, void *, size_t);
	int (*bulk_write)(struct i2c_client *, unsigned int, const void *, size_t);
	int (*update_bits)(struct i2c_client *, unsigned int, unsigned int, unsigned int);
	struct dvb_frontend *dvb_frontend;
	struct v4l2_subdev *v4l2_subdev;
	struct dvb_usb_device *dvb_usb_device;
};

#endif /* RTL2832_SDR_H */
