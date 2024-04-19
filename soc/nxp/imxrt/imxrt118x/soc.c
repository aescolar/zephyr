/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <soc.h>
#include <zephyr/linker/sections.h>
#include <zephyr/linker/linker-defs.h>
#if defined(CONFIG_SOC_MIMXRT1189_CM7)
#include <zephyr/cache.h>
#elif defined(CONFIG_IMXRT118X_CM33_XCACHE_PS)
#include <fsl_cache.h>
#endif
#include <fsl_clock.h>
#include <fsl_gpc.h>
#include <fsl_pmu.h>
#include <fsl_dcdc.h>
#include <fsl_ele_base_api.h>
#include <fsl_trdc.h>
#include <zephyr/dt-bindings/clock/imx_ccm_rev2.h>
#include <cmsis_core.h>

/*
 * Set ELE_STICK_FAILED_STS to 0 when ELE status check is not required,
 * which is useful when debug reset, where the core has already get the
 * TRDC ownership at first time and ELE is not able to release TRDC
 * ownership again for the following TRDC ownership request.
 */
#define ELE_STICK_FAILED_STS 1

#if ELE_STICK_FAILED_STS
#define ELE_IS_FAILED(x) (x != kStatus_Success)
#else
#define ELE_IS_FAILED(x) false
#endif

#define ELE_TRDC_AON_ID    0x74
#define ELE_TRDC_WAKEUP_ID 0x78
#define ELE_CORE_CM33_ID   0x1
#define ELE_CORE_CM7_ID    0x2

#ifdef CONFIG_INIT_ARM_PLL
static const clock_arm_pll_config_t armPllConfig_BOARD_BootClockRUN = {
#if defined(CONFIG_SOC_MIMXRT1189_CM33) || defined(CONFIG_SOC_MIMXRT1189_CM7)
	/* Post divider, 0 - DIV by 2, 1 - DIV by 4, 2 - DIV by 8, 3 - DIV by 1 */
	.postDivider = kCLOCK_PllPostDiv2,
	/* PLL Loop divider, Fout = Fin * ( loopDivider / ( 2 * postDivider ) ) */
	.loopDivider = 132,
#else
	#error "Unknown SOC, no pll configuration defined"
#endif
};
#endif

const clock_sys_pll1_config_t sysPll1Config_BOARD_BootClockRUN = {
	/* Enable Sys Pll1 divide-by-2 clock or not */
	.pllDiv2En = 1,
	/* Enable Sys Pll1 divide-by-5 clock or not */
	.pllDiv5En = 1,
	/* Spread spectrum parameter */
	.ss = NULL,
	/* Enable spread spectrum or not */
	.ssEnable = false,
};

const clock_sys_pll2_config_t sysPll2Config_BOARD_BootClockRUN = {
	/* Denominator of spread spectrum */
	.mfd = 268435455,
	/* Spread spectrum parameter */
	.ss = NULL,
	/* Enable spread spectrum or not */
	.ssEnable = false,
};

/**
 * @brief Initialize the system clock
 */
