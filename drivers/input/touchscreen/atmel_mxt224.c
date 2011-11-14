/*
 * atmel_mxt224.c - Touchscreen driver for Atmel mXT224
 *
 * Copyright (C) 2010 Texas Instruments
 * Alejandro Jimenez <alexjimenez@ti.com>
 *
 * Description:	Atmel maXTouch 224-channel Touchscreen Sensor IC driver,
 *		supporting this processor:
 *			- Texas Instruments OMAP4
 *		and this sensor controller:
 *			- AT42Q602240 Version 22
 *
 * Based on:	qt602240_ts.c - AT42QT602240/ATMXT224 Touchscreen driver
 *		Copyright (C) 2010 Samsung Electronics Co.Ltd
 *		Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *		Version: 5
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/i2c/atmel_mxt224.h>
#include <linux/input/mt.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/gpio.h>

/* Version */
#define QT602240_VER_22                        22

/* Registers */
#define QT602240_FAMILY_ID		0x00
#define QT602240_VARIANT_ID		0x01
#define QT602240_VERSION		0x02
#define QT602240_BUILD			0x03
#define QT602240_MATRIX_X_SIZE		0x04
#define QT602240_MATRIX_Y_SIZE		0x05
#define QT602240_OBJECT_NUM		0x06
#define QT602240_OBJECT_START		0x07

#define QT602240_OBJECT_SIZE		6

/* Object types */
#define QT602240_DEBUG_DIAGNOSTIC	37
#define QT602240_GEN_MESSAGE		5
#define QT602240_GEN_COMMAND		6
#define QT602240_GEN_POWER		7
#define QT602240_GEN_ACQUIRE		8
#define QT602240_TOUCH_MULTI		9
#define QT602240_TOUCH_KEYARRAY		15
#define QT602240_TOUCH_PROXIMITY	23
#define QT602240_PROCI_GRIPFACE		20
#define QT602240_PROCG_NOISE		22
#define QT602240_PROCI_ONETOUCH		24
#define QT602240_PROCI_TWOTOUCH		27
#define QT602240_SPT_COMMSCONFIG	18
#define QT602240_SPT_GPIOPWM		19
#define QT602240_SPT_SELFTEST		25
#define QT602240_SPT_CTECONFIG		28
#define QT602240_SPT_USERDATA		38

/* QT602240_GEN_COMMAND field */
#define QT602240_COMMAND_RESET		0
#define QT602240_COMMAND_BACKUPNV	1
#define QT602240_COMMAND_CALIBRATE	2
#define QT602240_COMMAND_REPORTALL	3
#define QT602240_COMMAND_DIAGNOSTIC	5

/* QT602240_GEN_POWER field */
#define QT602240_POWER_IDLEACQINT	0
#define QT602240_POWER_ACTVACQINT	1
#define QT602240_POWER_ACTV2IDLETO	2

/* QT602240_GEN_ACQUIRE field */
#define QT602240_ACQUIRE_CHRGTIME	0
#define QT602240_ACQUIRE_TCHDRIFT	2
#define QT602240_ACQUIRE_DRIFTST	3
#define QT602240_ACQUIRE_TCHAUTOCAL	4
#define QT602240_ACQUIRE_SYNC		5
#define QT602240_ACQUIRE_ATCHCALST	6
#define QT602240_ACQUIRE_ATCHCALSTHR	7

/* QT602240_TOUCH_MULTI field */
#define QT602240_TOUCH_CTRL		0
#define QT602240_TOUCH_XORIGIN		1
#define QT602240_TOUCH_YORIGIN		2
#define QT602240_TOUCH_XSIZE		3
#define QT602240_TOUCH_YSIZE		4
#define QT602240_TOUCH_BLEN		6
#define QT602240_TOUCH_TCHTHR		7
#define QT602240_TOUCH_TCHDI		8
#define QT602240_TOUCH_ORIENT		9
#define QT602240_TOUCH_MOVHYSTI		11
#define QT602240_TOUCH_MOVHYSTN		12
#define QT602240_TOUCH_NUMTOUCH		14
#define QT602240_TOUCH_MRGHYST		15
#define QT602240_TOUCH_MRGTHR		16
#define QT602240_TOUCH_AMPHYST		17
#define QT602240_TOUCH_XRANGE_LSB	18
#define QT602240_TOUCH_XRANGE_MSB	19
#define QT602240_TOUCH_YRANGE_LSB	20
#define QT602240_TOUCH_YRANGE_MSB	21
#define QT602240_TOUCH_XLOCLIP		22
#define QT602240_TOUCH_XHICLIP		23
#define QT602240_TOUCH_YLOCLIP		24
#define QT602240_TOUCH_YHICLIP		25
#define QT602240_TOUCH_XEDGECTRL	26
#define QT602240_TOUCH_XEDGEDIST	27
#define QT602240_TOUCH_YEDGECTRL	28
#define QT602240_TOUCH_YEDGEDIST	29
#define QT602240_TOUCH_JUMPLIMIT	30

