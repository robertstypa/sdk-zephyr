/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright (c) 2020 Prevas A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>

/* Watchdog configuration - synchronized with overlay UICR.WDTSTART timeout (~125ms) */
#define WDT_MAX_WINDOW  2000U     // Must match overlay crv calculation: (0x1000+1)/32768 ≈ 125ms
#define WDT_MIN_WINDOW  0U
#define WDG_FEED_INTERVAL 150U
#define WDT_OPT WDT_OPT_PAUSE_HALTED_BY_DBG

/* Watchdog variables - accessible from other modules */
extern const struct device *wdt_dev;
extern int wdt_channel_id;
extern int64_t start_time;
extern bool stop_feeding;

/**
 * @brief Print a delimiter bar to the log
 */
void print_bar(void);

/**
 * @brief Print the current reset cause to the log
 *
 * @param cause Pointer to store the reset cause value
 */
void print_current_reset_cause(uint32_t *cause);

/**
 * @brief Clear the reset cause
 */
void clear_reset_cause(void);

/**
 * @brief Initialize the watchdog timer
 *
 * @return 0 on success, negative error code on failure
 */
int init_watchdog(void);

#endif /* WATCHDOG_H */

