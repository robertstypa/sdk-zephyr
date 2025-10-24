/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright (c) 2020 Prevas A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/stats/stats.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/reboot.h>

 #ifdef CONFIG_MCUMGR_GRP_FS
 #include <zephyr/device.h>
 #include <zephyr/fs/fs.h>
 #include <zephyr/fs/littlefs.h>
 #endif
 #ifdef CONFIG_MCUMGR_GRP_STAT
 #include <zephyr/mgmt/mcumgr/grp/stat_mgmt/stat_mgmt.h>
 #endif

 #define LOG_LEVEL LOG_LEVEL_DBG
 #include <zephyr/logging/log.h>
 LOG_MODULE_REGISTER(smp_sample);

 #include "common.h"

 #define STORAGE_PARTITION_LABEL	storage_partition
 #define STORAGE_PARTITION_ID	FIXED_PARTITION_ID(STORAGE_PARTITION_LABEL)

/* Watchdog configuration - synchronized with overlay UICR.WDTSTART timeout (~125ms) */
#define WDT_MAX_WINDOW  2000U     // Must match overlay crv calculation: (0x1000+1)/32768 ≈ 125ms
#define WDT_MIN_WINDOW  0U
#define WDG_FEED_INTERVAL 150U
#define WDT_OPT WDT_OPT_PAUSE_HALTED_BY_DBG
 /* Define an example stats group; approximates seconds since boot. */
 STATS_SECT_START(smp_svr_stats)
 STATS_SECT_ENTRY(ticks)
 STATS_SECT_END;

 /* Assign a name to the `ticks` stat. */
 STATS_NAME_START(smp_svr_stats)
 STATS_NAME(smp_svr_stats, ticks)
 STATS_NAME_END(smp_svr_stats);

 /* Define an instance of the stats group. */
 STATS_SECT_DECL(smp_svr_stats) smp_svr_stats;

/* Watchdog variables */
static const struct device *wdt_dev;
static int wdt_channel_id;
static int64_t start_time;
static bool stop_feeding = false;

 #ifdef CONFIG_MCUMGR_GRP_FS
 FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(cstorage);
 static struct fs_mount_t littlefs_mnt = {
	 .type = FS_LITTLEFS,
	 .fs_data = &cstorage,
	 .storage_dev = (void *)STORAGE_PARTITION_ID,
	 .mnt_point = "/lfs1"
 };
 #endif

/* Print LOG delimiter */
static void print_bar(void)
{
	LOG_INF("===================================================================");
}

static void print_current_reset_cause(uint32_t *cause)
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

/* Clear reset cause */
static void clear_reset_cause(void)
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
 static int init_watchdog(void)
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

 int main(void)
 {
	 int rc = STATS_INIT_AND_REG(smp_svr_stats, STATS_SIZE_32,
					 "smp_svr_stats");
	 uint32_t reset_cause = 0;

	 if (rc < 0) {
		 LOG_ERR("Error initializing stats system [%d]", rc);
	 }

	 /* Initialize watchdog */
	 rc = init_watchdog();
	 if (rc < 0) {
		 LOG_ERR("Error initializing watchdog [%d]", rc);
	 }

	 /* Record start time for watchdog simulation */
	 start_time = k_uptime_get();

	 /* Register the built-in mcumgr command handlers. */
  #ifdef CONFIG_MCUMGR_GRP_FS
	 rc = fs_mount(&littlefs_mnt);
	 if (rc < 0) {
		 LOG_ERR("Error mounting littlefs [%d]", rc);
	 }
  #endif

  #ifdef CONFIG_MCUMGR_TRANSPORT_BT
	 start_smp_bluetooth_adverts();
  #endif

	 /* using __TIME__ ensure that a new binary will be built on every
	  * compile which is convenient when testing firmware upgrade.
	  */
	 LOG_INF("build time - updated app: " __DATE__ " " __TIME__);

	/* The system work queue handles all incoming mcumgr requests.  Let the
	 * main thread idle while the mcumgr server runs.
	 */
	print_current_reset_cause(&reset_cause);
	clear_reset_cause();
	while (1) {
		k_sleep(K_MSEC(WDG_FEED_INTERVAL));  // Use defined interval (50ms)
		STATS_INC(smp_svr_stats, ticks);

		// if ((reset_cause & RESET_PIN) && !stop_feeding && (k_uptime_get() - start_time) > 15000) {
		// 	//LOG_INF("Stopping watchdog feeding - reset should occur in ~125ms");
		// 	//stop_feeding = true;
		// 	LOG_INF("SOFT RESET IN 3....2...1");
		// 	sys_reboot(SYS_REBOOT_COLD);
		// }

		 /* Feed the watchdog to prevent reset (only if not stopped) */
		if (wdt_dev && wdt_channel_id >= 0 && !stop_feeding) {
		 	wdt_feed(wdt_dev, wdt_channel_id);
		}
	 }
	 return 0;
 }