/* QT602240_PROCI_GRIPFACE field */
#define QT602240_GRIPFACE_CTRL		0
#define QT602240_GRIPFACE_XLOGRIP	1
#define QT602240_GRIPFACE_XHIGRIP	2
#define QT602240_GRIPFACE_YLOGRIP	3
#define QT602240_GRIPFACE_YHIGRIP	4
#define QT602240_GRIPFACE_MAXTCHS	5
#define QT602240_GRIPFACE_SZTHR1	7
#define QT602240_GRIPFACE_SZTHR2	8
#define QT602240_GRIPFACE_SHPTHR1	9
#define QT602240_GRIPFACE_SHPTHR2	10
#define QT602240_GRIPFACE_SUPEXTTO	11

/* QT602240_PROCI_NOISE field */
#define QT602240_NOISE_CTRL		0
#define QT602240_NOISE_OUTFLEN		1
#define QT602240_NOISE_GCAFUL_LSB	3
#define QT602240_NOISE_GCAFUL_MSB	4
#define QT602240_NOISE_GCAFLL_LSB	5
#define QT602240_NOISE_GCAFLL_MSB	6
#define QT602240_NOISE_ACTVGCAFVALID	7
#define QT602240_NOISE_NOISETHR		8
#define QT602240_NOISE_FREQHOPSCALE	10
#define QT602240_NOISE_FREQ0		11
#define QT602240_NOISE_FREQ1		12
#define QT602240_NOISE_FREQ2		13
#define QT602240_NOISE_FREQ3		14
#define QT602240_NOISE_FREQ4		15
#define QT602240_NOISE_IDLEGCAFVALID	16

/* QT602240_SPT_COMMSCONFIG */
#define QT602240_COMMS_CTRL		0
#define QT602240_COMMS_CMD		1

/* QT602240_SPT_CTECONFIG field */
#define QT602240_CTE_CTRL		0
#define QT602240_CTE_CMD		1
#define QT602240_CTE_MODE		2
#define QT602240_CTE_IDLEGCAFDEPTH	3
#define QT602240_CTE_ACTVGCAFDEPTH	4
#define QT602240_CTE_VOLTAGE		5

#define QT602240_VOLTAGE_DEFAULT	2700000
#define QT602240_VOLTAGE_STEP		10000

/* Define for QT602240_GEN_COMMAND */
#define QT602240_RESET_TIME		65      /* msec */

/* Touch status */
#define QT602240_SUPPRESS		(1 << 1)
#define QT602240_AMP			(1 << 2)
#define QT602240_VECTOR			(1 << 3)
#define QT602240_MOVE			(1 << 4)
#define QT602240_RELEASE		(1 << 5)
#define QT602240_PRESS			(1 << 6)
#define QT602240_DETECT			(1 << 7)

/* Touchscreen absolute values */
#define QT602240_MAX_XC			1024
#define QT602240_MAX_YC			768
#define QT602240_MAX_AREA		0xff

#define QT602240_MAX_FINGER		10

