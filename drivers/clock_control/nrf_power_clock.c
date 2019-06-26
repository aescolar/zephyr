/*
 * Copyright (c) 2016-2019 Nordic Semiconductor ASA
 * Copyright (c) 2016 Vinayak Kariappa Chettimada
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <errno.h>
#include <sys/atomic.h>
#include <device.h>
#include <sys/__assert.h>
#include <drivers/clock_control.h>
#include <drivers/clock_control/nrf_clock_control.h>
#include <nrf_clock.h>
#include <logging/log.h>
#if defined(CONFIG_USB_NRF52840) || NRF_POWER_HAS_POFCON
#include <nrf_power.h>
#endif

LOG_MODULE_REGISTER(clock_control, CONFIG_CLOCK_CONTROL_LOG_LEVEL);

/* Helper logging macros which prepends device name to the log. */
#define CLOCK_LOG(lvl, dev, ...) \
	LOG_##lvl("%s: " GET_ARG1(__VA_ARGS__), dev->config->name \
			COND_CODE_0(NUM_VA_ARGS_LESS_1(__VA_ARGS__),\
					(), (, GET_ARGS_LESS_1(__VA_ARGS__))))
#define ERR(dev, ...) CLOCK_LOG(ERR, dev, __VA_ARGS__)
#define WRN(dev, ...) CLOCK_LOG(WRN, dev, __VA_ARGS__)
#define INF(dev, ...) CLOCK_LOG(INF, dev, __VA_ARGS__)
#define DBG(dev, ...) CLOCK_LOG(DBG, dev, __VA_ARGS__)

/* returns true if clock stopping must be deferred due to active calibration. */
typedef void (*nrf_clock_stop_handler_t)(struct device *dev);

/* Clock instance structure */
struct nrf_clock_control {
	atomic_t ref;		/* Users counter */
	sys_slist_t list;	/* List of users requesting callback */
	bool started;		/* When set clock is started */
};

/* Clock instance static configuration */
struct nrf_clock_control_config {
	u32_t start_tsk;	/* Clock start task */
	u32_t started_evt;	/* Clock started event */
	u32_t stop_tsk;		/* Clock stop task */
	nrf_clock_stop_handler_t stop_handler; /* Function called before stop */
};

#define INT_MASK \
	(NRF_CLOCK_INT_HF_STARTED_MASK | NRF_CLOCK_INT_LF_STARTED_MASK \
	COND_CODE_1(CONFIG_USB_NRF52840, \
		    (|NRF_POWER_INT_USBDETECTED_MASK | \
		      NRF_POWER_INT_USBREMOVED_MASK | \
		      NRF_POWER_INT_USBPWRRDY_MASK), ()))

/* Return true if given event has enabled interrupt and is triggered. Event
 * is cleared.
 */
static bool clock_event_check_and_clean(u32_t evt, u32_t intmask)
{
	bool ret = nrf_clock_event_check(evt) &&
			nrf_clock_int_enable_check(intmask);

	if (ret) {
		nrf_clock_event_clear(evt);
	}

	return ret;
}

static enum clock_control_status get_status(struct device *dev,
					    clock_control_subsys_t sys)
{
	struct nrf_clock_control *data = dev->driver_data;

	__ASSERT_NO_MSG(z_arch_irq_current_prio() >=
			NVIC_GetPriority(nrfx_get_irq_number(NRF_CLOCK)));

	if (data->ref > 0) {
		return data->started ?
			CLOCK_CONTROL_STATUS_ON : CLOCK_CONTROL_STATUS_STARTING;
	}

	return CLOCK_CONTROL_STATUS_OFF;
}

static int clock_stop(struct device *dev, clock_control_subsys_t sub_system)
{
	const struct nrf_clock_control_config *config =
						dev->config->config_info;
	struct nrf_clock_control *data = dev->driver_data;

	int key = irq_lock();
	int err = 0;

	data->ref--;

	if (data->ref == 0) {
		DBG(dev, "Stopping");
		sys_slist_init(&data->list);

		if (config->stop_handler) {
			config->stop_handler(dev);
		}

		nrf_clock_task_trigger(config->stop_tsk);
		data->started = false;
	} else if (data->ref < 0) {
		data->ref = 0;
		err = -EALREADY;
	}

	irq_unlock(key);

	return err;
}

