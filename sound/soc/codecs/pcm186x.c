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

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/gcd.h>
#include <linux/clk.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/tlv.h>

#include <linux/pm.h>
#include <linux/regmap.h>

#include "pcm186x.h"

/* Default based on PCM186x EVM board clock */
#define PCM186x_ADC_MASTERCLOCK_DEFAULT 24576000

/*----------------------------------------------------------------------------------*/

struct pcm186x_priv
{
	struct regmap *regmap;
	struct clk *sclk;
	int fmt;
	int powered;
};

/*----------------------------------------------------------------------------------*/
/* I2C Address definition */
/*----------------------------------------------------------------------------------*/

static const struct i2c_device_id pcm186x_i2c_id[] = {
	{"pcm186x", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c,pcm186x_i2c_id);

static const struct of_device_id pcm186x_of_match[] = {
	{.compatible = "ti,pcm186x",},
	{}
};
MODULE_DEVICE_TABLE(of,pcm186x_of_match);

static const struct reg_default pcm186x_reg_defaults[] = {
	{PCM186x_PGA_VAL_CH1_L,0x00},
	{PCM186x_PGA_VAL_CH1_R,0x00},
	{PCM186x_PGA_VAL_CH2_L,0x00},
	{PCM186x_PGA_VAL_CH2_R,0x00},
	{PCM186x_PGA_CLIP_CONTROL,0x86},
	{PCM186x_ADC1_INPUT_SELECT_L,0x41},
	{PCM186x_ADC1_INPUT_SELECT_R,0x41},	
	{PCM186x_ADC2_INPUT_SELECT_L,0x42},
	{PCM186x_ADC2_INPUT_SELECT_R,0x42},	
	{PCM186x_ADC2_INPUT_CHANNEL,0x00},
	{PCM186x_PCM_FORMAT,0x44},
	{PCM186x_TDM_SELECT,0x00},
	{PCM186x_TX_TDM_OFFSET,0x00},
	{PCM186x_RX_TDM_OFFSET,0x00},
	{PCM186x_DPGA_VAL_CH1_L,0x00},
	{PCM186x_GPIO_0_AND_1_CTRL,0x01},
	{PCM186x_GPIO_2_AND_3_CTRL,0x20},
	{PCM186x_GPIO_0_AND_1_DIR,0x00},
	{PCM186x_GPIO_2_AND_3_DIR,0x00},
	{PCM186x_GPIO_STATE,0x00},
	{PCM186x_GPIO_PULL_DOWN,0x00},
	{PCM186x_DPGA_VAL_CH1_R,0x00},
	{PCM186x_DPGA_VAL_CH2_L,0x00},
	{PCM186x_DPGA_VAL_CH2_R,0x00},
	{PCM186x_PGA_GAIN_MAP,0x00},
	{PCM186x_DIGMIC_INPUT,0x00},
	{PCM186x_DIN_RESAMPLE,0x00},
	{PCM186x_CLK_SELECT,0x01},
	{PCM186x_DSP1_CLK_DIVIDER,0x01},
	{PCM186x_DSP2_CLK_DIVIDER,0x01},
	{PCM186x_ADC_CLK_DIVIDER,0x03},
	{PCM186x_PLL_SCK_DIVIDER,0x07},
	{PCM186x_SCK_TO_BCK_DIVIDER,0x03},
	{PCM186x_SCK_TO_LRCK_DIVIDER,0x3f},
	{PCM186x_PLL_STATUS,0x01},
	{PCM186x_PLL_P_DIVIDER,0x00},
	{PCM186x_PLL_R_DIVIDER,0x00},
	{PCM186x_PLL_J_DIVIDER,0x01},
	{PCM186x_PLL_D1_DIVIDER,0x00},
	{PCM186x_PLL_D2_DIVIDER,0x00},
	{PCM186x_SIGDET_CHANNELS,0x00},
	{PCM186x_SIGDET_INTR_MASK,0x00},
	{PCM186x_SIGDET_STATUS,0x00},
	{PCM186x_SIGDET_LOSS_TIME,0x00},
	{PCM186x_SIGDET_SCAN_TIME,0x00},
	{PCM186x_SIGDET_INT_INTVL,0x01},
	{PCM186x_SIGDET_DC_REF_CH1_L,0x80},
	{PCM186x_SIGDET_DC_DIFF_CH1_L,0x7f},
	{PCM186x_SIGDET_DC_LEVEL_CH1_L,0x00},
	{PCM186x_SIGDET_DC_REF_CH1_R,0x80},
	{PCM186x_SIGDET_DC_DIFF_CH1_R,0x7f},
	{PCM186x_SIGDET_DC_LEVEL_CH1_R,0x00},
	{PCM186x_SIGDET_DC_REF_CH2_L,0x80},
	{PCM186x_SIGDET_DC_DIFF_CH2_L,0x7f},
	{PCM186x_SIGDET_DC_LEVEL_CH2_L,0x00},
	{PCM186x_SIGDET_DC_REF_CH2_R,0x80},
	{PCM186x_SIGDET_DC_DIFF_CH2_R,0x7f},
	{PCM186x_SIGDET_DC_LEVEL_CH2_R,0x00},
	{PCM186x_SIGDET_DC_REF_CH3_L,0x80},
	{PCM186x_SIGDET_DC_DIFF_CH3_L,0x7f},
	{PCM186x_SIGDET_DC_LEVEL_CH3_L,0x00},
	{PCM186x_SIGDET_DC_REF_CH3_R,0x80},
	{PCM186x_SIGDET_DC_DIFF_CH3_R,0x7f},
	{PCM186x_SIGDET_DC_LEVEL_CH3_R,0x00},
	{PCM186x_SIGDET_DC_REF_CH4_L,0x80},
	{PCM186x_SIGDET_DC_DIFF_CH4_L,0x7f},
	{PCM186x_SIGDET_DC_LEVEL_CH4_L,0x00},
	{PCM186x_SIGDET_DC_REF_CH4_R,0x80},
	{PCM186x_SIGDET_DC_DIFF_CH4_R,0x7f},
	{PCM186x_SIGDET_DC_LEVEL_CH4_R,0x00},
	{PCM186x_AUXADC_DATA_CTRL,0x00},
	{PCM186x_AUXADC_DATA_LSB,0x00},
	{PCM186x_AUXADC_DATA_MSB,0x00},
	{PCM186x_INTR_MASK,0x01},
	{PCM186x_INTR_STATUS,0x00},
	{PCM186x_INTR_PROPERTIES,0x10},
	{PCM186x_POWER_CTRL,0x70},
	{PCM186x_DSP_CTRL,0x10},
	{PCM186x_DEVICE_STATUS,0x00},
	{PCM186x_CURRENT_FREQUENCY,0x00},
	{PCM186x_CURRENT_CLK_RATIO,0x00},
	{PCM186x_CLK_STATUS,0x00},
	{PCM186x_DVDD_STATUS,0x00}
};

static bool pcm186x_readable(struct device *dev,unsigned int reg)
{
	return reg < 0xff;
}

static bool pcm186x_volatile(struct device *dev,unsigned int reg)
{
	switch(reg)
	{
		case PCM186x_GPIO_STATE:
		case PCM186x_PLL_STATUS:
		case PCM186x_SIGDET_STATUS:
		case PCM186x_AUXADC_DATA_CTRL:
		case PCM186x_INTR_STATUS:
		case PCM186x_DEVICE_STATUS:
		case PCM186x_CURRENT_FREQUENCY:
		case PCM186x_CURRENT_CLK_RATIO:
		case PCM186x_CLK_STATUS:
		case PCM186x_DVDD_STATUS:
			return true;
			
		default:
			return reg < 0xff;
	}
}

static const struct regmap_range_cfg pcm186x_range = {
	.name = "Pages", 
	.range_min = PCM186x_VIRT_BASE,
	.range_max = PCM186x_MAX_REGISTER,
	.selector_reg = PCM186x_PAGE,
	.selector_mask = 0xff,
	.window_start = 0,
	.window_len = 0x100,
};

const struct regmap_config pcm186x_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.readable_reg = pcm186x_readable,
	.volatile_reg = pcm186x_volatile,

	.ranges = &pcm186x_range,
	.num_ranges = 1,

	.max_register = PCM186x_MAX_REGISTER,
	.reg_defaults = pcm186x_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(pcm186x_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
};

/*----------------------------------------------------------------------------------*/
/* Debug Register Print */
/*----------------------------------------------------------------------------------*/

static void pcm186x_print_register(struct snd_soc_codec *codec,const char *name,unsigned int reg)
{
	int err;
	unsigned int val = 0;
	
	err = regmap_read(codec->component.regmap,reg,&val);
	if(!err)
	{
		printk(KERN_INFO "Err on read %s register\n",name);
	}
	printk(KERN_INFO "%s: %x\n",name,val);
}

/*
Description of the problem that is being seen and the various solutions that have been applied.

The problem: When the DSP clock and configuration is set, at the start of sampling, the correct I2S
clock signals are established. Using a 1kHz tone signal as input, an interference pattern is seen to
form on the input analog lines. It is like the analog signal is being reflected back from the DSP chip
with a phase value that is variable (seen interference cancel the signal, amplyify the signal, or be at
90 degree phase difference). The sampled output produced by 'arecord' corrisponds with this interference
pattern.

Now when the input and reflected signal cancel each other an interesting thing is noticed on the logic
analyser. Namely there are samples being produced at first on the DOUT line but these quickly reduce to
zero over the course of about 4ms or so. Hence this is the primary motivation for looking at section 12
and the power up sequence. Hence my questions about the Fade-IN part. 

It is like there are two ADC signals present (But ADC2 by default has no input selection). Both signals
are from the line 1 input but they can be out of phase from each other. The fade in/cancel provides 
evidence of this because there is a signal (when none should be as fade-in should be happening and thus
I would expect mute) but as the real signal is brought in it cancels the one that should not be there and
the DOUT line appears muted.

-----

In terms of sound driver archtecture the PCM186x DSP clock, and thus internal circuits are powered up,
when the pcm186x_dai_startup function is called. 12.5 shows the power up sequence and process. In return when
pcm186x_dai_shutdown is called the chip should be placed into stand-by mode. 

The primary motivation for this is that when the clock configuration is done (ie pcm186x_setup_clocks
based on sampling rate and bit depth) then it is at this point the I2S output signal is generated. Hence
if the chip is powered on an I2S output signal is generated by the PCM186x chip. In fact this is the
default state of the chip when it powered on and it is the reason why I managed to get a sample output
from it in very quick order. However I think this "interference" problem has always been there from
the start.... just that my original input source was from a PC headphone output so I thought that was
the problem. Hence using DAC+ with my player as tone generator.

If the chip powered on and left on then the I2S clock signal continues to be produced and thus hogging the
I2S bus. This is why I am putting the chip into stand-by mode pcm186x_dai_shutdown.

Now if I could keep the chip powered with its internal circuits clocked from the oscillator wihout
producing an I2S output signal that would be a good fix. Not ideal as the chip consumes power, but 
the kernel power management could later be used for handling this. Probably can be done via 
PCM186x_CLK_SELECT by setting it as a slave mode.... but then the DSP has no clock (if the RPi
is not producing an I2S signal).
*/

static void pcm186x_print_registers(struct snd_soc_codec *codec)
{
	printk(KERN_INFO "PCM186x registers\n");
	
	/* Default value of 0 means 0dB gain */
	pcm186x_print_register(codec,"Pg(0)/Reg(1) PCM186x_PGA_VAL_CH1_L",PCM186x_PGA_VAL_CH1_L); 
	pcm186x_print_register(codec,"Pg(0)/Reg(2) PCM186x_PGA_VAL_CH1_R",PCM186x_PGA_VAL_CH1_R);
	pcm186x_print_register(codec,"Pg(0)/Reg(3) PCM186x_PGA_VAL_CH2_L",PCM186x_PGA_VAL_CH2_L);
	pcm186x_print_register(codec,"Pg(0)/Reg(4) PCM186x_PGA_VAL_CH2_R",PCM186x_PGA_VAL_CH2_R);
	
	/* Discuss - this has a number of options that could be a cause of the problem */
	pcm186x_print_register(codec,"Pg(0)/Reg(5) PCM186x_PGA_CLIP_CONTROL",PCM186x_PGA_CLIP_CONTROL);
	
	/* Input selection has a signal polarity bit, the inverse wave input is being seen, is this required - discuss */
	/* Input channel 1 - Left Channel */
	pcm186x_print_register(codec,"Pg(0)/Reg(6) PCM186x_ADC1_INPUT_SELECT_L",PCM186x_ADC1_INPUT_SELECT_L);
	/* Input channel 1 - Right Channel */
	pcm186x_print_register(codec,"Pg(0)/Reg(7) PCM186x_ADC1_INPUT_SELECT_R",PCM186x_ADC1_INPUT_SELECT_R);
	/* Input channel 2 - Left Channel */
	pcm186x_print_register(codec,"Pg(0)/Reg(8) PCM186x_ADC2_INPUT_SELECT_L",PCM186x_ADC2_INPUT_SELECT_L);
	/* Input channel 2 - Right Channel */
	pcm186x_print_register(codec,"Pg(0)/Reg(9) PCM186x_ADC2_INPUT_SELECT_R",PCM186x_ADC2_INPUT_SELECT_R);

	/* Discuss - Ensure that this 0 by default as ADC2 should by in use */
	/* What is the difference between this ADC2 input select and those defined by Reg 8/9? */
	pcm186x_print_register(codec,"Pg(0)/Reg(10) PCM186x_ADC2_INPUT_CHANNEL",PCM186x_ADC2_INPUT_CHANNEL);

	/* PCM word length (default 24-bit) - set by pcm186x_set_bits_per_sample */
	pcm186x_print_register(codec,"Pg(0)/Reg(11) PCM186x_PCM_FORMAT",PCM186x_PCM_FORMAT);

	/* Not too sure about this but default is for 2 channel selection */
	pcm186x_print_register(codec,"Pg(0)/Reg(12) PCM186x_TDM_SELECT",PCM186x_TDM_SELECT);

	/* Serial audio data offset adjustment, another possible but default is 0 and although clock errors are
	   seen during the initial clock adjustment phase (when format is set or startup) the actual clock signal
	   stablises but it the output from the analog that is delayed and mirrored (interference pattern on oscilloscope
	   when it is clock adjustment is made) along with either a muted output (the sine wave cancels itself out?)
	   or sample is taken where interference is heard and seen on oscilloscope. */
	pcm186x_print_register(codec,"Pg(0)/Reg(13) PCM186x_TX_TDM_OFFSET",PCM186x_TX_TDM_OFFSET);
	pcm186x_print_register(codec,"Pg(0)/Reg(14) PCM186x_RX_TDM_OFFSET",PCM186x_RX_TDM_OFFSET);
	
	/* Discuss - guess this is an analog mic input rather than digital Reg 1-4. Its default value is for 0dB */
	pcm186x_print_register(codec,"Pg(0)/Reg(15) PCM186x_DPGA_VAL_CH1_L",PCM186x_DPGA_VAL_CH1_L);

	/* Discuss - would any of the default GPIO states have an effect. */
	/* Default - Digital MIC Input 1 */
	pcm186x_print_register(codec,"Pg(0)/Reg(16) PCM186x_GPIO_0_AND_1_CTRL",PCM186x_GPIO_0_AND_1_CTRL);
	/* Default - GPIO3 -> INT interupt, GPIO2 as GPIO2 */
	pcm186x_print_register(codec,"Pg(0)/Reg(17) PCM186x_GPIO_2_AND_3_CTRL",PCM186x_GPIO_2_AND_3_CTRL);
	/* Default - GPIO0 & GPIO1 are input */
	pcm186x_print_register(codec,"Pg(0)/Reg(18) PCM186x_GPIO_0_AND_1_DIR",PCM186x_GPIO_0_AND_1_DIR);
	/* Default - GPIO2 & GPIO3 are input */
	pcm186x_print_register(codec,"Pg(0)/Reg(19) PCM186x_GPIO_2_AND_3_DIR",PCM186x_GPIO_2_AND_3_DIR);
	/* Default - all GPIO state 0 */
	pcm186x_print_register(codec,"Pg(0)/Reg(20) PCM186x_GPIO_STATE",PCM186x_GPIO_STATE);
	/* Default - pull down is enabled on all GPIO */
	pcm186x_print_register(codec,"Pg(0)/Reg(21) PCM186x_GPIO_PULL_DOWN",PCM186x_GPIO_PULL_DOWN);

	/* Discuss - This one is a primary candidate. This is PGA digital gain. Firstly the default value
	   is 0. This does not map to the dB scale given particularly a gain of 0db. Second, is the value
	   actually applicable in a default setup.. i.e. How exactly is it applied? Third there are only
	   3 registers for a 4 channel setup, so is this all relative to the left channel 1. */
	pcm186x_print_register(codec,"Pg(0)/Reg(22) PCM186x_DPGA_VAL_CH1_R",PCM186x_DPGA_VAL_CH1_R);
	pcm186x_print_register(codec,"Pg(0)/Reg(23) PCM186x_DPGA_VAL_CH2_L",PCM186x_DPGA_VAL_CH2_L);
	pcm186x_print_register(codec,"Pg(0)/Reg(24) PCM186x_DPGA_VAL_CH2_R",PCM186x_DPGA_VAL_CH2_R);
	
	/* Default - Autogain mapping enabled for both 1/2 on analog and digital */
	pcm186x_print_register(codec,"Pg(0)/Reg(25) PCM186x_PGA_GAIN_MAP",PCM186x_PGA_GAIN_MAP);
	/* Default - Digital mic GPIO 0 */
	pcm186x_print_register(codec,"Pg(0)/Reg(26) PCM186x_DIGMIC_INPUT",PCM186x_DIGMIC_INPUT);
	
	pcm186x_print_register(codec,"Pg(0)/Reg(27) PCM186x_DIN_RESAMPLE",PCM186x_DIN_RESAMPLE);

	/* Discuss - This is one of our primary candidates. The question is where and when should it be
	   used. A series of tests using Bus Pirate and Logic Analyser should help see the exact behaviour
	   of the chip when this is set. Namely this is clock configuration. At a minimum (based on 12.5)
	   when this set then PCM186x_PLL_STATUS needs to be checked. Also based on Fig 29 all parts
	   should either be using SCK and PLL.*/
	/* Default - (SCK or XI) as input, Slave selected, SCK as ADC, DSP1 and DSP2 clock source, auto-detect clock.
	   This default state means no I2S signal as it is slave but master is selected and then the clock signal is
	   produced. From this producure should be
	   1. Set to slave mode (hence no I2S output) and pause 2ms or so
	   2. Configure the clock divider registers accordingly
	   3. THEN set PCM186x_CLK_SELECT accordingly
	   4. WAIT for the PLL lock signal from PLL status register */
	pcm186x_print_register(codec,"Pg(0)/Reg(32) PCM186x_CLK_SELECT",PCM186x_CLK_SELECT);

	/* The default configuration with EVM and our ADC card work for 48000kHz and 24-bit setup given ~24MHz SCK */
	/* Discuss - Prime candiate. The pcm186x_setup_clocks calculates and sets these accordingly. From memory, when designing the 
	   clock divider algorithm I had set the various dividers for the I2S signal. However in looking at it again there are the
	   clock dividers for the DSP1, DSP2 and ADC circuits. These have not been setup and from previous investigation it was
	   decided that the default values were always sufficient... This might not be the case.*/
	pcm186x_print_register(codec,"Pg(0)/Reg(33) PCM186x_DSP1_CLK_DIVIDER",PCM186x_DSP1_CLK_DIVIDER);
	pcm186x_print_register(codec,"Pg(0)/Reg(34) PCM186x_DSP2_CLK_DIVIDER",PCM186x_DSP2_CLK_DIVIDER);
	pcm186x_print_register(codec,"Pg(0)/Reg(35) PCM186x_ADC_CLK_DIVIDER",PCM186x_ADC_CLK_DIVIDER);
	pcm186x_print_register(codec,"Pg(0)/Reg(37) PCM186x_PLL_SCK_DIVIDER",PCM186x_ADC_CLK_DIVIDER);

	/* Used to define ratio for bit clock onto I2S line - */
	pcm186x_print_register(codec,"Pg(0)/Reg(38) PCM186x_SCK_TO_BCK_DIVIDER",PCM186x_SCK_TO_BCK_DIVIDER);
	/* Divide up the I2S L/R clock - set in pcm186x_set_bits_per_sample */
	/* Discuss - There is a source of confusion here as it is called SCK->LRCK divider. But in Fig 29 CLK_DIV_MST_BCK....
	   Ah but one of the many "definitions" from T.I. -- end of notes*/
	pcm186x_print_register(codec,"Pg(0)/Reg(39) PCM186x_SCK_TO_LRCK_DIVIDER",PCM186x_SCK_TO_LRCK_DIVIDER);
	
	pcm186x_print_register(codec,"Pg(0)/Reg(40) PCM186x_PLL_STATUS",PCM186x_PLL_STATUS);
	pcm186x_print_register(codec,"Pg(0)/Reg(41) PCM186x_PLL_P_DIVIDER",PCM186x_PLL_P_DIVIDER);
	pcm186x_print_register(codec,"Pg(0)/Reg(42) PCM186x_PLL_R_DIVIDER",PCM186x_PLL_R_DIVIDER);
	pcm186x_print_register(codec,"Pg(0)/Reg(43) PCM186x_PLL_J_DIVIDER",PCM186x_PLL_J_DIVIDER);
	pcm186x_print_register(codec,"Pg(0)/Reg(44) PCM186x_PLL_D1_DIVIDER",PCM186x_PLL_D1_DIVIDER);
	pcm186x_print_register(codec,"Pg(0)/Reg(45) PCM186x_PLL_D2_DIVIDER",PCM186x_PLL_D2_DIVIDER);
	pcm186x_print_register(codec,"Pg(0)/Reg(48) PCM186x_SIGDET_CHANNELS",PCM186x_SIGDET_CHANNELS);
	pcm186x_print_register(codec,"Pg(0)/Reg(49) PCM186x_SIGDET_INTR_MASK",PCM186x_SIGDET_INTR_MASK);
	pcm186x_print_register(codec,"Pg(0)/Reg(50) PCM186x_SIGDET_STATUS",PCM186x_SIGDET_STATUS);
	pcm186x_print_register(codec,"Pg(0)/Reg(52) PCM186x_SIGDET_LOSS_TIME",PCM186x_SIGDET_LOSS_TIME);
	pcm186x_print_register(codec,"Pg(0)/Reg(53) PCM186x_SIGDET_SCAN_TIME",PCM186x_SIGDET_SCAN_TIME);
	pcm186x_print_register(codec,"Pg(0)/Reg(54) PCM186x_SIGDET_INT_INTVL",PCM186x_SIGDET_INT_INTVL);
	pcm186x_print_register(codec,"Pg(0)/Reg(64) PCM186x_SIGDET_DC_REF_CH1_L",PCM186x_SIGDET_DC_REF_CH1_L);
	pcm186x_print_register(codec,"Pg(0)/Reg(65) PCM186x_SIGDET_DC_DIFF_CH1_L",PCM186x_SIGDET_DC_DIFF_CH1_L);
	pcm186x_print_register(codec,"Pg(0)/Reg(66) PCM186x_SIGDET_DC_LEVEL_CH1_L",PCM186x_SIGDET_DC_LEVEL_CH1_L);
	pcm186x_print_register(codec,"Pg(0)/Reg(67) PCM186x_SIGDET_DC_REF_CH1_R",PCM186x_SIGDET_DC_REF_CH1_R);
	pcm186x_print_register(codec,"Pg(0)/Reg(68) PCM186x_SIGDET_DC_DIFF_CH1_R",PCM186x_SIGDET_DC_DIFF_CH1_R);
	pcm186x_print_register(codec,"Pg(0)/Reg(69) PCM186x_SIGDET_DC_LEVEL_CH1_R",PCM186x_SIGDET_DC_LEVEL_CH1_R);
	pcm186x_print_register(codec,"Pg(0)/Reg(70) PCM186x_SIGDET_DC_REF_CH2_L",PCM186x_SIGDET_DC_REF_CH2_L);
	pcm186x_print_register(codec,"Pg(0)/Reg(71) PCM186x_SIGDET_DC_DIFF_CH2_L",PCM186x_SIGDET_DC_DIFF_CH2_L);
	pcm186x_print_register(codec,"Pg(0)/Reg(72) PCM186x_SIGDET_DC_LEVEL_CH2_L",PCM186x_SIGDET_DC_LEVEL_CH2_L);
	pcm186x_print_register(codec,"Pg(0)/Reg(73) PCM186x_SIGDET_DC_REF_CH2_R",PCM186x_SIGDET_DC_REF_CH2_R);
	pcm186x_print_register(codec,"Pg(0)/Reg(74) PCM186x_SIGDET_DC_DIFF_CH2_R",PCM186x_SIGDET_DC_DIFF_CH2_R);
	pcm186x_print_register(codec,"Pg(0)/Reg(75) PCM186x_SIGDET_DC_LEVEL_CH2_R",PCM186x_SIGDET_DC_LEVEL_CH2_R);
	pcm186x_print_register(codec,"Pg(0)/Reg(76) PCM186x_SIGDET_DC_REF_CH3_L",PCM186x_SIGDET_DC_REF_CH3_L);
	pcm186x_print_register(codec,"Pg(0)/Reg(77) PCM186x_SIGDET_DC_DIFF_CH3_L",PCM186x_SIGDET_DC_DIFF_CH3_L);
	pcm186x_print_register(codec,"Pg(0)/Reg(78) PCM186x_SIGDET_DC_LEVEL_CH3_L",PCM186x_SIGDET_DC_LEVEL_CH3_L);
	pcm186x_print_register(codec,"Pg(0)/Reg(79) PCM186x_SIGDET_DC_REF_CH3_R",PCM186x_SIGDET_DC_REF_CH3_R);
	pcm186x_print_register(codec,"Pg(0)/Reg(80) PCM186x_SIGDET_DC_DIFF_CH3_R",PCM186x_SIGDET_DC_DIFF_CH3_R);
	pcm186x_print_register(codec,"Pg(0)/Reg(81) PCM186x_SIGDET_DC_LEVEL_CH3_R",PCM186x_SIGDET_DC_LEVEL_CH3_R);
	pcm186x_print_register(codec,"Pg(0)/Reg(82) PCM186x_SIGDET_DC_REF_CH4_L",PCM186x_SIGDET_DC_REF_CH4_L);
	pcm186x_print_register(codec,"Pg(0)/Reg(83) PCM186x_SIGDET_DC_DIFF_CH4_L",PCM186x_SIGDET_DC_DIFF_CH4_L);
	pcm186x_print_register(codec,"Pg(0)/Reg(84) PCM186x_SIGDET_DC_LEVEL_CH4_L",PCM186x_SIGDET_DC_LEVEL_CH4_L);
	pcm186x_print_register(codec,"Pg(0)/Reg(85) PCM186x_SIGDET_DC_REF_CH4_R",PCM186x_SIGDET_DC_REF_CH4_R);
	pcm186x_print_register(codec,"Pg(0)/Reg(86) PCM186x_SIGDET_DC_DIFF_CH4_R",PCM186x_SIGDET_DC_DIFF_CH4_R);
	pcm186x_print_register(codec,"Pg(0)/Reg(87) PCM186x_SIGDET_DC_LEVEL_CH4_R",PCM186x_SIGDET_DC_LEVEL_CH4_R);
	pcm186x_print_register(codec,"Pg(0)/Reg(88) PCM186x_AUXADC_DATA_CTRL",PCM186x_AUXADC_DATA_CTRL);
	pcm186x_print_register(codec,"Pg(0)/Reg(89) PCM186x_AUXADC_DATA_LSB",PCM186x_AUXADC_DATA_LSB);
	pcm186x_print_register(codec,"Pg(0)/Reg(90) PCM186x_AUXADC_DATA_MSB",PCM186x_AUXADC_DATA_MSB);
	pcm186x_print_register(codec,"Pg(0)/Reg(96) PCM186x_INTR_MASK",PCM186x_INTR_MASK);
	pcm186x_print_register(codec,"Pg(0)/Reg(97) PCM186x_INTR_STATUS",PCM186x_INTR_STATUS);
	pcm186x_print_register(codec,"Pg(0)/Reg(98) PCM186x_INTR_PROPERTIES",PCM186x_INTR_PROPERTIES);
	pcm186x_print_register(codec,"Pg(0)/Reg(112) PCM186x_POWER_CTRL",PCM186x_POWER_CTRL);
	pcm186x_print_register(codec,"Pg(0)/Reg(113) PCM186x_DSP_CTRL",PCM186x_DSP_CTRL);
	pcm186x_print_register(codec,"Pg(0)/Reg(114) PCM186x_DEVICE_STATUS",PCM186x_DEVICE_STATUS);
	pcm186x_print_register(codec,"Pg(0)/Reg(115) PCM186x_CURRENT_FREQUENCY",PCM186x_CURRENT_FREQUENCY);
	pcm186x_print_register(codec,"Pg(0)/Reg(116) PCM186x_CURRENT_CLK_RATIO",PCM186x_CURRENT_CLK_RATIO);
	pcm186x_print_register(codec,"Pg(0)/Reg(117) PCM186x_CLK_STATUS",PCM186x_CLK_STATUS);
	pcm186x_print_register(codec,"Pg(0)/Reg(120) PCM186x_DVDD_STATUS",PCM186x_DVDD_STATUS);
}

/*----------------------------------------------------------------------------------*/
/* Controls */
/*----------------------------------------------------------------------------------*/

/* -- Disable controls to remove confliction --
static const DECLARE_TLV_DB_MINMAX(pga_tlv, -1200, 4000);

static const char * const pcm186x_line_select_texts[] = {
	"Line In 1",
	"Line In 2",
	"Line In 3",
	"Line In 4",
};

static const unsigned int pcm186x_line_select_values[] = {
	1, 2, 4, 8,
};

static SOC_VALUE_ENUM_SINGLE_DECL(pcm186x_line_select_left_1, PCM186x_ADC1_INPUT_SELECT_L, 0, 0xf, pcm186x_line_select_texts, pcm186x_line_select_values);
static SOC_VALUE_ENUM_SINGLE_DECL(pcm186x_line_select_right_1, PCM186x_ADC1_INPUT_SELECT_R, 0, 0xf, pcm186x_line_select_texts, pcm186x_line_select_values);
static SOC_VALUE_ENUM_SINGLE_DECL(pcm186x_line_select_left_2, PCM186x_ADC2_INPUT_SELECT_L, 0, 0xf, pcm186x_line_select_texts, pcm186x_line_select_values);
static SOC_VALUE_ENUM_SINGLE_DECL(pcm186x_line_select_right_2, PCM186x_ADC2_INPUT_SELECT_R, 0, 0xf, pcm186x_line_select_texts, pcm186x_line_select_values);

static const struct snd_kcontrol_new pcm186x_controls[] = {
	SOC_DOUBLE_R_TLV("Mic 1 Boost Volume", PCM186x_PGA_VAL_CH1_L, PCM186x_PGA_VAL_CH1_R, 0x50, 0xe8, 1, pga_tlv),
	SOC_DOUBLE_R_TLV("Mic 2 Boost Volume", PCM186x_PGA_VAL_CH2_L, PCM186x_PGA_VAL_CH2_R, 0x50, 0xe8, 1, pga_tlv),
	SOC_ENUM("ADC Input Left Select 1", pcm186x_line_select_left_1),
	SOC_ENUM("ADC Input Right Select 1", pcm186x_line_select_right_1),
	SOC_ENUM("ADC Input Left Select 2", pcm186x_line_select_left_2),
	SOC_ENUM("ADC Input Right Select 2", pcm186x_line_select_right_2),
};
*/

/*----------------------------------------------------------------------------------*/
/* Derived from 9.13.5 - PCM1863/5 Manual PLL Calculation */
/*----------------------------------------------------------------------------------*/

static int pcm186x_calc_pll_clk_out(int pllClkIn,int R,int J,int D,int P)
{
	s64 t = (s64)pllClkIn * (s64)((R * J * 10000) + D);
	t = div64_s64(t,P * 10000);
	return (int)t;
}

/*----------------------------------------------------------------------------------*/

static int pcm186x_is_pll_valid(int pllClkIn,int fSRef,int R,int J,int D,int P)
{
	int pllClkOut,inDiv;

	if(!(R>=1 && R<=16))
		return PCM186X_PLL_INVALID_R_OUT_OF_RANGE;
	if(!(J>=1 && J<=63))
		return PCM186X_PLL_INVALID_J_OUT_OF_RANGE;
	if(!(D>=0 && D<=9999))
		return PCM186X_PLL_INVALID_D_OUT_OF_RANGE;
	if(!(P>=1 && P<=15))
		return PCM186X_PLL_INVALID_P_OUT_OF_RANGE;

	inDiv = pllClkIn / P;
	if(inDiv > 20000000)
		return PCM186X_PLL_INVALID_INPUTDIV_HIGH;

	pllClkOut = pcm186x_calc_pll_clk_out(pllClkIn,R,J,D,P);
	if(pllClkOut < 64000000)
		return PCM186X_PLL_INVALID_OUTPUT_LOW;
	else if(pllClkOut > PCM186X_PLL_MAX_FREQ)
		return PCM186X_PLL_INVALID_OUTPUT_HIGH;

	if(!D)
	{
		if(inDiv < 1000000)
			return PCM186X_PLL_INVALID_INPUTDIV_LOW;
	}
	else
	{
		if(inDiv < 6667000)
			return PCM186X_PLL_INVALID_INPUTDIV_LOW;
		if(!(J>=4 && J<=11))
			return PCM186X_PLL_INVALID_J_OUT_OF_RANGE;
		if(R!=1)
			return PCM186X_PLL_INVALID_R_OUT_OF_RANGE;
	}
	return PCM186X_PLL_VALID;
}

/*----------------------------------------------------------------------------------*/
/* From looking at the PLL ratio (Table 10) */
/*----------------------------------------------------------------------------------*/

static int pcm186x_pll_N_multipler(int fSRef)
{
	int m,N,O,diffN,diffO;

	N = 2;
	do
	{
		N <<= 1;
		m = fSRef * N;
	} while(m < PCM186X_PLL_MAX_FREQ);
	N >>= 1;

	O = 3;
	do
	{
		O <<= 1;
		m = fSRef * O;
	} while(m < PCM186X_PLL_MAX_FREQ);
	O >>= 1;

	diffN = PCM186X_PLL_MAX_FREQ - (fSRef * N);
	diffO = PCM186X_PLL_MAX_FREQ - (fSRef * O);

	return (diffN < diffO) ? N : O;
}

/*----------------------------------------------------------------------------------*/
/* As R=1 then it is implied implicitly */
/*----------------------------------------------------------------------------------*/

static int pcm186x_pll_determine_fraction(u64 W,int *J,int P)
{
	int i,iM,j;
	u64 Pv,Jv,Jt,W0,r,mult,diff,diffM;
	const u64 oneFP = 0x0000000100000000ULL;
	const u64 maxFP = 0xffffffffffffffffULL;

	mult = oneFP;
	Pv = (u64)(P);
	Jv = (u64)(*J);

	for(j=0;j<4;j++)
	{
		Jv *= 10;
		mult = div_u64(mult,10);
		for(i=-5,iM=-5,diffM=maxFP;i<=5;i++)
		{
			if(i<0 && !Jv)
				continue;
			Jt = (Jv + i) * mult;
			W0 = div64_u64_rem(Jt,Pv,&r);
			diff = (W > W0) ? W - W0 : W0 - W;
			if(diff < diffM)
			{
				iM = i;
				diffM = diff;
			}
		}
		Jv += iM;
	}
	Jt = (u64)(*J) * 10000;
	if(Jv > Jt)
	{
		Jv -= Jt;
	}
	else
	{
		Jv -= (Jt - 10000);
		*J -= 1;
	}
	return (int)Jv;
}

/*----------------------------------------------------------------------------------*/

static int pcm186x_is_pll_required(int fSRef,int bitsPerSample,int mclk_rate)
{
	return !(mclk_rate % (fSRef * bitsPerSample));
}

/*----------------------------------------------------------------------------------*/

static int pcm186x_set_bits_per_sample(struct snd_soc_codec *codec,int bitsPerSample)
{
	int ret = 0;

	if(bitsPerSample==16)
	{
		snd_soc_update_bits(codec,PCM186x_PCM_FORMAT,0xcc,0xcc);
		snd_soc_update_bits(codec,PCM186x_SCK_TO_LRCK_DIVIDER,0x3f,31);
	}
	else if(bitsPerSample==24)
	{
		snd_soc_update_bits(codec,PCM186x_PCM_FORMAT,0xcc,0x44);
		snd_soc_update_bits(codec,PCM186x_SCK_TO_LRCK_DIVIDER,0x3f,47);
	}
	else
	{
		ret = -1;
	}
	return ret;
}

/*----------------------------------------------------------------------------------*/

static int pcm186x_calc_pll_fractional_rate(int fSRef,int mClkRate,int *J,int *P)
{
	int jCount,Js,Ds,Ps,D;
	u64 W,Jt,W0,dW,dWMax;
	const u64 maxFP = 0xffffffffffffffffULL;

	D = -1;
	W = div64_u64(((u64)(*J) << 32),(u64)(*P));
	*J = -1;
	dWMax = maxFP;

	for(jCount = 4; jCount <= 11; jCount++)
	{
		Jt = div64_u64((u64)jCount << 33,W);
		if(Jt & 1)
		{
			Jt += 2;
		}
		Jt >>= 1;

		if(Jt>=1 && Jt<=15)
		{
			Js = jCount;
			Ps = (int)Jt;
			Ds = pcm186x_pll_determine_fraction(W,&Js,Ps);
			W0 = div64_u64(((u64)(Js) << 32) | (u64)(Ds * (0xffffffff / 10000)),(u64)Ps);
			dW = (W > W0) ? W - W0 : W0 - W;

			if(dW < dWMax && !pcm186x_is_pll_valid(mClkRate,fSRef,1,Js,Ds,Ps))
			{
				*J = Js;
				*P = Ps;
				D = Ds;
				dWMax = dW;
			}
		}
	}
	return D;
}

/*----------------------------------------------------------------------------------*/

static int pcm186x_setup_clocks(struct snd_soc_codec *codec,int fSRef,int bitsPerSample,int mClkRate)
{
	int g,R,J,D,P,N,pllClkOut,bitClkDiv,ret;

	if(pcm186x_is_pll_required(fSRef,bitsPerSample,mClkRate))
	{
		// PLL is not required
		snd_soc_update_bits(codec,PCM186x_CLK_SELECT,0xff,0x91);
		bitClkDiv = mClkRate / (fSRef * bitsPerSample * 2);
		snd_soc_update_bits(codec,PCM186x_SCK_TO_BCK_DIVIDER,0x7f,(unsigned int)bitClkDiv-1);
		snd_soc_update_bits(codec,PCM186x_PLL_STATUS,0x03,0x00);
		ret = pcm186x_set_bits_per_sample(codec,bitsPerSample);
	}
	else
	{
		// Use PLL
		N = pcm186x_pll_N_multipler(fSRef);
		pllClkOut = N * fSRef;

		g = (int)gcd((unsigned long)pllClkOut,(unsigned long)mClkRate);
		J = pllClkOut / g;
		P = mClkRate / g;
		R = 1;

		if(P > 15)
		{
			D = pcm186x_calc_pll_fractional_rate(fSRef,mClkRate,&J,&P);
			ret = (D >= 0) ? 0 : -1;
		}
		else
		{
			D = 0;
			ret = pcm186x_is_pll_valid(mClkRate,fSRef,R,J,D,P);
		}

		if(!ret)
		{
			bitClkDiv = pllClkOut / (fSRef * bitsPerSample * 16);
			snd_soc_update_bits(codec,PCM186x_CLK_SELECT,0xff,0xbe);
			snd_soc_update_bits(codec,PCM186x_PLL_STATUS,0x03,0x01);
			snd_soc_update_bits(codec,PCM186x_PLL_P_DIVIDER,0x7f,(unsigned int)P-1);
			snd_soc_update_bits(codec,PCM186x_PLL_R_DIVIDER,0x0f,(unsigned int)R-1);
			snd_soc_update_bits(codec,PCM186x_PLL_J_DIVIDER,0x3f,(unsigned int)J);
			snd_soc_update_bits(codec,PCM186x_PLL_D1_DIVIDER,0xff,(unsigned int)D & 0x000000ff);
			snd_soc_update_bits(codec,PCM186x_PLL_D2_DIVIDER,0x3f,(unsigned int)(D >> 8) & 0x0000003f);
			snd_soc_update_bits(codec,PCM186x_PLL_SCK_DIVIDER,0x7f,0x07);
			snd_soc_update_bits(codec,PCM186x_SCK_TO_BCK_DIVIDER,0x7f,(unsigned int)bitClkDiv-1);
			ret = pcm186x_set_bits_per_sample(codec,bitsPerSample);
			snd_soc_update_bits(codec,PCM186x_PLL_STATUS,0x03,0x05);
		}
		else
		{
			ret = -1;
		}
	}
	return ret;
}

/*----------------------------------------------------------------------------------*/

static void pcm186x_power_device(struct snd_soc_codec *codec,int turnon)
{
	struct pcm186x_priv *pcm186x = snd_soc_codec_get_drvdata(codec);
	
	if(pcm186x->powered && !turnon)
	{
		snd_soc_update_bits(codec,PCM186x_POWER_CTRL,0x04,0x04);
		pcm186x->powered = 0;
	}
	else if(!pcm186x->powered && turnon)
	{
		snd_soc_update_bits(codec,PCM186x_POWER_CTRL,0x04,0x00);
		pcm186x->powered = 1;
	}
}

/*----------------------------------------------------------------------------------*/

static int pcm186x_set_master_capture_rate(struct snd_soc_dai *dai,int bps,int fs)
{
	int res,mclkrate;
	struct snd_soc_codec *codec = dai->codec;
	struct pcm186x_priv *pcm186x = snd_soc_codec_get_drvdata(codec);
	struct device *dev = dai->dev;
	
	if(IS_ERR(pcm186x->sclk))
	{
		dev_err(dev, "Need SCLK for master mode: %ld\n", PTR_ERR(pcm186x->sclk));
		return PTR_ERR(pcm186x->sclk);
	}
	if(bps < 16)
	{
		bps = 16;
	}
	if(!fs)
	{
		fs = 48000;
	}
		
	mclkrate = (int)clk_get_rate(pcm186x->sclk);
	if(!mclkrate)
	{
		dev_err(dev, "No rate for master clock found\n");
		return -EINVAL;
	}
	
	printk(KERN_INFO "PCM186x clock rate: %d, frequency: %d, bps: %d\n",mclkrate,fs,bps);

	res = pcm186x_setup_clocks(codec,fs,bps,mclkrate);
	if(res)
	{
		dev_err(dev, "Error setting up PCM186x DSP clock %d\n", res);
	}
	
	return res;
}

/*----------------------------------------------------------------------------------*/

static int pcm186x_dai_master_startup(struct snd_pcm_substream *substream,struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct snd_pcm_runtime *runtime = substream->runtime;

	printk(KERN_INFO "pcm186x_dai_master_startup\n");

	pcm186x_power_device(codec,1);

	return pcm186x_set_master_capture_rate(dai,snd_pcm_format_physical_width(runtime->format),runtime->rate);
}

/*----------------------------------------------------------------------------------*/

static int pcm186x_dai_slave_startup(struct snd_pcm_substream *substream,struct snd_soc_dai *dai)
{
	printk(KERN_ERR "pcm186x: Slave mode is to be implemented\n");
	return -1;
}

/*----------------------------------------------------------------------------------*/

static int pcm186x_dai_startup(struct snd_pcm_substream *substream,struct snd_soc_dai *dai)
{
	int res;
	struct snd_soc_codec *codec = dai->codec;
	struct pcm186x_priv *pcm186x = snd_soc_codec_get_drvdata(codec);
	
	printk(KERN_INFO "pcm186x_dai_startup\n");
	
	switch(pcm186x->fmt & SND_SOC_DAIFMT_MASTER_MASK)
	{
		case SND_SOC_DAIFMT_CBM_CFM:
		case SND_SOC_DAIFMT_CBM_CFS:
			res = pcm186x_dai_master_startup(substream,dai);
			break;
			
		case SND_SOC_DAIFMT_CBS_CFS:
			res = pcm186x_dai_slave_startup(substream,dai);
			break;
			
		default:
			res = -EINVAL;
			break;
	}
	return res;
}

/*----------------------------------------------------------------------------------*/

static void pcm186x_dai_shutdown(struct snd_pcm_substream *substream,struct snd_soc_dai *dai)
{
	printk(KERN_INFO "pcm186x_dai_shutdown\n");
	pcm186x_power_device(dai->codec,0);
}

/*----------------------------------------------------------------------------------*/

static int pcm186x_hw_params(struct snd_pcm_substream *substream,struct snd_pcm_hw_params *params,struct snd_soc_dai *dai)
{
	int res,bps;
	struct snd_soc_codec *codec = dai->codec;
	struct pcm186x_priv *pcm186x = snd_soc_codec_get_drvdata(codec);
	
	printk(KERN_INFO "pcm186x_hw_params\n");

	switch(pcm186x->fmt & SND_SOC_DAIFMT_MASTER_MASK)
	{
		case SND_SOC_DAIFMT_CBM_CFM:
		case SND_SOC_DAIFMT_CBM_CFS:
			bps = snd_pcm_format_physical_width(params_format(params));
			res = pcm186x_set_master_capture_rate(dai,bps,params_rate(params));
			break;
			
		case SND_SOC_DAIFMT_CBS_CFS:
		default:
			res = -EINVAL;
			break;
	}		
	return res;
}

/*----------------------------------------------------------------------------------*/

static int pcm186x_set_fmt(struct snd_soc_dai *dai,unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	struct pcm186x_priv *pcm186x = snd_soc_codec_get_drvdata(codec);
	
	printk(KERN_INFO "pcm186x_set_fmt\n");
	
	pcm186x->fmt = fmt;
	return 0;
}

/*----------------------------------------------------------------------------------*/

static const struct snd_soc_dai_ops pcm186x_dai_ops = {
	.startup = pcm186x_dai_startup,
	.shutdown = pcm186x_dai_shutdown,
	.hw_params = pcm186x_hw_params,
	.set_fmt = pcm186x_set_fmt,
};

/*----------------------------------------------------------------------------------*/

#define PCM186X_RATES ( \
	SNDRV_PCM_RATE_8000 | \
	SNDRV_PCM_RATE_16000 | \
	SNDRV_PCM_RATE_32000 | \
	SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000 | \
	SNDRV_PCM_RATE_64000 | \
	SNDRV_PCM_RATE_88200 | \
	SNDRV_PCM_RATE_96000 | \
	SNDRV_PCM_RATE_176400 | \
	SNDRV_PCM_RATE_192000 \
)