static const u8 init_vals_ver_22[] = {
	/* QT602240_GEN_COMMAND(6) */
	0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	/* QT602240_GEN_POWER(7) */
	0x58, 0x58, 0x32,
	/* QT602240_GEN_ACQUIRE(8) */
	0x0a, 0x05, 0x14, 0x14, 0x0a, 0x00, 0x09, 0x23,
	/* QT602240_TOUCH_MULTI(9) */
	0x83, 0x00, 0x00, 0x11, 0x0d, 0x00, 0x01, 0x64, 0x02, 0x00,
	0x00, 0x01, 0x01, 0x00, 0x01, 0x0a, 0x0a, 0x0a, 0xff, 0x03,
	0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00,
	/* QT602240_TOUCH_KEYARRAY(15) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00,
	/* QT602240_SPT_GPIOPWM(19) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* QT602240_PROCI_GRIPFACE(20) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x28, 0x04,
	0x0f, 0x0a,
	/* QT602240_PROCG_NOISE(22) */
	0x07, 0x00, 0x00, 0x0a, 0x00, 0xf6, 0xff, 0x04, 0x08, 0x00,
	0x01, 0x0a, 0x0f, 0x14, 0x19, 0x1e, 0x04,
	/* QT602240_TOUCH_PROXIMITY(23) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* QT602240_PROCI_ONETOUCH(24) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* QT602240_SPT_SELFTEST(25) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	/* QT602240_PROCI_TWOTOUCH(27) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* QT602240_SPT_CTECONFIG(28) */
	0x00, 0x00, 0x01, 0x04, 0x08, 0x00,
};

struct atmel_mxt224_info {
	u8 family_id;
	u8 variant_id;
	u8 version;
	u8 build;
	u8 matrix_xsize;
	u8 matrix_ysize;
	u8 object_num;
};

struct atmel_mxt224_object {
	u8 type;
	u16 start_address;
	u8 size;
	u8 instances;
	u8 num_report_ids;

	/* to map object and message */
	u8 max_reportid;
};

struct atmel_mxt224_message {
	u8 reportid;
	u8 message[7];
	u8 checksum;
};

struct atmel_mxt224_finger {
	int status;
	int x;
	int y;
	int area;
};

/* Each client has this additional data */
struct atmel_mxt224_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct atmel_mxt224_platform_data *pdata;
	struct atmel_mxt224_object *object_table;
	struct atmel_mxt224_info info;
	struct atmel_mxt224_finger finger[QT602240_MAX_FINGER];
	unsigned int irq;
};

static int atmel_mxt224_object_readable(unsigned int type)
{
	switch (type) {
	case QT602240_GEN_MESSAGE:
	case QT602240_GEN_COMMAND:
	case QT602240_GEN_POWER:
	case QT602240_GEN_ACQUIRE:
	case QT602240_TOUCH_MULTI:
	case QT602240_TOUCH_KEYARRAY:
	case QT602240_TOUCH_PROXIMITY:
	case QT602240_PROCI_GRIPFACE:
	case QT602240_PROCG_NOISE:
	case QT602240_PROCI_ONETOUCH:
	case QT602240_PROCI_TWOTOUCH:
	case QT602240_SPT_COMMSCONFIG:
	case QT602240_SPT_GPIOPWM:
	case QT602240_SPT_SELFTEST:
	case QT602240_SPT_CTECONFIG:
	case QT602240_SPT_USERDATA:
		return 1;
	default:
		return 0;
	}
}

static int atmel_mxt224_object_writable(unsigned int type)
{
	switch (type) {
	case QT602240_GEN_COMMAND:
	case QT602240_GEN_POWER:
	case QT602240_GEN_ACQUIRE:
	case QT602240_TOUCH_MULTI:
	case QT602240_TOUCH_KEYARRAY:
	case QT602240_TOUCH_PROXIMITY:
	case QT602240_PROCI_GRIPFACE:
	case QT602240_PROCG_NOISE:
	case QT602240_PROCI_ONETOUCH:
	case QT602240_PROCI_TWOTOUCH:
	case QT602240_SPT_GPIOPWM:
	case QT602240_SPT_SELFTEST:
	case QT602240_SPT_CTECONFIG:
		return 1;
	default:
		return 0;
	}
}

static void atmel_mxt224_dump_message(struct device *dev,
		struct atmel_mxt224_message *message)
{
	dev_dbg(dev, "reportid:\t0x%x\n", message->reportid);
	dev_dbg(dev, "message1:\t0x%x\n", message->message[0]);
	dev_dbg(dev, "message2:\t0x%x\n", message->message[1]);
	dev_dbg(dev, "message3:\t0x%x\n", message->message[2]);
	dev_dbg(dev, "message4:\t0x%x\n", message->message[3]);
	dev_dbg(dev, "message5:\t0x%x\n", message->message[4]);
	dev_dbg(dev, "message6:\t0x%x\n", message->message[5]);
	dev_dbg(dev, "message7:\t0x%x\n", message->message[6]);
	dev_dbg(dev, "checksum:\t0x%x\n", message->checksum);
}