static int clock_async_start(struct device *dev,
			     clock_control_subsys_t sub_system,
			     struct clock_control_async_data *data)
{
	const struct nrf_clock_control_config *config =
						dev->config->config_info;
	struct nrf_clock_control *clk_data = dev->driver_data;
	int key = irq_lock();

	switch (get_status(dev, sub_system)) {
	case CLOCK_CONTROL_STATUS_ON:
		irq_unlock(key);
		if (data && data->cb) {
			data->cb(dev, data->user_data);
		}
		break;
	case CLOCK_CONTROL_STATUS_OFF:
		nrf_clock_event_clear(config->started_evt);
		nrf_clock_task_trigger(config->start_tsk);
		DBG(dev, "Triggered start task");
		/* fall through */
	case CLOCK_CONTROL_STATUS_STARTING:
		/* if node is in the list it means that it is scheduled for
		 * the second time.
		 */
		if (data) {
			if (data->node.next) {
				irq_unlock(key);
				return -EALREADY;
			}

			sys_slist_append(&clk_data->list, &data->node);
		}

		break;
	default:
		break;
	}

	clk_data->ref++;

	irq_unlock(key);

	return 0;
}

static int clock_start(struct device *dev, clock_control_subsys_t sub_system)
{
	return clock_async_start(dev, sub_system, NULL);
}

static void nrf_power_clock_isr(void *arg);

static int clock_control_init(struct device *hfclk_dev)
{
	/* TODO: Initialization will be called twice, once for 32KHz and then
	 * for 16 MHz clock. The vector is also shared for other power related
	 * features. Hence, design a better way to init IRQISR when adding
	 * power peripheral driver and/or new SoC series.
	 * NOTE: Currently the operations here are idempotent.
	 */
	IRQ_CONNECT(DT_INST_0_NORDIC_NRF_CLOCK_IRQ_0,
		    DT_INST_0_NORDIC_NRF_CLOCK_IRQ_0_PRIORITY,
		    nrf_power_clock_isr, 0, 0);

	irq_enable(DT_INST_0_NORDIC_NRF_CLOCK_IRQ_0);

	nrf_clock_lf_src_set(CLOCK_CONTROL_NRF_K32SRC);

	if (IS_ENABLED(CONFIG_CLOCK_CONTROL_NRF_K32SRC_RC_CALIBRATION)) {
		nrf_clock_control_calibration_init(hfclk_dev);
	}

	nrf_clock_int_enable(INT_MASK);

	return 0;
}

static int k32_dummy_init(struct device *dev)
{
	return 0;
}

static const struct clock_control_driver_api clock_control_api = {
	.on = clock_start,
	.off = clock_stop,
	.async_on = clock_async_start,
	.get_status = get_status,
};

static struct nrf_clock_control hfclk;
static const struct nrf_clock_control_config hfclk_config = {
	.start_tsk = NRF_CLOCK_TASK_HFCLKSTART,
	.started_evt = NRF_CLOCK_EVENT_HFCLKSTARTED,
	.stop_tsk = NRF_CLOCK_TASK_HFCLKSTOP
};

DEVICE_AND_API_INIT(clock_nrf5_m16src,
		    DT_INST_0_NORDIC_NRF_CLOCK_LABEL  "_16M",
		    clock_control_init, &hfclk, &hfclk_config, PRE_KERNEL_1,
		    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &clock_control_api);


static struct nrf_clock_control lfclk;
static const struct nrf_clock_control_config lfclk_config = {
	.start_tsk = NRF_CLOCK_TASK_LFCLKSTART,
	.started_evt = NRF_CLOCK_EVENT_LFCLKSTARTED,
	.stop_tsk = NRF_CLOCK_TASK_LFCLKSTOP,
	.stop_handler =
		IS_ENABLED(CONFIG_CLOCK_CONTROL_NRF_K32SRC_RC_CALIBRATION) ?
			nrf_clock_control_calibration_stop : NULL
};

DEVICE_AND_API_INIT(clock_nrf5_k32src,
		    DT_INST_0_NORDIC_NRF_CLOCK_LABEL  "_32K",
		    k32_dummy_init, &lfclk, &lfclk_config, PRE_KERNEL_1,
		    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &clock_control_api);

