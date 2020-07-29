/*
 * Copyright (c) 2015, Guangdong OPPO Mobile Communication(Shanghai) 
 * Corp.,Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/input.h>
#include "hypnus.h"
#include <linux/ktime.h>
#include <linux/timekeeping.h>

static void handle_touch_down(struct hypnus_data *hypdata)
{
	unsigned long flags;
	struct input_data *input = &hypdata->input_data;

	spin_lock_irqsave(&hypdata->scene_lock, flags);
	input->is_touch_down = true;
	input->last_click = ktime_to_ms(ktime_get_boottime());

	spin_unlock_irqrestore(&hypdata->scene_lock, flags);
}

static void handle_touch_up(struct hypnus_data *hypdata)
{
	unsigned long flags;
	struct input_data *input = &hypdata->input_data;

	spin_lock_irqsave(&hypdata->scene_lock, flags);
	input->is_touch_down = false;
	input->last_click = ktime_to_ms(ktime_get_boottime());
	spin_unlock_irqrestore(&hypdata->scene_lock, flags);
}

static void hypnus_input_event(struct input_handle *handle,
				unsigned int type, unsigned int code, int value)
{
	struct hypnus_data *hypdata = get_hypnus_data();
	struct input_data *input = NULL;

	if (hypdata) {
		input = &hypdata->input_data;
	} else {
		pr_err("%s hypnus data is not initialized!\n", __func__);
		return;
	}

	switch (code) {
	case KEY_POWER:
	case KEY_HOME:
		switch (value) {
		case 1:
			break;
		case 0:
		default:
			/* else - against_lpm clear will be done in fb_cb */
			break;
		}
		break;
	case KEY_HOMEPAGE:
		switch (value) {
		case 1:
			break;
		case 0:
		default:
			break;
		}
		break;
	case KEY_BACK:
	case KEY_MENU:
		break;
	case BTN_TOUCH:
		if (value && !input->is_touch_down)
			handle_touch_down(hypdata);

		if (value == 0 && input->is_touch_down)
			handle_touch_up(hypdata);

		break;
	case ABS_MT_TRACKING_ID:
		if (value >= 0 && !input->is_touch_down)
			handle_touch_down(hypdata);

		if (value < 0 && input->is_touch_down)
			handle_touch_up(hypdata);

		break;
	case ABS_MT_POSITION_X:
	case ABS_MT_POSITION_Y:
	case ABS_MT_SLOT:
		break;
	default:
		break;
	}
}

static int hypnus_input_connect(struct input_handler *handler,
				struct input_dev *dev,
				const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "hypnus-input";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void hypnus_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id hypnus_input_ids[] = {
//#ifdef MONITOR_TOUCH_EVENTS
	/* multi-touch touchscreen */
	{
	 .flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_ABSBIT,
	 .evbit = {BIT_MASK(EV_ABS)},
	 .absbit = {[BIT_WORD(ABS_MT_POSITION_X)] =
		    BIT_MASK(ABS_MT_POSITION_X) | BIT_MASK(ABS_MT_POSITION_Y)},
	 },
	/* touchpad */
	{
	 .flags = INPUT_DEVICE_ID_MATCH_KEYBIT | INPUT_DEVICE_ID_MATCH_ABSBIT,
	 .keybit = {[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH)},
	 .absbit = {[BIT_WORD(ABS_X)] = BIT_MASK(ABS_X) | BIT_MASK(ABS_Y)},
	 },
//#endif
//#ifdef MONITOR_KEY_EVENTS
	/* Keypad */
	{
	 .flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_KEYBIT,
	 .evbit = {BIT_MASK(EV_KEY)},
	 .keybit = {[BIT_WORD(KEY_POWER)] = BIT_MASK(KEY_POWER)},
	 },
	/* Keypad */
	{
	 .flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_KEYBIT,
	 .evbit = {BIT_MASK(EV_KEY)},
	 .keybit = {[BIT_WORD(KEY_HOMEPAGE)] = BIT_MASK(KEY_HOMEPAGE)},
	 },
//#endif
	{},
};

static struct input_handler hypnus_input_handler = {
	.event = hypnus_input_event,
	.connect = hypnus_input_connect,
	.disconnect = hypnus_input_disconnect,
	.name = "hypnus-input",
	.id_table = hypnus_input_ids,
};

int hypnus_input_init(struct hypnus_data *hypdata)
{
	int ret;

	hypnus_input_handler.private = (void *)hypdata;
	ret = input_register_handler(&hypnus_input_handler);

	return ret;
}

void hypnus_input_exit(struct hypnus_data *hypdata)
{
	input_unregister_handler(&hypnus_input_handler);
}