static int __atmel_mxt224_read_reg(struct i2c_client *client, u16 reg, u16 len,
		u8 *val)
{
	struct i2c_msg xfer[2];
	u8 buf[2];

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 2;
	xfer[0].buf = buf;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = val;

	if (i2c_transfer(client->adapter, xfer, 2) != 2) {
		dev_err(&client->dev, "%s: i2c transfer failed\n", __func__);
		return -EIO;
	}

	return 0;
}

static int atmel_mxt224_read_reg(struct i2c_client *client, u16 reg, u8 *val)
{
	return __atmel_mxt224_read_reg(client, reg, 1, val);
}

static int atmel_mxt224_write_reg(struct i2c_client *client, u16 reg, u8 val)
{
	u8 buf[3];

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;
	buf[2] = val;

	if (i2c_master_send(client, buf, 3) != 3) {
		dev_err(&client->dev, "%s: i2c send failed\n", __func__);
		return -EIO;
	}

	return 0;
}

static int atmel_mxt224_read_object_table(struct i2c_client *client, u16 reg,
		u8 *object_buf)
{
	return __atmel_mxt224_read_reg(client, reg, QT602240_OBJECT_SIZE,
			object_buf);
}

static struct atmel_mxt224_object *
atmel_mxt224_get_object(struct atmel_mxt224_data *data, u8 type)
{
	struct atmel_mxt224_object *object;
	int i;

	for (i = 0; i < data->info.object_num; i++) {
		object = data->object_table + i;
		if (object->type == type)
			return object;
	}

	dev_err(&data->client->dev, "Invalid object type\n");
	return NULL;
}

static int atmel_mxt224_read_message(struct atmel_mxt224_data *data,
		struct atmel_mxt224_message *message)
{
	struct atmel_mxt224_object *object;
	u16 reg;

	object = atmel_mxt224_get_object(data, QT602240_GEN_MESSAGE);
	if (!object)
		return -EINVAL;

	reg = object->start_address;
	return __atmel_mxt224_read_reg(data->client, reg,
		sizeof(struct atmel_mxt224_message), (u8 *)message);
}

static int atmel_mxt224_read_object(struct atmel_mxt224_data *data,
		u8 type, u8 offset, u8 *val)
{
	struct atmel_mxt224_object *object;
	u16 reg;

	object = atmel_mxt224_get_object(data, type);
	if (!object)
		return -EINVAL;

	reg = object->start_address;
	return __atmel_mxt224_read_reg(data->client, reg + offset, 1, val);
}
static int atmel_mxt224_write_object(struct atmel_mxt224_data *data,
			u8 type, u8 offset, u8 val)
{
	struct atmel_mxt224_object *object;
	u16 reg;

	object = atmel_mxt224_get_object(data, type);
	if (!object)
		return -EINVAL;

	reg = object->start_address;
	return atmel_mxt224_write_reg(data->client, reg + offset, val);
}

static void atmel_mxt224_input_report(struct atmel_mxt224_data *data,
			int single_id)
{
	struct atmel_mxt224_finger *finger = data->finger;
	struct input_dev *input_dev = data->input_dev;
	int status = finger[single_id].status;
	int finger_num = 0;
	int id;

	for (id = 0; id < QT602240_MAX_FINGER; id++) {
		if (!finger[id].status)
			continue;

		input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
				finger[id].status != QT602240_RELEASE ?
				finger[id].area : 0);
		input_report_abs(input_dev, ABS_MT_POSITION_X,
				finger[id].x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y,
				finger[id].y);
		input_mt_sync(input_dev);

		if (finger[id].status == QT602240_RELEASE)
			finger[id].status = 0;
		else
			finger_num++;
	}

	input_report_key(input_dev, BTN_TOUCH, finger_num > 0);

	if (status != QT602240_RELEASE) {
		input_report_abs(input_dev, ABS_X, finger[single_id].x);
		input_report_abs(input_dev, ABS_Y, finger[single_id].y);
	}

	input_sync(input_dev);
}

static void atmel_mxt224_input_touchevent(struct atmel_mxt224_data *data,
		struct atmel_mxt224_message *message, int id)
{
	struct atmel_mxt224_finger *finger = data->finger;
	struct device *dev = &data->client->dev;
	u8 status = message->message[0];
	int x;
	int y;
	int area;

