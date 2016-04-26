/*
 * Driver for the PCM186x codec
 *
 * Author:	Stuart MacLean <stuart@hifiberry.com>
 *		Copyright 2015
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef _SND_SOC_PCM186X
#define _SND_SOC_PCM186X

#include <linux/pm.h>
#include <linux/regmap.h>

#define PCM186x_VIRT_BASE 0x100
#define PCM186x_PAGE_LEN  0x100
#define PCM186x_PAGE_BASE(n)  (PCM186x_VIRT_BASE + (PCM186x_PAGE_LEN * n))

#define PCM186x_PAGE              0

/* Page 0 Registers */
#define PCM186x_PGA_VAL_CH1_L                   (PCM186x_PAGE_BASE(0) +   1)
#define PCM186x_PGA_VAL_CH1_R                   (PCM186x_PAGE_BASE(0) +   2)
#define PCM186x_PGA_VAL_CH2_L                   (PCM186x_PAGE_BASE(0) +   3)
#define PCM186x_PGA_VAL_CH2_R                   (PCM186x_PAGE_BASE(0) +   4)
#define PCM186x_PGA_CLIP_CONTROL                (PCM186x_PAGE_BASE(0) +   5)
#define PCM186x_ADC1_INPUT_SELECT_L             (PCM186x_PAGE_BASE(0) +   6)
#define PCM186x_ADC1_INPUT_SELECT_R             (PCM186x_PAGE_BASE(0) +   7)
#define PCM186x_ADC2_INPUT_SELECT_L             (PCM186x_PAGE_BASE(0) +   8)
#define PCM186x_ADC2_INPUT_SELECT_R             (PCM186x_PAGE_BASE(0) +   9)
#define PCM186x_ADC2_INPUT_CHANNEL              (PCM186x_PAGE_BASE(0) +  10)
#define PCM186x_PCM_FORMAT                      (PCM186x_PAGE_BASE(0) +  11)
#define PCM186x_TDM_SELECT                      (PCM186x_PAGE_BASE(0) +  12)
#define PCM186x_TX_TDM_OFFSET                   (PCM186x_PAGE_BASE(0) +  13)
#define PCM186x_RX_TDM_OFFSET                   (PCM186x_PAGE_BASE(0) +  14)
#define PCM186x_DPGA_VAL_CH1_L                  (PCM186x_PAGE_BASE(0) +  15)
#define PCM186x_GPIO_0_AND_1_CTRL               (PCM186x_PAGE_BASE(0) +  16)
#define PCM186x_GPIO_2_AND_3_CTRL               (PCM186x_PAGE_BASE(0) +  17)
#define PCM186x_GPIO_0_AND_1_DIR                (PCM186x_PAGE_BASE(0) +  18)
#define PCM186x_GPIO_2_AND_3_DIR                (PCM186x_PAGE_BASE(0) +  19)
#define PCM186x_GPIO_STATE                      (PCM186x_PAGE_BASE(0) +  20)
#define PCM186x_GPIO_PULL_DOWN                  (PCM186x_PAGE_BASE(0) +  21)
#define PCM186x_DPGA_VAL_CH1_R                  (PCM186x_PAGE_BASE(0) +  22)
#define PCM186x_DPGA_VAL_CH2_L                  (PCM186x_PAGE_BASE(0) +  23)
#define PCM186x_DPGA_VAL_CH2_R                  (PCM186x_PAGE_BASE(0) +  24)
#define PCM186x_PGA_GAIN_MAP                    (PCM186x_PAGE_BASE(0) +  25)
#define PCM186x_DIGMIC_INPUT                    (PCM186x_PAGE_BASE(0) +  26)
#define PCM186x_DIN_RESAMPLE                    (PCM186x_PAGE_BASE(0) +  27)
#define PCM186x_CLK_SELECT                      (PCM186x_PAGE_BASE(0) +  32)
#define PCM186x_DSP1_CLK_DIVIDER                (PCM186x_PAGE_BASE(0) +  33)
#define PCM186x_DSP2_CLK_DIVIDER                (PCM186x_PAGE_BASE(0) +  34)
#define PCM186x_ADC_CLK_DIVIDER                 (PCM186x_PAGE_BASE(0) +  35)
#define PCM186x_PLL_SCK_DIVIDER                 (PCM186x_PAGE_BASE(0) +  37)
#define PCM186x_SCK_TO_BCK_DIVIDER              (PCM186x_PAGE_BASE(0) +  38)
#define PCM186x_SCK_TO_LRCK_DIVIDER             (PCM186x_PAGE_BASE(0) +  39)
#define PCM186x_PLL_STATUS                      (PCM186x_PAGE_BASE(0) +  40)
#define PCM186x_PLL_P_DIVIDER                   (PCM186x_PAGE_BASE(0) +  41)
#define PCM186x_PLL_R_DIVIDER                   (PCM186x_PAGE_BASE(0) +  42)
#define PCM186x_PLL_J_DIVIDER                   (PCM186x_PAGE_BASE(0) +  43)
#define PCM186x_PLL_D1_DIVIDER                  (PCM186x_PAGE_BASE(0) +  44)
#define PCM186x_PLL_D2_DIVIDER                  (PCM186x_PAGE_BASE(0) +  45)
#define PCM186x_SIGDET_CHANNELS                 (PCM186x_PAGE_BASE(0) +  48)
#define PCM186x_SIGDET_INTR_MASK                (PCM186x_PAGE_BASE(0) +  49)
#define PCM186x_SIGDET_STATUS                   (PCM186x_PAGE_BASE(0) +  50)
#define PCM186x_SIGDET_LOSS_TIME                (PCM186x_PAGE_BASE(0) +  52)
#define PCM186x_SIGDET_SCAN_TIME                (PCM186x_PAGE_BASE(0) +  53)
#define PCM186x_SIGDET_INT_INTVL                (PCM186x_PAGE_BASE(0) +  54)
#define PCM186x_SIGDET_DC_REF_CH1_L             (PCM186x_PAGE_BASE(0) +  64)
#define PCM186x_SIGDET_DC_DIFF_CH1_L            (PCM186x_PAGE_BASE(0) +  65)
#define PCM186x_SIGDET_DC_LEVEL_CH1_L           (PCM186x_PAGE_BASE(0) +  66)
#define PCM186x_SIGDET_DC_REF_CH1_R             (PCM186x_PAGE_BASE(0) +  67)
#define PCM186x_SIGDET_DC_DIFF_CH1_R            (PCM186x_PAGE_BASE(0) +  68)
#define PCM186x_SIGDET_DC_LEVEL_CH1_R           (PCM186x_PAGE_BASE(0) +  69)
#define PCM186x_SIGDET_DC_REF_CH2_L             (PCM186x_PAGE_BASE(0) +  70)
#define PCM186x_SIGDET_DC_DIFF_CH2_L            (PCM186x_PAGE_BASE(0) +  71)
#define PCM186x_SIGDET_DC_LEVEL_CH2_L           (PCM186x_PAGE_BASE(0) +  72)
#define PCM186x_SIGDET_DC_REF_CH2_R             (PCM186x_PAGE_BASE(0) +  73)
#define PCM186x_SIGDET_DC_DIFF_CH2_R            (PCM186x_PAGE_BASE(0) +  74)
#define PCM186x_SIGDET_DC_LEVEL_CH2_R           (PCM186x_PAGE_BASE(0) +  75)
#define PCM186x_SIGDET_DC_REF_CH3_L             (PCM186x_PAGE_BASE(0) +  76)
#define PCM186x_SIGDET_DC_DIFF_CH3_L            (PCM186x_PAGE_BASE(0) +  77)
#define PCM186x_SIGDET_DC_LEVEL_CH3_L           (PCM186x_PAGE_BASE(0) +  78)
#define PCM186x_SIGDET_DC_REF_CH3_R             (PCM186x_PAGE_BASE(0) +  79)
#define PCM186x_SIGDET_DC_DIFF_CH3_R            (PCM186x_PAGE_BASE(0) +  80)
#define PCM186x_SIGDET_DC_LEVEL_CH3_R           (PCM186x_PAGE_BASE(0) +  81)
#define PCM186x_SIGDET_DC_REF_CH4_L             (PCM186x_PAGE_BASE(0) +  82)
#define PCM186x_SIGDET_DC_DIFF_CH4_L            (PCM186x_PAGE_BASE(0) +  83)
#define PCM186x_SIGDET_DC_LEVEL_CH4_L           (PCM186x_PAGE_BASE(0) +  84)
#define PCM186x_SIGDET_DC_REF_CH4_R             (PCM186x_PAGE_BASE(0) +  85)
#define PCM186x_SIGDET_DC_DIFF_CH4_R            (PCM186x_PAGE_BASE(0) +  86)
#define PCM186x_SIGDET_DC_LEVEL_CH4_R           (PCM186x_PAGE_BASE(0) +  87)
#define PCM186x_AUXADC_DATA_CTRL                (PCM186x_PAGE_BASE(0) +  88)
#define PCM186x_AUXADC_DATA_LSB                 (PCM186x_PAGE_BASE(0) +  89)
#define PCM186x_AUXADC_DATA_MSB                 (PCM186x_PAGE_BASE(0) +  90)
#define PCM186x_INTR_MASK                       (PCM186x_PAGE_BASE(0) +  96)
#define PCM186x_INTR_STATUS                     (PCM186x_PAGE_BASE(0) +  97)
#define PCM186x_INTR_PROPERTIES                 (PCM186x_PAGE_BASE(0) +  98)
#define PCM186x_POWER_CTRL                      (PCM186x_PAGE_BASE(0) + 112)
#define PCM186x_DSP_CTRL                        (PCM186x_PAGE_BASE(0) + 113)
#define PCM186x_DEVICE_STATUS                   (PCM186x_PAGE_BASE(0) + 114)
#define PCM186x_CURRENT_FREQUENCY               (PCM186x_PAGE_BASE(0) + 115)
#define PCM186x_CURRENT_CLK_RATIO               (PCM186x_PAGE_BASE(0) + 116)
#define PCM186x_CLK_STATUS                      (PCM186x_PAGE_BASE(0) + 117)
#define PCM186x_DVDD_STATUS                     (PCM186x_PAGE_BASE(0) + 120)

#define PCM186x_MAX_REGISTER                    (PCM186x_PAGE_BASE(253) + 64)

#define PCM186X_PLL_VALID                   0
#define PCM186X_PLL_INVALID_INPUTDIV_LOW   -1
#define PCM186X_PLL_INVALID_INPUTDIV_HIGH  -2
#define PCM186X_PLL_INVALID_OUTPUT_LOW     -3
#define PCM186X_PLL_INVALID_OUTPUT_HIGH    -4
#define PCM186X_PLL_INVALID_R_OUT_OF_RANGE -5
#define PCM186X_PLL_INVALID_J_OUT_OF_RANGE -6
#define PCM186X_PLL_INVALID_D_OUT_OF_RANGE -7
#define PCM186X_PLL_INVALID_P_OUT_OF_RANGE -8

#define PCM186X_PLL_MAX_FREQ 100000000

#define PCM186X_MASTER_CLK 1

#endif
