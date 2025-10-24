/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright (c) 2020 Prevas A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/drivers/hwinfo.h>

#define LOG_LEVEL LOG_LEVEL_DBG
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(watchdog);

#include "watchdog.h"

/* Watchdog variables */
const struct device *wdt_dev;
int wdt_channel_id;
int64_t start_time;
bool stop_feeding = false;

void print_bar(void)
{
	LOG_INF("===================================================================");
}

void print_current_reset_cause(uint32_t *cause)
{
	int32_t ret;

	ret = hwinfo_get_reset_cause(cause);
	if (ret == 0) {
		LOG_INF("Current reset cause is:");
		if (*cause & RESET_PIN) {
			LOG_INF(" 0: reset due to RESET_PIN");
		}
		if (*cause & RESET_SOFTWARE) {
			LOG_INF(" 1: reset due to RESET_SOFTWARE");
		}
		if (*cause & RESET_BROWNOUT) {
			LOG_INF(" 2: reset due to RESET_BROWNOUT");
		}
		if (*cause & RESET_POR) {
			LOG_INF(" 3: reset due to RESET_POR");
		}
		if (*cause & RESET_WATCHDOG) {
			LOG_INF(" 4: reset due to RESET_WATCHDOG");
		}
		if (*cause & RESET_DEBUG) {
			LOG_INF(" 5: reset due to RESET_DEBUG");
		}
		if (*cause & RESET_SECURITY) {
			LOG_INF(" 6: reset due to RESET_SECURITY");
		}
		if (*cause & RESET_LOW_POWER_WAKE) {
			LOG_INF(" 7: reset due to RESET_LOW_POWER_WAKE");
		}
		if (*cause & RESET_CPU_LOCKUP) {
			LOG_INF(" 8: reset due to RESET_CPU_LOCKUP");
		}
		if (*cause & RESET_PARITY) {
			LOG_INF(" 9: reset due to RESET_PARITY");
		}
		if (*cause & RESET_PLL) {
			LOG_INF("10: reset due to RESET_PLL");
		}
		if (*cause & RESET_CLOCK) {
			LOG_INF("11: reset due to RESET_CLOCK");
		}
		if (*cause & RESET_HARDWARE) {
			LOG_INF("12: reset due to RESET_HARDWARE");
		}
		if (*cause & RESET_USER) {
			LOG_INF("13: reset due to RESET_USER");
		}
		if (*cause & RESET_TEMPERATURE) {
			LOG_INF("14: reset due to RESET_TEMPERATURE");
		}
		if (*cause & RESET_BOOTLOADER) {
			LOG_INF("15: reset due to RESET_BOOTLOADER");
		}
		if (*cause & RESET_FLASH) {
			LOG_INF("16: reset due to RESET_FLASH");
		}
	} else if (ret == -ENOSYS) {
		LOG_INF("hwinfo_get_reset_cause() is NOT supported");
		*cause = 0;
	} else {
		LOG_ERR("hwinfo_get_reset_cause() failed (ret = %d)", ret);
	}
	print_bar();
}

void clear_reset_cause(void)
{
	int32_t ret, temp;

	ret = hwinfo_clear_reset_cause();
	if (ret == 0) {
		LOG_INF("hwinfo_clear_reset_cause() was executed");
	} else if (ret == -ENOSYS) {
		LOG_INF("hwinfo_clear_reset_cause() is NOT supported");
	} else {
		LOG_ERR("hwinfo_clear_reset_cause() failed (ret = %d)", ret);
	}

	/* Confirm all are cleared */
	hwinfo_get_reset_cause(&temp);
	if (temp == 0) {
		LOG_INF("PASS: reset causes were cleared");
	} else {
		LOG_ERR("FAIL: reset case = %u while expected is 0", temp);
	}
	print_bar();
}

int init_watchdog(void)
{
	int err;
	struct wdt_timeout_cfg wdt_config = {
		/* Reset SoC when watchdog timer expires. */
		.flags = WDT_FLAG_RESET_SOC,
		/* Expire watchdog after max window */
		.window.min = WDT_MIN_WINDOW,
		.window.max = WDT_MAX_WINDOW,
	};

	wdt_dev = DEVICE_DT_GET(DT_ALIAS(watchdog0));  // Using board-defined alias
	if (!device_is_ready(wdt_dev)) {
		LOG_ERR("Watchdog device not ready");
		return -ENODEV;
	}

	wdt_channel_id = wdt_install_timeout(wdt_dev, &wdt_config);
	if (wdt_channel_id < 0) {
		LOG_ERR("Watchdog install error: %d", wdt_channel_id);
		return wdt_channel_id;
	}

	err = wdt_setup(wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);
	if (err < 0) {
		LOG_ERR("Watchdog setup error: %d", err);
		return err;
	}

	LOG_INF("Watchdog initialized successfully");
	return 0;
}