	/* Check the touch is present on the screen */
	if (!(status & QT602240_DETECT)) {
		if (status & QT602240_RELEASE) {
			dev_dbg(dev, "[%d] released\n", id);

			finger[id].status = QT602240_RELEASE;
			atmel_mxt224_input_report(data, id);
		}
		return;
	}

	/* Check only AMP detection */
	if (!(status & (QT602240_PRESS | QT602240_MOVE)))
		return;

	x = (message->message[1] << 2) | ((message->message[3] & ~0x3f) >> 6);
	y = (message->message[2] << 2) | ((message->message[3] & ~0xf3) >> 2);
	area = message->message[4];

	dev_dbg(dev, "[%d] %s x: %d, y: %d, area: %d\n", id,
			status & QT602240_MOVE ? "moved" : "pressed",
			x, y, area);

	finger[id].status = status & QT602240_MOVE ?
		QT602240_MOVE : QT602240_PRESS;
	finger[id].x = x;
	finger[id].y = y;
	finger[id].area = area;

	atmel_mxt224_input_report(data, id);
}

static irqreturn_t atmel_mxt224_interrupt(int irq, void *dev_id)
{
	struct atmel_mxt224_data *data = dev_id;
	struct atmel_mxt224_message message;
	struct atmel_mxt224_object *object;
	struct device *dev = &data->client->dev;
	int id;
	u8 reportid;
	u8 max_reportid;
	u8 min_reportid;

	do {
		if (atmel_mxt224_read_message(data, &message)) {
			dev_err(dev, "Failed to read message\n");
			goto end;
		}

		reportid = message.reportid;

		/* whether reportid is thing of QT602240_TOUCH_MULTI */
		object = atmel_mxt224_get_object(data, QT602240_TOUCH_MULTI);
		if (!object)
			goto end;

		max_reportid = object->max_reportid;
		min_reportid = max_reportid - object->num_report_ids + 1;
		id = reportid - min_reportid;

		if ((reportid >= min_reportid) && (reportid <= max_reportid))
			atmel_mxt224_input_touchevent(data, &message, id);
		else
			atmel_mxt224_dump_message(dev, &message);
	} while (reportid != 0xff);

end:
	return IRQ_HANDLED;
}

static int atmel_mxt224_check_reg_init(struct atmel_mxt224_data *data)
{
	struct atmel_mxt224_object *object;
	struct device *dev = &data->client->dev;
	int index = 0;
	int i, j;
	u8 version = data->info.version;
	u8 *init_vals;

	switch (version) {
	case QT602240_VER_22:
		init_vals = (u8 *)init_vals_ver_22;
		break;
	default:
		dev_err(dev, "Firmware version %d doesn't support\n", version);
		return -EINVAL;
	}

	for (i = 0; i < data->info.object_num; i++) {
		object = data->object_table + i;

		if (!atmel_mxt224_object_writable(object->type))
			continue;

		for (j = 0; j < object->size + 1; j++)
			atmel_mxt224_write_object(data, object->type, j,
					init_vals[index + j]);

		index += object->size + 1;
	}

	return 0;
}

static int atmel_mxt224_check_matrix_size(struct atmel_mxt224_data *data)
{
	const struct atmel_mxt224_platform_data *pdata = data->pdata;
	struct device *dev = &data->client->dev;
	int mode = -1;
	int ret;
	u8 val;

	dev_dbg(dev, "Number of X lines: %d\n", pdata->x_line);
	dev_dbg(dev, "Number of Y lines: %d\n", pdata->y_line);

	switch (pdata->x_line) {
	case 0 ... 15:
		if (pdata->y_line <= 14)
			mode = 0;
		break;
	case 16:
		if (pdata->y_line <= 12)
			mode = 1;
		if (pdata->y_line == 13 || pdata->y_line == 14)
			mode = 0;
		break;
	case 17:
		if (pdata->y_line <= 11)
			mode = 2;
		if (pdata->y_line == 12 || pdata->y_line == 13)
			mode = 1;
		break;
	case 18:
		if (pdata->y_line <= 10)
			mode = 3;
		if (pdata->y_line == 11 || pdata->y_line == 12)
			mode = 2;
		break;
	case 19:
		if (pdata->y_line <= 9)
			mode = 4;
		if (pdata->y_line == 10 || pdata->y_line == 11)
			mode = 3;
		break;
	case 20:
		mode = 4;
	}

	if (mode < 0) {
		dev_err(dev, "Invalid X/Y lines\n");
		return -EINVAL;
	}

	ret = atmel_mxt224_read_object(data, QT602240_SPT_CTECONFIG,
			QT602240_CTE_MODE, &val);
	if (ret)
		return ret;

	if (mode == val)
		return 0;

	/* Change the CTE configuration */
	atmel_mxt224_write_object(data, QT602240_SPT_CTECONFIG,
			QT602240_CTE_CTRL, 1);
	atmel_mxt224_write_object(data, QT602240_SPT_CTECONFIG,
			QT602240_CTE_MODE, mode);
	atmel_mxt224_write_object(data, QT602240_SPT_CTECONFIG,
			QT602240_CTE_CTRL, 0);

	return 0;
}

