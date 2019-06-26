/*
 * Copyright (c) 2016-2019 Nordic Semiconductor ASA
 * Copyright (c) 2016 Vinayak Kariappa Chettimada
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <sensor.h>
#include <drivers/clock_control/nrf_clock_control.h>
#include <drivers/clock_control.h>
#include <nrf_clock.h>
#include <logging/log.h>

LOG_MODULE_DECLARE(clock_control, CONFIG_CLOCK_CONTROL_LOG_LEVEL);

#define TEMP_SENSOR_NAME COND_CODE_1(TEMP_NRF5, (CONFIG_TEMP_NRF5_NAME), (NULL))

/* Calibration state enum */
enum nrf_cal_state {
	CAL_IDLE,	/* Calibration timer active, waiting for expiration. */
	CAL_HFCLK_REQ,	/* HFCLK XTAL requested. */
	CAL_TEMP_REQ,	/* Temperature measurement requested. */
	CAL_ACTIVE	/* Ongoing calibration. */
};

static enum nrf_cal_state cal_state; /* Calibration state. */
static float prev_temperature; /* Previous temperature measurement. */
static u8_t calib_skip_cnt; /* Counting down skipped calibrations. */

/* Callback called on hfclk started. */
static void cal_hf_on_callback(struct device *dev, void *user_data);
static struct clock_control_async_data cal_hf_on_data = {
	.cb = cal_hf_on_callback
};

static struct device *hfclk_dev; /* Handler to hfclk device. */
static struct device *temp_sensor; /* Handler to temperature sensor device. */

static void measure_temperature(struct k_work *work);
struct k_work temp_measure_work = {
	.handler = measure_temperature
};

static bool clock_event_check_and_clean(u32_t evt, u32_t intmask)
{
	bool ret = nrf_clock_event_check(evt) &&
			nrf_clock_int_enable_check(intmask);

	if (ret) {
		nrf_clock_event_clear(evt);
	}

	return ret;
}

void nrf_clock_control_calibration_stop(struct device *dev)
{
	int key;

	nrf_clock_task_trigger(NRF_CLOCK_TASK_CTSTOP);

	key = irq_lock();

	/* If calibration is active then pend until completed.
	 * Currently (and most likely in the future), LFCLK is never stopped so
	 * it is not an issue.
	 */
	if (cal_state == CAL_ACTIVE) {
		while (clock_event_check_and_clean(NRF_CLOCK_EVENT_DONE,
					NRF_CLOCK_INT_DONE_MASK) == false) {
		}
	}

	irq_unlock(key);
}

void nrf_clock_control_calibration_init(struct device *dev)
{
	/* errata 56 */
	nrf_clock_event_clear(NRF_CLOCK_EVENT_DONE);
	nrf_clock_event_clear(NRF_CLOCK_EVENT_CTTO);

	nrf_clock_int_enable(NRF_CLOCK_INT_DONE_MASK | NRF_CLOCK_INT_CTTO_MASK);
	nrf_clock_cal_timer_timeout_set(
			CONFIG_CLOCK_CONTROL_NRF_CALIBRATION_PERIOD);

	if (CONFIG_CLOCK_CONTROL_NRF_CALIBRATION_MAX_SKIP != 0) {
		temp_sensor = device_get_binding(TEMP_SENSOR_NAME);
	}

	hfclk_dev = dev;
	cal_state = CAL_IDLE;
	nrf_clock_task_trigger(NRF_CLOCK_TASK_CTSTART);
}

/* Start calibration assuming that HFCLK XTAL is on. */
static void start_calibration(void)
{
	cal_state = CAL_ACTIVE;
	nrf_clock_task_trigger(NRF_CLOCK_TASK_CAL);
	LOG_DBG("Start calibration.");
	calib_skip_cnt = CONFIG_CLOCK_CONTROL_NRF_CALIBRATION_MAX_SKIP;
}

/* Restart calibration timer, release HFCLK XTAL. */
static void to_idle(void)
{
	cal_state = CAL_IDLE;
	clock_control_off(hfclk_dev, 0);
	nrf_clock_task_trigger(NRF_CLOCK_TASK_CTSTART);
}

static inline double sensor_value_to_float(struct sensor_value *val)
{
	return (float)val->val1 + (float)val->val2 / 1000000;
}

/* Function reads from temperature sensor and converts to float. */
static float get_temperature(void)
{
	struct sensor_value sensor_val;

	sensor_sample_fetch(temp_sensor);
	sensor_channel_get(temp_sensor, SENSOR_CHAN_DIE_TEMP, &sensor_val);

	return sensor_value_to_float(&sensor_val);
}

/* Function determines if calibration should be performed based on temperature
 * measurement. Function is called from system work queue context. It is
 * reading temperature from TEMP sensor and compares with last measurement.
 */
static void measure_temperature(struct k_work *work)
{
	float temperature;
	float diff;

	temperature = get_temperature();
	diff = temperature - prev_temperature;
	diff = diff < 0.0 ? -diff : diff;
	prev_temperature = temperature;

	if (diff >= CONFIG_CLOCK_CONTROL_NRF_CALIBRATION_TEMP_DIFF) {
		start_calibration();
	} else {
		LOG_DBG("Calibration skipped due to stable temperature.");
		to_idle();
		calib_skip_cnt--;
	}
}

/* Called when HFCLK XTAL is on. Schedules temperature measurement or triggers
 * calibration.
 */
static void cal_hf_on_callback(struct device *dev, void *user_data)
{
	int key = irq_lock();

	if (cal_state == CAL_HFCLK_REQ) {
		if ((CONFIG_CLOCK_CONTROL_NRF_CALIBRATION_MAX_SKIP == 0) ||
		    (IS_ENABLED(CONFIG_MULTITHREADING) == false) ||
		    calib_skip_cnt == 0) {
			start_calibration();
		} else {
			cal_state = CAL_TEMP_REQ;
			k_work_submit_to_queue(&k_sys_work_q,
					       &temp_measure_work);
		}
	}

	irq_unlock(key);
}

void nrf_clock_control_calibration_isr(void)
{
	if (clock_event_check_and_clean(NRF_CLOCK_EVENT_CTTO,
						NRF_CLOCK_INT_CTTO_MASK)) {
		LOG_DBG("Calibration timeout.");

		/* Start XTAL HFCLK. It is needed for temperature measurement
		 * and calibration.
		 */
		cal_state = CAL_HFCLK_REQ;
		clock_control_async_on(hfclk_dev, 0, &cal_hf_on_data);
	}

	if (clock_event_check_and_clean(NRF_CLOCK_EVENT_DONE,
					NRF_CLOCK_INT_DONE_MASK)) {
		LOG_DBG("Calibration done, timer restarted.");
		to_idle();
	}
}