static void clkstarted_handle(struct device *dev)
{
	struct clock_control_async_data *async_data;
	struct nrf_clock_control *data = dev->driver_data;
	sys_snode_t *node = sys_slist_get(&data->list);

	DBG(dev, "Clock started");
	data->started = true;

	while (node != NULL) {
		async_data = CONTAINER_OF(node,
				struct clock_control_async_data, node);
		async_data->cb(dev, async_data->user_data);
		node = sys_slist_get(&data->list);
	}
}

#if defined(CONFIG_USB_NRF52840) || NRF_POWER_HAS_POFCON
static bool power_event_check_and_clean(u32_t evt, u32_t intmask)
{
	bool ret = nrf_power_event_check(evt) &&
			nrf_power_int_enable_check(intmask);

	if (ret) {
		nrf_clock_event_clear(evt);
	}

	return ret;
}
#endif

static void usb_power_isr(void)
{
#if defined(CONFIG_USB_NRF52840)
	extern void usb_dc_nrfx_power_event_callback(nrf_power_event_t event);

	if (power_event_check_and_clean(NRF_POWER_EVENT_USBDETECTED,
					NRF_POWER_INT_USBDETECTED_MASK)) {
		usb_dc_nrfx_power_event_callback(NRF_POWER_EVENT_USBDETECTED);
	}

	if (power_event_check_and_clean(NRF_POWER_EVENT_USBPWRRDY,
					NRF_POWER_INT_USBPWRRDY_MASK)) {
		usb_dc_nrfx_power_event_callback(NRF_POWER_EVENT_USBPWRRDY);
	}

	if (power_event_check_and_clean(NRF_POWER_EVENT_USBREMOVED,
					NRF_POWER_INT_USBREMOVED_MASK)) {
		usb_dc_nrfx_power_event_callback(NRF_POWER_EVENT_USBREMOVED);
	}
#endif
}

static void pof_isr(void)
{
#if NRF_POWER_HAS_POFCON
	if (power_event_check_and_clean(NRF_POWER_EVENT_POFWARN,
					NRF_POWER_INT_POFWARN_MASK)) {

	}
#endif
}

/* Note: this function has public linkage, and MUST have this
 * particular name.  The platform architecture itself doesn't care,
 * but there is a test (tests/kernel/arm_irq_vector_table) that needs
 * to find it to it can set it in a custom vector table.  Should
 * probably better abstract that at some point (e.g. query and reset
 * it by pointer at runtime, maybe?) so we don't have this leaky
 * symbol.
 */
void nrf_power_clock_isr(void *arg)
{
	ARG_UNUSED(arg);

	pof_isr();

	if (clock_event_check_and_clean(NRF_CLOCK_EVENT_HFCLKSTARTED,
					NRF_CLOCK_INT_HF_STARTED_MASK)) {
		struct device *hfclk_dev = &DEVICE_NAME_GET(clock_nrf5_m16src);
		struct nrf_clock_control *data = hfclk_dev->driver_data;

		if (!data->started) {
			clkstarted_handle(hfclk_dev);
		}
	}

	if (clock_event_check_and_clean(NRF_CLOCK_EVENT_LFCLKSTARTED,
					NRF_CLOCK_INT_LF_STARTED_MASK)) {
		clkstarted_handle(&DEVICE_NAME_GET(clock_nrf5_k32src));
	}

	usb_power_isr();

	if (IS_ENABLED(CONFIG_CLOCK_CONTROL_NRF_K32SRC_RC_CALIBRATION)) {
		nrf_clock_control_calibration_isr();
	}
}

#if defined(CONFIG_USB_NRF52840)

void nrf5_power_usb_power_int_enable(bool enable)
{
	u32_t mask;


	mask = NRF_POWER_INT_USBDETECTED_MASK |
	       NRF_POWER_INT_USBREMOVED_MASK |
	       NRF_POWER_INT_USBPWRRDY_MASK;

	if (enable) {
		nrf_power_int_enable(mask);
		irq_enable(DT_NORDIC_NRF_CLOCK_0_IRQ_0);
	} else {
		nrf_power_int_disable(mask);
	}
}

#endif