static int atmel_mxt224_make_highchg(struct atmel_mxt224_data *data)
{
	struct device *dev = &data->client->dev;
	int count = 10;
	int ret;
	u8 val;

	/* Read dummy message to make high CHG pin */
	do {
		ret = atmel_mxt224_read_object(data,
			QT602240_GEN_MESSAGE, 0, &val);
		if (ret)
			return ret;
	} while ((val != 0xff) && --count);

	if (!count) {
		dev_err(dev, "CHG pin isn't cleared\n");
		return -EBUSY;
	}

	return 0;
}

static void atmel_mxt224_handle_pdata(struct atmel_mxt224_data *data)
{
	const struct atmel_mxt224_platform_data *pdata = data->pdata;
	u8 voltage;

	/* Set touchscreen lines */
	atmel_mxt224_write_object(data, QT602240_TOUCH_MULTI,
			QT602240_TOUCH_XSIZE, pdata->x_line);
	atmel_mxt224_write_object(data, QT602240_TOUCH_MULTI,
			QT602240_TOUCH_YSIZE, pdata->y_line);

	/* Set touchscreen orient */
	atmel_mxt224_write_object(data, QT602240_TOUCH_MULTI,
			QT602240_TOUCH_ORIENT, pdata->orient);

	/* Set touchscreen burst length */
	atmel_mxt224_write_object(data, QT602240_TOUCH_MULTI,
			QT602240_TOUCH_BLEN, pdata->blen);

	/* Set touchscreen threshold */
	atmel_mxt224_write_object(data, QT602240_TOUCH_MULTI,
			QT602240_TOUCH_TCHTHR, pdata->threshold);

	/* Set touchscreen resolution */
	atmel_mxt224_write_object(data, QT602240_TOUCH_MULTI,
			QT602240_TOUCH_XRANGE_LSB, (pdata->x_size - 1) & 0xff);
	atmel_mxt224_write_object(data, QT602240_TOUCH_MULTI,
			QT602240_TOUCH_XRANGE_MSB, (pdata->x_size - 1) >> 8);
	atmel_mxt224_write_object(data, QT602240_TOUCH_MULTI,
			QT602240_TOUCH_YRANGE_LSB, (pdata->y_size - 1) & 0xff);
	atmel_mxt224_write_object(data, QT602240_TOUCH_MULTI,
			QT602240_TOUCH_YRANGE_MSB, (pdata->y_size - 1) >> 8);

	/* Set touchscreen voltage */
	if (pdata->voltage) {
		if (pdata->voltage < QT602240_VOLTAGE_DEFAULT) {
			voltage = (QT602240_VOLTAGE_DEFAULT - pdata->voltage) /
				QT602240_VOLTAGE_STEP;
			voltage = 0xff - voltage + 1;
		}
	}
}

static int atmel_mxt224_get_info(struct atmel_mxt224_data *data)
{
	struct i2c_client *client = data->client;
	struct atmel_mxt224_info *info = &data->info;
	int ret;
	u8 val;

	ret = atmel_mxt224_read_reg(client, QT602240_FAMILY_ID, &val);
	if (ret)
		return ret;
	info->family_id = val;

	ret = atmel_mxt224_read_reg(client, QT602240_VARIANT_ID, &val);
	if (ret)
		return ret;
	info->variant_id = val;

	ret = atmel_mxt224_read_reg(client, QT602240_VERSION, &val);
	if (ret)
		return ret;
	info->version = val;

	ret = atmel_mxt224_read_reg(client, QT602240_BUILD, &val);
	if (ret)
		return ret;
	info->build = val;

	ret = atmel_mxt224_read_reg(client, QT602240_OBJECT_NUM, &val);
	if (ret)
		return ret;
	info->object_num = val;

	return 0;
}