static ALWAYS_INLINE void clock_init(void)
{
	clock_root_config_t rootCfg = {0};

	/* Init OSC RC 400M */
	CLOCK_OSC_EnableOscRc400M();
	CLOCK_OSC_GateOscRc400M(false);

#if CONFIG_CPU_CORTEX_M7
	/* Switch both core to OscRC400M first */
	rootCfg.mux = kCLOCK_M7_ClockRoot_MuxOscRc400M;
	rootCfg.div = 1;
	CLOCK_SetRootClock(kCLOCK_Root_M7, &rootCfg);
#endif

#if CONFIG_CPU_CORTEX_M33
	rootCfg.mux = kCLOCK_M33_ClockRoot_MuxOscRc400M;
	rootCfg.div = 2;
	CLOCK_SetRootClock(kCLOCK_Root_M33, &rootCfg);
#endif

#if CONFIG_CPU_CORTEX_M7
	DCDC_SetVDD1P0BuckModeTargetVoltage(DCDC, kDCDC_CORE0, kDCDC_1P0Target1P1V);
	DCDC_SetVDD1P0BuckModeTargetVoltage(DCDC, kDCDC_CORE1, kDCDC_1P0Target1P1V);
	/* FBB need to be enabled in OverDrive(OD) mode */
	PMU_EnableFBB(ANADIG_PMU, true);
#endif

	/* Config CLK_1M */
	CLOCK_OSC_Set1MHzOutputBehavior(kCLOCK_1MHzOutEnableFreeRunning1Mhz);

	/* Init OSC RC 24M */
	CLOCK_OSC_EnableOscRc24M(true);

	/* Config OSC 24M */
	ANADIG_OSC->OSC_24M_CTRL |= ANADIG_OSC_OSC_24M_CTRL_OSC_EN(1) |
	ANADIG_OSC_OSC_24M_CTRL_BYPASS_EN(0) | ANADIG_OSC_OSC_24M_CTRL_LP_EN(1) |
	ANADIG_OSC_OSC_24M_CTRL_OSC_24M_GATE(0);

	/* Wait for 24M OSC to be stable. */
	while (ANADIG_OSC_OSC_24M_CTRL_OSC_24M_STABLE_MASK !=
			(ANADIG_OSC->OSC_24M_CTRL & ANADIG_OSC_OSC_24M_CTRL_OSC_24M_STABLE_MASK)) {
	}

#ifdef CONFIG_INIT_ARM_PLL
	/* Init Arm Pll. */
	CLOCK_InitArmPll(&armPllConfig_BOARD_BootClockRUN);
#endif

	/* Init Sys Pll1. */
	CLOCK_InitSysPll1(&sysPll1Config_BOARD_BootClockRUN);

	/* Init Sys Pll2. */
	CLOCK_InitSysPll2(&sysPll2Config_BOARD_BootClockRUN);
	/* Init System Pll2 pfd0. */
	CLOCK_InitPfd(kCLOCK_PllSys2, kCLOCK_Pfd0, 27);
	/* Init System Pll2 pfd1. */
	CLOCK_InitPfd(kCLOCK_PllSys2, kCLOCK_Pfd1, 16);
	/* Init System Pll2 pfd2. */
	CLOCK_InitPfd(kCLOCK_PllSys2, kCLOCK_Pfd2, 24);
	/* Init System Pll2 pfd3. */
	CLOCK_InitPfd(kCLOCK_PllSys2, kCLOCK_Pfd3, 32);

	/* Init Sys Pll3. */
	CLOCK_InitSysPll3();
	/* Init System Pll3 pfd0. */
	CLOCK_InitPfd(kCLOCK_PllSys3, kCLOCK_Pfd0, 22);
	/* Init System Pll3 pfd1. */
	CLOCK_InitPfd(kCLOCK_PllSys3, kCLOCK_Pfd1, 33);
	/* Init System Pll3 pfd2. */
	CLOCK_InitPfd(kCLOCK_PllSys3, kCLOCK_Pfd2, 22);
	/* Init System Pll3 pfd3. */
	CLOCK_InitPfd(kCLOCK_PllSys3, kCLOCK_Pfd3, 18);

	/* Bypass Audio Pll. */
	CLOCK_SetPllBypass(kCLOCK_PllAudio, true);
	/* DeInit Audio Pll. */
	CLOCK_DeinitAudioPll();

#if defined(CONFIG_SOC_MIMXRT1189_CM7)
	/* Module clock root configurations. */
	/* Configure M7 using ARM_PLL_CLK */
	rootCfg.mux = kCLOCK_M7_ClockRoot_MuxArmPllOut;
	rootCfg.div = 1;
	CLOCK_SetRootClock(kCLOCK_Root_M7, &rootCfg);
#endif

#if defined(CONFIG_SOC_MIMXRT1189_CM33)
	/* Configure M33 using SYS_PLL3_CLK */
	rootCfg.mux = kCLOCK_M33_ClockRoot_MuxSysPll3Out;
	rootCfg.div = 2;
	CLOCK_SetRootClock(kCLOCK_Root_M33, &rootCfg);
#endif

	/* Configure BUS_AON using SYS_PLL2_CLK */
	rootCfg.mux = kCLOCK_BUS_AON_ClockRoot_MuxSysPll2Out;
	rootCfg.div = 4;
	CLOCK_SetRootClock(kCLOCK_Root_Bus_Aon, &rootCfg);

	/* Configure BUS_WAKEUP using SYS_PLL2_CLK */
	rootCfg.mux = kCLOCK_BUS_WAKEUP_ClockRoot_MuxSysPll2Out;
	rootCfg.div = 4;
	CLOCK_SetRootClock(kCLOCK_Root_Bus_Wakeup, &rootCfg);

	/* Configure WAKEUP_AXI using SYS_PLL3_CLK */
	rootCfg.mux = kCLOCK_WAKEUP_AXI_ClockRoot_MuxSysPll3Out;
	rootCfg.div = 2;
	CLOCK_SetRootClock(kCLOCK_Root_Wakeup_Axi, &rootCfg);

	/* Configure SWO_TRACE using SYS_PLL3_DIV2_CLK */
	rootCfg.mux = kCLOCK_SWO_TRACE_ClockRoot_MuxSysPll3Div2;
	rootCfg.div = 3;
	CLOCK_SetRootClock(kCLOCK_Root_Swo_Trace, &rootCfg);

#if CONFIG_CPU_CORTEX_M33
	/* Configure M33_SYSTICK using OSC_24M */
	rootCfg.mux = kCLOCK_M33_SYSTICK_ClockRoot_MuxOsc24MOut;
	rootCfg.div = 240;
	CLOCK_SetRootClock(kCLOCK_Root_M33_Systick, &rootCfg);
#endif

#if CONFIG_CPU_CORTEX_M7
	/* Configure M7_SYSTICK using OSC_24M */
	rootCfg.mux = kCLOCK_M7_SYSTICK_ClockRoot_MuxOsc24MOut;
	rootCfg.div = 240;
	CLOCK_SetRootClock(kCLOCK_Root_M7_Systick, &rootCfg);
#endif

#if defined(CONFIG_UART_MCUX_LPUART) && \
	(DT_NODE_HAS_STATUS(DT_NODELABEL(lpuart1), okay) \
	|| DT_NODE_HAS_STATUS(DT_NODELABEL(lpuart2), okay))
	/* Configure LPUART0102 using SYS_PLL3_DIV2_CLK */
	rootCfg.mux = kCLOCK_LPUART0102_ClockRoot_MuxSysPll3Div2;
	rootCfg.div = 10;
#endif

#if defined(CONFIG_I2C_MCUX_LPI2C) && \
	(DT_NODE_HAS_STATUS(DT_NODELABEL(lpi2c1), okay) \
	|| DT_NODE_HAS_STATUS(DT_NODELABEL(lpi2c2), okay))
	/* Configure LPI2C0102 using SYS_PLL3_DIV2_CLK */
	rootCfg.mux = kCLOCK_LPI2C0102_ClockRoot_MuxSysPll3Div2;
	rootCfg.div = 4;
	CLOCK_SetRootClock(kCLOCK_Root_Lpi2c0102, &rootCfg);
#endif

#if defined(CONFIG_I2C_MCUX_LPI2C) && \
	(DT_NODE_HAS_STATUS(DT_NODELABEL(lpi2c3), okay) \
	|| DT_NODE_HAS_STATUS(DT_NODELABEL(lpi2c4), okay))
	/* Configure LPI2C0304 using SYS_PLL3_DIV2_CLK */
	rootCfg.mux = kCLOCK_LPI2C0304_ClockRoot_MuxSysPll3Div2;
	rootCfg.div = 4;
	CLOCK_SetRootClock(kCLOCK_Root_Lpi2c0304, &rootCfg);
#endif

#if defined(CONFIG_I2C_MCUX_LPI2C) && \
	(DT_NODE_HAS_STATUS(DT_NODELABEL(lpi2c5), okay) \
	|| DT_NODE_HAS_STATUS(DT_NODELABEL(lpi2c6), okay))
	/* Configure LPI2C0506 using SYS_PLL3_DIV2_CLK */
	rootCfg.mux = kCLOCK_LPI2C0506_ClockRoot_MuxSysPll3Div2;
	rootCfg.div = 4;
	CLOCK_SetRootClock(kCLOCK_Root_Lpi2c0506, &rootCfg);
#endif

#if defined(CONFIG_SPI_MCUX_LPSPI) && \
	(DT_NODE_HAS_STATUS(DT_NODELABEL(lpspi1), okay) \
	|| DT_NODE_HAS_STATUS(DT_NODELABEL(lpspi2), okay))
	/* Configure LPSPI0102 using SYS_PLL3_PFD1_CLK */
	rootCfg.mux = kCLOCK_LPSPI0102_ClockRoot_MuxSysPll3Pfd1;
	rootCfg.div = 2;
	CLOCK_SetRootClock(kCLOCK_Root_Lpspi0102, &rootCfg);
#endif

#if defined(CONFIG_COUNTER_MCUX_GPT)

#if (DT_NODE_HAS_STATUS(DT_NODELABEL(gpt1), okay))
	/* Configure GPT1 using SYS_PLL3_DIV2_CLK */
	rootCfg.mux = kCLOCK_GPT1_ClockRoot_MuxSysPll3Div2;
	rootCfg.div = 1;
	CLOCK_SetRootClock(kCLOCK_Root_Gpt1, &rootCfg);
#endif /* DT_NODE_HAS_STATUS(DT_NODELABEL(gpt1), okay) */

#if (DT_NODE_HAS_STATUS(DT_NODELABEL(gpt2), okay))
	/* Configure GPT2 using SYS_PLL3_DIV2_CLK */
	rootCfg.mux = kCLOCK_GPT2_ClockRoot_MuxSysPll3Div2;
	rootCfg.div = 1;
	CLOCK_SetRootClock(kCLOCK_Root_Gpt2, &rootCfg);
#endif /* DT_NODE_HAS_STATUS(DT_NODELABEL(gpt2), okay) */

#endif /* CONFIG_COUNTER_MCUX_GPT */

#ifdef CONFIG_MCUX_ACMP

#if (DT_NODE_HAS_STATUS(DT_NODELABEL(acmp1), okay)  \
	|| DT_NODE_HAS_STATUS(DT_NODELABEL(acmp2), okay) \
	|| DT_NODE_HAS_STATUS(DT_NODELABEL(acmp3), okay) \
	|| DT_NODE_HAS_STATUS(DT_NODELABEL(acmp4), okay))
	/* Configure ACMP using MuxSysPll3Out */
	rootCfg.mux = kCLOCK_ACMP_ClockRoot_MuxSysPll3Out;
	rootCfg.div = 2;
	CLOCK_SetRootClock(kCLOCK_Root_Acmp, &rootCfg);