#define PCM186X_FORMATS ( \
	SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FMTBIT_S24_LE \
)

static struct snd_soc_dai_driver pcm186x_dai = {
	.name = "pcm186x-hifi",
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = PCM186X_RATES,
		.formats = PCM186X_FORMATS,
	},
	.ops = &pcm186x_dai_ops,
	.symmetric_rates = 1
};

static const struct snd_soc_codec_driver soc_codec_dev_pcm186x = {
	.idle_bias_off = false,
/*
	.controls = pcm186x_controls,
	.num_controls = ARRAY_SIZE(pcm186x_controls),
*/
};

/*----------------------------------------------------------------------------------*/

static int pcm186x_probe(struct device *dev, struct regmap *regmap)
{
	int ret;
	struct pcm186x_priv *pcm186x;
	
	pcm186x = devm_kzalloc(dev,sizeof(struct pcm186x_priv),GFP_KERNEL);
	if(!pcm186x)
	{
		return -ENOMEM;
	}
	
	dev_set_drvdata(dev,pcm186x);
	pcm186x->regmap = regmap;
	
	pcm186x->sclk = devm_clk_get(dev,NULL);
	if(PTR_ERR(pcm186x->sclk) == -EPROBE_DEFER)
	{
		return -EPROBE_DEFER;
	}
	if(!IS_ERR(pcm186x->sclk))
	{
		ret = clk_prepare_enable(pcm186x->sclk);
		if(ret)
		{
			dev_err(dev,"Failed to enable SCLK: %d\n",ret);
			return ret;
		}
	}

	ret = snd_soc_register_codec(dev,&soc_codec_dev_pcm186x,&pcm186x_dai,1);
	if(ret)
	{
		dev_err(dev, "Failed to register CODEC: %d\n", ret);
	}
	
	return ret;
}