static int atmel_mxt224_get_object_table(struct atmel_mxt224_data *data)
{
	int ret;
	int i;
	u16 reg;
	u8 reportid = 0;
	u8 buf[QT602240_OBJECT_SIZE];

	for (i = 0; i < data->info.object_num; i++) {
		struct atmel_mxt224_object *object = data->object_table + i;

		reg = QT602240_OBJECT_START + QT602240_OBJECT_SIZE * i;
		ret = atmel_mxt224_read_object_table(data->client, reg, buf);
		if (ret)
			return ret;

		object->type = buf[0];
		object->start_address = (buf[2] << 8) | buf[1];
		object->size = buf[3];
		object->instances = buf[4];
		object->num_report_ids = buf[5];

		if (object->num_report_ids) {
			reportid += object->num_report_ids *
				(object->instances + 1);
			object->max_reportid = reportid;
		}
	}

	return 0;
}

static int atmel_mxt224_initialize(struct atmel_mxt224_data *data)
{
	struct i2c_client *client = data->client;
	struct atmel_mxt224_info *info = &data->info;
	int ret;
	u8 val;

	ret = atmel_mxt224_get_info(data);
	if (ret)
		return ret;

	data->object_table =
		kzalloc(sizeof(struct atmel_mxt224_data) * info->object_num,
				GFP_KERNEL);
	if (!data->object_table) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	/* Get object table information */
	ret = atmel_mxt224_get_object_table(data);
	if (ret)
		return ret;

	/* Check register init values */
	ret = atmel_mxt224_check_reg_init(data);
	if (ret)
		return ret;

	/* Check X/Y matrix size */
	ret = atmel_mxt224_check_matrix_size(data);
	if (ret)
		return ret;

	ret = atmel_mxt224_make_highchg(data);
	if (ret)
		return ret;

	atmel_mxt224_handle_pdata(data);

	/* Update matrix size at info struct */
	ret = atmel_mxt224_read_reg(client, QT602240_MATRIX_X_SIZE, &val);
	if (ret)
		return ret;
	info->matrix_xsize = val;

	ret = atmel_mxt224_read_reg(client, QT602240_MATRIX_Y_SIZE, &val);
	if (ret)
		return ret;
	info->matrix_ysize = val;

	dev_info(&client->dev,
			"Family ID: %d Variant ID: %d Version: %d Build: %d\n",
			info->family_id, info->variant_id, info->version,
			info->build);

	dev_info(&client->dev,
			"Matrix X Size: %d Matrix Y Size: %d Object Num: %d\n",
			info->matrix_xsize, info->matrix_ysize,
			info->object_num);

	return 0;
}

static struct attribute *atmel_mxt224_attrs[] = {
	NULL
};

static const struct attribute_group atmel_mxt224_attr_group = {
	.attrs = atmel_mxt224_attrs,
};

static void atmel_mxt224_start(struct atmel_mxt224_data *data)
{
	/* Touch enable */
	atmel_mxt224_write_object(data, QT602240_TOUCH_MULTI,
			QT602240_TOUCH_CTRL, 0x83);

	atmel_mxt224_make_highchg(data);
}

static void atmel_mxt224_stop(struct atmel_mxt224_data *data)
{
	/* Touch disable */
	atmel_mxt224_write_object(data, QT602240_TOUCH_MULTI,
			QT602240_TOUCH_CTRL, 0);
}

static int atmel_mxt224_input_open(struct input_dev *dev)
{
	struct atmel_mxt224_data *data = input_get_drvdata(dev);

	atmel_mxt224_start(data);

	return 0;
}

static void atmel_mxt224_input_close(struct input_dev *dev)
{
	struct atmel_mxt224_data *data = input_get_drvdata(dev);

	atmel_mxt224_stop(data);
}

