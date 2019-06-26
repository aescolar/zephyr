/*
 * Copyright (c) 2016 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_NRF_CLOCK_CONTROL_H_
#define ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_NRF_CLOCK_CONTROL_H_

#if defined(CONFIG_USB) && defined(CONFIG_SOC_NRF52840)
#include <device.h>
#endif
#include <nrf_clock.h>

/* TODO: move all these to clock_control.h ? */

/* Define 32KHz clock source */
#ifdef CONFIG_CLOCK_CONTROL_NRF_K32SRC_RC
#define CLOCK_CONTROL_NRF_K32SRC NRF_CLOCK_LFCLK_RC
#endif
#ifdef CONFIG_CLOCK_CONTROL_NRF_K32SRC_XTAL
#define CLOCK_CONTROL_NRF_K32SRC NRF_CLOCK_LFCLK_Xtal
#endif
#ifdef CONFIG_CLOCK_CONTROL_NRF_K32SRC_SYNTH
#define CLOCK_CONTROL_NRF_K32SRC NRF_CLOCK_LFCLK_Synth
#endif
#ifdef CONFIG_CLOCK_CONTROL_NRF_K32SRC_EXT_LOW_SWING
#define CLOCK_CONTROL_NRF_K32SRC NRF_CLOCK_LFCLK_Xtal_Low_Swing
#endif
#ifdef CONFIG_CLOCK_CONTROL_NRF_K32SRC_EXT_FULL_SWING
#define CLOCK_CONTROL_NRF_K32SRC NRF_CLOCK_LFCLK_Xtal_Full_Swing
#endif

/* Define 32KHz clock accuracy */
#ifdef CONFIG_CLOCK_CONTROL_NRF_K32SRC_500PPM
#define CLOCK_CONTROL_NRF_K32SRC_ACCURACY 0
#endif
#ifdef CONFIG_CLOCK_CONTROL_NRF_K32SRC_250PPM
#define CLOCK_CONTROL_NRF_K32SRC_ACCURACY 1
#endif
#ifdef CONFIG_CLOCK_CONTROL_NRF_K32SRC_150PPM
#define CLOCK_CONTROL_NRF_K32SRC_ACCURACY 2
#endif
#ifdef CONFIG_CLOCK_CONTROL_NRF_K32SRC_100PPM
#define CLOCK_CONTROL_NRF_K32SRC_ACCURACY 3
#endif
#ifdef CONFIG_CLOCK_CONTROL_NRF_K32SRC_75PPM
#define CLOCK_CONTROL_NRF_K32SRC_ACCURACY 4
#endif
#ifdef CONFIG_CLOCK_CONTROL_NRF_K32SRC_50PPM
#define CLOCK_CONTROL_NRF_K32SRC_ACCURACY 5
#endif
#ifdef CONFIG_CLOCK_CONTROL_NRF_K32SRC_30PPM
#define CLOCK_CONTROL_NRF_K32SRC_ACCURACY 6
#endif
#ifdef CONFIG_CLOCK_CONTROL_NRF_K32SRC_20PPM
#define CLOCK_CONTROL_NRF_K32SRC_ACCURACY 7
#endif

#if defined(CONFIG_USB) && defined(CONFIG_SOC_NRF52840)
void nrf5_power_usb_power_int_enable(bool enable);
#endif

/**
 * @brief Initialize LFCLK RC calibration.
 *
 * @param hfclk_dev HFCLK device.
 */
void nrf_clock_control_calibration_init(struct device *hfclk_dev);

/**
 * @brief Calibration interrupts handler
 *
 * Must be called from clock interrupt context.
 */
void nrf_clock_control_calibration_isr(void);

/**
 * @brief Stop calibration.
 *
 * Function called when LFCLK RC clock is being stopped.
 *
 * @param dev LFCLK device.
 */
void nrf_clock_control_calibration_stop(struct device *dev);

#endif /* ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_NRF_CLOCK_CONTROL_H_ */