/*----------------------------------------------------------------------------------*/

static void pcm186x_remove(struct device *dev)
{
	struct pcm186x_priv *pcm186x = dev_get_drvdata(dev);

	snd_soc_unregister_codec(dev);
	if (!IS_ERR(pcm186x->sclk))
	{
		clk_disable_unprepare(pcm186x->sclk);
	}
}

/*----------------------------------------------------------------------------------*/

static int pcm186x_i2c_probe(struct i2c_client *i2c,const struct i2c_device_id *id)
{
	struct regmap *regmap;
	struct regmap_config config = pcm186x_regmap;

	printk(KERN_INFO "pcm186x_i2c_probe\n");
	
	/* msb needs to be set to enable auto-increment of addresses */
	config.read_flag_mask = 0x80;
	config.write_flag_mask = 0x80;

	regmap = devm_regmap_init_i2c(i2c,&config);
	if(IS_ERR(regmap))
	{
		return PTR_ERR(regmap);
	}
	
	return pcm186x_probe(&i2c->dev,regmap);
}

/*----------------------------------------------------------------------------------*/

static int pcm186x_i2c_remove(struct i2c_client *i2c)
{
	printk(KERN_INFO "pcm186x_i2c_remove\n");
	pcm186x_remove(&i2c->dev);
	return 0;
}

/*----------------------------------------------------------------------------------*/

static struct i2c_driver pcm186x_i2c_driver = {
	.driver		= {
		.name	= "pcm186x",
		.owner	= THIS_MODULE,
		.of_match_table = pcm186x_of_match,
	},
	.probe 		= pcm186x_i2c_probe,
	.remove 	= pcm186x_i2c_remove,
	.id_table	= pcm186x_i2c_id,
};

/*----------------------------------------------------------------------------------*/

module_i2c_driver(pcm186x_i2c_driver);

MODULE_DESCRIPTION("Texas Instruments PCM186x codec driver - I2C");
MODULE_AUTHOR("Stuart MacLean <stuart@hifiberry.com>");
MODULE_LICENSE("GPL v2");