static int __devinit atmel_mxt224_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct atmel_mxt224_data *data;
	struct input_dev *input_dev;
	int ret;

	if (!client->dev.platform_data)
		return -EINVAL;

	data = kzalloc(sizeof(struct atmel_mxt224_data), GFP_KERNEL);

	input_dev = input_allocate_device();
	if (!data || !input_dev) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_free_mem;
	}

	input_dev->name = "Atmel maXTouch 224-channel Touchscreen Driver";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_dev->open = atmel_mxt224_input_open;
	input_dev->close = atmel_mxt224_input_close;

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);

	/* For single touch */
	input_set_abs_params(input_dev, ABS_X, 0, QT602240_MAX_XC, 0,
			0);
	input_set_abs_params(input_dev, ABS_Y, 0, QT602240_MAX_YC, 0,
			0);

	/* For mXT224 IC */
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0,
			QT602240_MAX_AREA, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0,
			QT602240_MAX_XC, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0,
			QT602240_MAX_YC, 0, 0);

	input_set_drvdata(input_dev, data);

	data->client = client;
	data->input_dev = input_dev;
	data->pdata = client->dev.platform_data;
	data->irq = client->irq;

	i2c_set_clientdata(client, data);

	ret = atmel_mxt224_initialize(data);
	if (ret)
		goto err_free_object;

	ret = gpio_request(data->irq, "atmel_mxt224_irq");
	if (ret < 0) {
		dev_dbg(&client->dev, "unable to get INT GPIO\n");
		ret = -ENODEV;
		goto err_free_mem;
	}

	gpio_direction_input(data->irq);

	ret = request_threaded_irq(gpio_to_irq(client->irq), NULL,
			atmel_mxt224_interrupt, IRQF_TRIGGER_FALLING,
			client->dev.driver->name, data);
	if (ret) {
		dev_err(&client->dev, "Failed to register interrupt\n");
		goto err_free_object;
	}

	ret = input_register_device(input_dev);
	if (ret)
		goto err_free_irq;

	ret = sysfs_create_group(&client->dev.kobj, &atmel_mxt224_attr_group);
	if (ret)
		goto err_unregister_device;

	return 0;

err_unregister_device:
	input_unregister_device(input_dev);
err_free_irq:
	free_irq(client->irq, data);
err_free_object:
	kfree(data->object_table);
err_free_mem:
	input_free_device(input_dev);
	kfree(data);
	return ret;
}

static int __devexit atmel_mxt224_remove(struct i2c_client *client)
{
	struct atmel_mxt224_data *data = i2c_get_clientdata(client);

	sysfs_remove_group(&client->dev.kobj, &atmel_mxt224_attr_group);
	free_irq(data->irq, data);
	input_unregister_device(data->input_dev);
	kfree(data->object_table);
	kfree(data);

	return 0;
}

#ifdef CONFIG_PM
static int atmel_mxt224_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct atmel_mxt224_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;

	mutex_lock(&input_dev->mutex);

	if (input_dev->users)
		atmel_mxt224_stop(data);

	mutex_unlock(&input_dev->mutex);

	return 0;
}

static int atmel_mxt224_resume(struct i2c_client *client)
{
	struct atmel_mxt224_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;

	/* Soft reset */
	atmel_mxt224_write_object(data, QT602240_GEN_COMMAND,
				QT602240_COMMAND_RESET, 1);

	msleep(QT602240_RESET_TIME);

	mutex_lock(&input_dev->mutex);

	if (input_dev->users)
		atmel_mxt224_start(data);

	mutex_unlock(&input_dev->mutex);

	return 0;
}
#else
#define atmel_mxt224_suspend	NULL
#define atmel_mxt224_resume	NULL
#endif

static const struct i2c_device_id atmel_mxt224_id[] = {
	{ "atmel_mxt224", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, atmel_mxt224_id);

static struct i2c_driver atmel_mxt224_driver = {
	.driver = {
		.name	= "atmel_mxt224",
	},
	.probe		= atmel_mxt224_probe,
	.remove		= __devexit_p(atmel_mxt224_remove),
	.suspend	= atmel_mxt224_suspend,
	.resume		= atmel_mxt224_resume,
	.id_table	= atmel_mxt224_id,
};

static int __init atmel_mxt224_init(void)
{
	return i2c_add_driver(&atmel_mxt224_driver);
}

static void __exit atmel_mxt224_exit(void)
{
	i2c_del_driver(&atmel_mxt224_driver);
}

module_init(atmel_mxt224_init);
module_exit(atmel_mxt224_exit);

/* Module information */
MODULE_AUTHOR("Alejandro Jimenez <alexjimenez@ti.com>");
MODULE_DESCRIPTION("Atmel mXT224 TouchScreen Driver");
MODULE_LICENSE("GPL");