#endif

#endif /* CONFIG_MCUX_ACMP */

	/* Keep core clock ungated during WFI */
	CCM->LPCG[1].LPM0 = 0x33333333;
	CCM->LPCG[1].LPM1 = 0x33333333;
	/* Keep the system clock running so SYSTICK can wake up
	 * the system from wfi.
	 */
	GPC_CM_SetNextCpuMode(0, kGPC_RunMode);
	GPC_CM_SetNextCpuMode(1, kGPC_RunMode);
	GPC_CM_EnableCpuSleepHold(0, false);
	GPC_CM_EnableCpuSleepHold(1, false);

#if !defined(CONFIG_PM)
	/* Enable the AHB clock while the CM7 is sleeping to allow debug access
	 * to TCM
	 */
	BLK_CTRL_S_AONMIX->M7_CFG |= BLK_CTRL_S_AONMIX_M7_CFG_TCM_SIZE_MASK;
#endif
}

/**
 * @brief Initialize the system clock
 */
static ALWAYS_INLINE void trdc_enable_all_access(void)
{
	status_t sts;
	uint8_t i, j;

    /* Get ELE FW status */
	do {
		uint32_t ele_fw_sts;

		sts = ELE_BaseAPI_GetFwStatus(MU_RT_S3MUA, &ele_fw_sts);
	} while (sts != kStatus_Success);

	do {
#if defined(CONFIG_SOC_MIMXRT1189_CM33)
		/* Release TRDC A to CM33 core */
		sts = ELE_BaseAPI_ReleaseRDC(MU_RT_S3MUA, ELE_TRDC_AON_ID, ELE_CORE_CM33_ID);
#elif defined(CONFIG_SOC_MIMXRT1189_CM7)
		/* Release TRDC A to CM7 core */
		sts = ELE_BaseAPI_ReleaseRDC(MU_RT_S3MUA, ELE_TRDC_AON_ID, ELE_CORE_CM7_ID);
#endif
	} while (ELE_IS_FAILED(sts));

	/* Release TRDC W to CM33 core */
	do {
#if defined(CONFIG_SOC_MIMXRT1189_CM33)
		/* Release TRDC A to CM33 core */
		sts = ELE_BaseAPI_ReleaseRDC(MU_RT_S3MUA, ELE_TRDC_WAKEUP_ID, ELE_CORE_CM33_ID);
#elif defined(CONFIG_SOC_MIMXRT1189_CM7)
		/* Release TRDC A to CM7 core */
		sts = ELE_BaseAPI_ReleaseRDC(MU_RT_S3MUA, ELE_TRDC_WAKEUP_ID, ELE_CORE_CM7_ID);
#endif
	} while (ELE_IS_FAILED(sts));

	/* Enable all access modes for MBC and MRC of TRDCA and TRDCW */
	trdc_hardware_config_t hwConfig;
	trdc_memory_access_control_config_t memAccessConfig;

	(void)memset(&memAccessConfig, 0, sizeof(memAccessConfig));
	memAccessConfig.nonsecureUsrX  = 1U;
	memAccessConfig.nonsecureUsrW  = 1U;
	memAccessConfig.nonsecureUsrR  = 1U;
	memAccessConfig.nonsecurePrivX = 1U;
	memAccessConfig.nonsecurePrivW = 1U;
	memAccessConfig.nonsecurePrivR = 1U;
	memAccessConfig.secureUsrX     = 1U;
	memAccessConfig.secureUsrW     = 1U;
	memAccessConfig.secureUsrR     = 1U;
	memAccessConfig.securePrivX    = 1U;
	memAccessConfig.securePrivW    = 1U;
	memAccessConfig.securePrivR    = 1U;

	TRDC_GetHardwareConfig(TRDC1, &hwConfig);
	for (i = 0U; i < hwConfig.mrcNumber; i++) {
		for (j = 0U; j < 8; j++) {
			TRDC_MrcSetMemoryAccessConfig(TRDC1, &memAccessConfig, i, j);
		}
	}

	for (i = 0U; i < hwConfig.mbcNumber; i++) {
		for (j = 0U; j < 8; j++) {
			TRDC_MbcSetMemoryAccessConfig(TRDC1, &memAccessConfig, i, j);
		}
	}

	TRDC_GetHardwareConfig(TRDC2, &hwConfig);
	for (i = 0U; i < hwConfig.mrcNumber; i++) {
		for (j = 0U; j < 8; j++) {
			TRDC_MrcSetMemoryAccessConfig(TRDC2, &memAccessConfig, i, j);
		}
	}

	for (i = 0U; i < hwConfig.mbcNumber; i++) {
		for (j = 0U; j < 8; j++) {
			TRDC_MbcSetMemoryAccessConfig(TRDC2, &memAccessConfig, i, j);
		}
	}
}

/**
 *
 * @brief Perform basic hardware initialization
 *
 * Initialize the interrupt controller device drivers.
 * Also initialize the timer device driver, if required.
 * If dual core operation is enabled, the second core image will be loaded to RAM
 *
 * @return 0
 */

static int imxrt_init(void)
{
	/* Initialize system clock */
	clock_init();
	/* Get trdc and enable all access modes for MBC and MRC of TRDCA and TRDCW */
	trdc_enable_all_access();

	/* Enable data cache */
#if defined(CONFIG_IMXRT118X_CM33_XCACHE_PS)
	XCACHE_EnableCache(XCACHE_PS);
#elif defined(CONFIG_SOC_MIMXRT1189_CM7)
	sys_cache_data_enable();
#endif
	__ISB();
	__DSB();

	return 0;
}

#ifdef CONFIG_PLATFORM_SPECIFIC_INIT
void z_arm_platform_init(void)
{
	SystemInit();
}
#endif

SYS_INIT(imxrt_init, PRE_KERNEL_1, 0);