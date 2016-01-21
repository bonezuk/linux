/*
 * ASoC Driver for HiFiBerry DAC+ / DAC Pro
 *
 * Author:	Daniel Matuschek, Stuart MacLean <stuart@hifiberry.com>
 *		Copyright 2014-2015
 *		based on code by Florian Meier <florian.meier@koalo.de>
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
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/jack.h>

#include "../codec/pcm512x.h"


#define HIFIBERRY_DACPRO_NOCLOCK 0
#define HIFIBERRY_DACPRO_CLK44EN 1
#define HIFIBERRY_DACPRO_CLK48EN 2

struct pcm512x_priv {
	struct regmap *regmap;
	struct clk *sclk;
};

/* Clock rate of CLK44EN attached to GPIO6 pin */
#define CLK_44EN_RATE 22579200UL
/* Clock rate of CLK48EN attached to GPIO3 pin */
#define CLK_48EN_RATE 24576000UL

/* 3-bytes per coefficient for each frequency 8kHz, 11.025kHz, 16kHz, 22.05kHz, 32kHz, 44.1kHz, 
   48kHz, 88.2kHz, 96kHz, 176.4kHz, 192kHz. Each block is a set of 5 coefficients.
   11 noFreq * 5 coefficients * 3 bytes */

struct dacplus_filter_ctl {
	bool is_active;
	unsigned char coeff[11][5][3];
};

struct dacplus_driver_data {
	struct regmap *regmap;
	bool is_dacpro;
	struct mutex dac_lock;
	struct dacplus_filter_ctl filters[12];
};

static struct dacplus_driver_data *driver_data = NULL;

/*-------------------------------------------------------------------------------------------*/

/*
static void pcm512x_print_register(struct pcm512x_priv *pcm512x,const char *name,unsigned int reg,unsigned int mask)
{
	int err;
	unsigned int val = 0;
	
	err = regmap_read(pcm512x->regmap,reg,&val);
	if(err==0)
	{
		val &= mask;
		printk(KERN_INFO "%s: %x\n",name,val);
	}
	else
	{
		printk(KERN_INFO "Failed to read %s register\n",name);
	}
}

static void pcm512x_print_coefficient(struct pcm512x_priv *pcm512x,const char *name,unsigned int page,unsigned int reg)
{
	int err;
	unsigned int val;
	unsigned int valA = 0;
	unsigned int valB = 0;
	unsigned int valC = 0;
	
	err  = regmap_read(pcm512x->regmap,PCM512x_PAGE_BASE(page) + reg + 0,&valA);
	err |= regmap_read(pcm512x->regmap,PCM512x_PAGE_BASE(page) + reg + 1,&valB);
	err |= regmap_read(pcm512x->regmap,PCM512x_PAGE_BASE(page) + reg + 2,&valC);
	if(err==0)
	{
		val  = (valA << 16) & 0x00ff0000;
		val |= (valB <<  8) & 0x0000ff00;
		val |= (valC) & 0x000000ff;
		
		printk(KERN_INFO "%s (Pg %d, Reg %d,%d,%d,%d) - 0x%x\n",name,page,reg+0,reg+1,reg+2,reg+3,val);
	}
	else
	{
		printk(KERN_INFO "Failed to read %s register\n",name);
	}
}

static void snd_rpi_hifiberry_dump_registers(struct pcm512x_priv *pcm512x)
{
	printk(KERN_INFO "PCM512x registers\n");
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(1)   PCM512x_RESET",PCM512x_RESET,PCM512x_RSTR | PCM512x_RSTM);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(2)   PCM512x_POWER",PCM512x_POWER,PCM512x_RQPD | PCM512x_RQST);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(3)   PCM512x_MUTE",PCM512x_MUTE,(1 << PCM512x_RQMR_SHIFT) | (1 << PCM512x_RQML_SHIFT));
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(4)   PCM512x_PLL_EN",PCM512x_PLL_EN,PCM512x_PLLE | PCM512x_PLCK);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(6)   PCM512x_SPI_MISO_FUNCTION",PCM512x_SPI_MISO_FUNCTION,0x03);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(7)   PCM512x_DSP",PCM512x_DSP,PCM512x_SDSL | PCM512x_DEMP);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(8)   PCM512x_GPIO_EN",PCM512x_GPIO_EN,0x3f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(9)   PCM512x_BCLK_LRCLK_CFG",PCM512x_BCLK_LRCLK_CFG,PCM512x_LRKO | PCM512x_BCKO | PCM512x_BCKP);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(10)  PCM512x_DSP_GPIO_INPUT",PCM512x_DSP_GPIO_INPUT,0xff);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(12)  PCM512x_MASTER_MODE",PCM512x_MASTER_MODE,PCM512x_RLRK | PCM512x_RBCK);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(13)  PCM512x_PLL_REF",PCM512x_PLL_REF,PCM512x_SREF);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(14)  PCM512x_DAC_REF",PCM512x_DAC_REF,PCM512x_SDAC);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(16)  PCM512x_GPIO_DACIN",PCM512x_GPIO_DACIN,PCM512x_GREF);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(18)  PCM512x_GPIO_PLLIN",PCM512x_GPIO_PLLIN,PCM512x_GREF);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(19)  PCM512x_SYNCHRONIZE",PCM512x_SYNCHRONIZE,PCM512x_RQSY);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(20)  PCM512x_PLL_COEFF_0",PCM512x_PLL_COEFF_0,0x0f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(21)  PCM512x_PLL_COEFF_1",PCM512x_PLL_COEFF_1,0x3f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(22)  PCM512x_PLL_COEFF_2",PCM512x_PLL_COEFF_2,0x3f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(23)  PCM512x_PLL_COEFF_3",PCM512x_PLL_COEFF_3,0xff);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(24)  PCM512x_PLL_COEFF_4",PCM512x_PLL_COEFF_4,0x0f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(27)  PCM512x_DSP_CLKDIV",PCM512x_DSP_CLKDIV,0x7f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(28)  PCM512x_DAC_CLKDIV",PCM512x_DAC_CLKDIV,0x7f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(29)  PCM512x_NCP_CLKDIV",PCM512x_NCP_CLKDIV,0x7f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(30)  PCM512x_OSR_CLKDIV",PCM512x_OSR_CLKDIV,0x7f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(32)  PCM512x_MASTER_CLKDIV_1",PCM512x_MASTER_CLKDIV_1,0x7f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(33)  PCM512x_MASTER_CLKDIV_2",PCM512x_MASTER_CLKDIV_2,0xff);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(34)  PCM512x_FS_SPEED_MODE",PCM512x_FS_SPEED_MODE,PCM512x_FSSP);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(35)  PCM512x_IDAC_1",PCM512x_IDAC_1,0xff);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(36)  PCM512x_IDAC_2",PCM512x_IDAC_2,0xff);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(37)  PCM512x_ERROR_DETECT",PCM512x_ERROR_DETECT,0x7f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(40)  PCM512x_I2S_1",PCM512x_I2S_1,0x33);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(41)  PCM512x_I2S_2",PCM512x_I2S_2,0xff);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(42)  PCM512x_DAC_ROUTING",PCM512x_DAC_ROUTING,0x33);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(43)  PCM512x_DSP_PROGRAM",PCM512x_DSP_PROGRAM,0x1f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(44)  PCM512x_CLKDET",PCM512x_CLKDET,0x07);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(59)  PCM512x_AUTO_MUTE",PCM512x_AUTO_MUTE,0x77);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(60)  PCM512x_DIGITAL_VOLUME_1",PCM512x_DIGITAL_VOLUME_1,0x03);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(61)  PCM512x_DIGITAL_VOLUME_2",PCM512x_DIGITAL_VOLUME_2,0xff);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(62)  PCM512x_DIGITAL_VOLUME_3",PCM512x_DIGITAL_VOLUME_3,0xff);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(63)  PCM512x_DIGITAL_MUTE_1",PCM512x_DIGITAL_MUTE_1,0xff);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(64)  PCM512x_DIGITAL_MUTE_2",PCM512x_DIGITAL_MUTE_2,0xf0);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(65)  PCM512x_DIGITAL_MUTE_3",PCM512x_DIGITAL_MUTE_3,0x07);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(80)  PCM512x_GPIO_OUTPUT_1",PCM512x_GPIO_OUTPUT_1,0x0f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(81)  PCM512x_GPIO_OUTPUT_2",PCM512x_GPIO_OUTPUT_2,0x0f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(82)  PCM512x_GPIO_OUTPUT_3",PCM512x_GPIO_OUTPUT_3,0x0f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(83)  PCM512x_GPIO_OUTPUT_4",PCM512x_GPIO_OUTPUT_4,0x0f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(84)  PCM512x_GPIO_OUTPUT_5",PCM512x_GPIO_OUTPUT_5,0x0f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(85)  PCM512x_GPIO_OUTPUT_6",PCM512x_GPIO_OUTPUT_6,0x0f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(86)  PCM512x_GPIO_CONTROL_1",PCM512x_GPIO_CONTROL_1,0x3f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(87)  PCM512x_GPIO_CONTROL_2",PCM512x_GPIO_CONTROL_2,0x3f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(90)  PCM512x_OVERFLOW",PCM512x_OVERFLOW,0x1f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(91)  PCM512x_RATE_DET_1",PCM512x_RATE_DET_1,0x7f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(92)  PCM512x_RATE_DET_2",PCM512x_RATE_DET_2,0x01);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(93)  PCM512x_RATE_DET_3",PCM512x_RATE_DET_3,0xff);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(94)  PCM512x_RATE_DET_4",PCM512x_RATE_DET_4,0x7f);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(95)  PCM512x_CLOCK_STATUS",PCM512x_CLOCK_STATUS,0xff);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(108) PCM512x_ANALOG_MUTE_DET",PCM512x_ANALOG_MUTE_DET,0x03);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(119) PCM512x_GPIN",PCM512x_GPIN,0x1e);
	pcm512x_print_register(pcm512x,"Pg(0)/Reg(120) PCM512x_DIGITAL_MUTE_DET",PCM512x_DIGITAL_MUTE_DET,0x11);
	pcm512x_print_register(pcm512x,"Pg(1)/Reg(1)   PCM512x_OUTPUT_AMPLITUDE",PCM512x_OUTPUT_AMPLITUDE,0x01);
	pcm512x_print_register(pcm512x,"Pg(1)/Reg(2)   PCM512x_ANALOG_GAIN_CTRL",PCM512x_ANALOG_GAIN_CTRL,0x11);
	pcm512x_print_register(pcm512x,"Pg(1)/Reg(5)   PCM512x_UNDERVOLTAGE_PROT",PCM512x_UNDERVOLTAGE_PROT,0x03);
	pcm512x_print_register(pcm512x,"Pg(1)/Reg(6)   PCM512x_ANALOG_MUTE_CTRL",PCM512x_ANALOG_MUTE_CTRL,0x01);
	pcm512x_print_register(pcm512x,"Pg(1)/Reg(7)   PCM512x_ANALOG_GAIN_BOOST",PCM512x_ANALOG_GAIN_BOOST,0x11);
	pcm512x_print_register(pcm512x,"Pg(1)/Reg(8)   PCM512x_VCOM_CTRL_1",PCM512x_VCOM_CTRL_1,0x01);
	pcm512x_print_register(pcm512x,"Pg(1)/Reg(9)   PCM512x_VCOM_CTRL_2",PCM512x_VCOM_CTRL_2,0x01);
	pcm512x_print_register(pcm512x,"Pg(44)/Reg(1)  PCM512x_CRAM_CTRL",PCM512x_CRAM_CTRL,0x0f);
}

static void snd_rpi_hifiberry_dump_biquad_coefficients(struct pcm512x_priv *pcm512x)
{
	printk(KERN_INFO "PCM512x BiQuad Coefficients\n");

	pcm512x_print_coefficient(pcm512x,"C10 (BiQuad 1A) N0",44,48);
	pcm512x_print_coefficient(pcm512x,"C11 (BiQuad 1A) N1",44,52);
	pcm512x_print_coefficient(pcm512x,"C12 (BiQuad 1A) N2",44,56);
	pcm512x_print_coefficient(pcm512x,"C13 (BiQuad 1A) D1",44,60);
	pcm512x_print_coefficient(pcm512x,"C14 (BiQuad 1A) D2",44,64);

	pcm512x_print_coefficient(pcm512x,"C15 (BiQuad 1B) N0",44,68);
	pcm512x_print_coefficient(pcm512x,"C16 (BiQuad 1B) N1",44,72);
	pcm512x_print_coefficient(pcm512x,"C17 (BiQuad 1B) N2",44,76);
	pcm512x_print_coefficient(pcm512x,"C18 (BiQuad 1B) D1",44,80);
	pcm512x_print_coefficient(pcm512x,"C19 (BiQuad 1B) D2",44,84);

	pcm512x_print_coefficient(pcm512x,"C20 (BiQuad 1C) N0",44,88);
	pcm512x_print_coefficient(pcm512x,"C21 (BiQuad 1C) N1",44,92);
	pcm512x_print_coefficient(pcm512x,"C22 (BiQuad 1C) N2",44,96);
	pcm512x_print_coefficient(pcm512x,"C23 (BiQuad 1C) D1",44,100);
	pcm512x_print_coefficient(pcm512x,"C24 (BiQuad 1C) D2",44,104);

	pcm512x_print_coefficient(pcm512x,"C25 (BiQuad 2A) N0",44,108);
	pcm512x_print_coefficient(pcm512x,"C26 (BiQuad 2A) N1",44,112);
	pcm512x_print_coefficient(pcm512x,"C27 (BiQuad 2A) N2",44,116);
	pcm512x_print_coefficient(pcm512x,"C28 (BiQuad 2A) D1",44,120);
	pcm512x_print_coefficient(pcm512x,"C29 (BiQuad 2A) D2",44,124);

	pcm512x_print_coefficient(pcm512x,"C30 (BiQuad 2B) N0",45,8);
	pcm512x_print_coefficient(pcm512x,"C31 (BiQuad 2B) N1",45,12);
	pcm512x_print_coefficient(pcm512x,"C32 (BiQuad 2B) N2",45,16);
	pcm512x_print_coefficient(pcm512x,"C33 (BiQuad 2B) D1",45,20);
	pcm512x_print_coefficient(pcm512x,"C34 (BiQuad 2B) D2",45,24);

	pcm512x_print_coefficient(pcm512x,"C35 (BiQuad 2C) N0",45,28);
	pcm512x_print_coefficient(pcm512x,"C36 (BiQuad 2C) N1",45,32);
	pcm512x_print_coefficient(pcm512x,"C37 (BiQuad 2C) N2",45,36);
	pcm512x_print_coefficient(pcm512x,"C38 (BiQuad 2C) D1",45,40);
	pcm512x_print_coefficient(pcm512x,"C39 (BiQuad 2C) D2",45,44);

	pcm512x_print_coefficient(pcm512x,"C40 (BiQuad 3) N0",45,48);
	pcm512x_print_coefficient(pcm512x,"C41 (BiQuad 3) N1",45,52);
	pcm512x_print_coefficient(pcm512x,"C42 (BiQuad 3) N2",45,56);
	pcm512x_print_coefficient(pcm512x,"C43 (BiQuad 3) D1",45,60);
	pcm512x_print_coefficient(pcm512x,"C44 (BiQuad 3) D2",45,64);

	pcm512x_print_coefficient(pcm512x,"C45 (BiQuad 4) N0",45,68);
	pcm512x_print_coefficient(pcm512x,"C46 (BiQuad 4) N1",45,72);
	pcm512x_print_coefficient(pcm512x,"C47 (BiQuad 4) N2",45,76);
	pcm512x_print_coefficient(pcm512x,"C48 (BiQuad 4) D1",45,80);
	pcm512x_print_coefficient(pcm512x,"C49 (BiQuad 4) D2",45,84);

	pcm512x_print_coefficient(pcm512x,"C50 (BiQuad 5) N0",45,88);
	pcm512x_print_coefficient(pcm512x,"C51 (BiQuad 5) N1",45,92);
	pcm512x_print_coefficient(pcm512x,"C52 (BiQuad 5) N2",45,96);
	pcm512x_print_coefficient(pcm512x,"C53 (BiQuad 5) D1",45,100);
	pcm512x_print_coefficient(pcm512x,"C54 (BiQuad 5) D2",45,104);

	pcm512x_print_coefficient(pcm512x,"C55 (BiQuad 6) N0",45,108);
	pcm512x_print_coefficient(pcm512x,"C56 (BiQuad 6) N1",45,112);
	pcm512x_print_coefficient(pcm512x,"C57 (BiQuad 6) N2",45,116);
	pcm512x_print_coefficient(pcm512x,"C58 (BiQuad 6) D1",45,120);
	pcm512x_print_coefficient(pcm512x,"C59 (BiQuad 6) D2",45,124);

	pcm512x_print_coefficient(pcm512x,"C55 (BiQuad 7) N0",46,8);
	pcm512x_print_coefficient(pcm512x,"C56 (BiQuad 7) N1",46,12);
	pcm512x_print_coefficient(pcm512x,"C57 (BiQuad 7) N2",46,16);
	pcm512x_print_coefficient(pcm512x,"C58 (BiQuad 7) D1",46,20);
	pcm512x_print_coefficient(pcm512x,"C59 (BiQuad 7) D2",46,24);

	pcm512x_print_coefficient(pcm512x,"C60 (BiQuad 8) N0",46,28);
	pcm512x_print_coefficient(pcm512x,"C61 (BiQuad 8) N1",46,32);
	pcm512x_print_coefficient(pcm512x,"C62 (BiQuad 8) N2",46,36);
	pcm512x_print_coefficient(pcm512x,"C63 (BiQuad 8) D1",46,40);
	pcm512x_print_coefficient(pcm512x,"C64 (BiQuad 8) D2",46,44);
}

static void snd_rpi_hifiberry_dump_drc_coefficients(struct pcm512x_priv *pcm512x)
{
	printk(KERN_INFO "PCM512x DRC Coefficients\n");

	pcm512x_print_coefficient(pcm512x,"C70 (DRC_MB_1_DRC_1_DRCAE)",46,48);
	pcm512x_print_coefficient(pcm512x,"C71 (DRC_MB_1_DRC_1_DRC1AE)",46,52);
	pcm512x_print_coefficient(pcm512x,"C72 (DRC_MB_1_DRC_1_DRCAA)",46,56);
	pcm512x_print_coefficient(pcm512x,"C73 (DRC_MB_1_DRC_1_DRC1AA)",46,60);
	pcm512x_print_coefficient(pcm512x,"C74 (DRC_MB_1_DRC_1_DRCAD)",46,64);
	pcm512x_print_coefficient(pcm512x,"C75 (DRC_MB_1_DRC_1_DRC1AD)",46,68);
	pcm512x_print_coefficient(pcm512x,"C76 (DRC_MB_1_DRC_2_DRCAE)",46,72);
	pcm512x_print_coefficient(pcm512x,"C77 (DRC_MB_1_DRC_2_DRC1AE)",46,76);
	pcm512x_print_coefficient(pcm512x,"C78 (DRC_MB_1_DRC_2_DRCAA)",46,80);
	pcm512x_print_coefficient(pcm512x,"C79 (DRC_MB_1_DRC_2_DRC1AA)",46,84);
	pcm512x_print_coefficient(pcm512x,"C80 (DRC_MB_1_DRC_2_DRCAD)",46,88);
	pcm512x_print_coefficient(pcm512x,"C81 (DRC_MB_1_DRC_2_DRC1AD)",46,92);
	pcm512x_print_coefficient(pcm512x,"C82 (DRC_MB_1_DRC_3_DRCAE)",46,96);
	pcm512x_print_coefficient(pcm512x,"C83 (DRC_MB_1_DRC_3_DRC1AE)",46,100);
	pcm512x_print_coefficient(pcm512x,"C84 (DRC_MB_1_DRC_3_DRCAA)",46,104);
	pcm512x_print_coefficient(pcm512x,"C85 (DRC_MB_1_DRC_3_DRC1AA)",46,108);
	pcm512x_print_coefficient(pcm512x,"C86 (DRC_MB_1_DRC_3_DRCAD)",46,112);
	pcm512x_print_coefficient(pcm512x,"C87 (DRC_MB_1_DRC_3_DRC1AD)",46,116);
	pcm512x_print_coefficient(pcm512x,"C88 (DRC_MB_1_DRC_DRCK0)",46,120);
	pcm512x_print_coefficient(pcm512x,"C89 (DRC_MB_1_DRC_DRCK1)",46,124);
	pcm512x_print_coefficient(pcm512x,"C90 (DRC_MB_1_DRC_DRCK2)",47,8);
	pcm512x_print_coefficient(pcm512x,"C91 (DRC_MB_1_DRC_DRCMT1)",47,12);
	pcm512x_print_coefficient(pcm512x,"C92 (DRC_MB_1_DRC_DRCMT2)",47,16);
	pcm512x_print_coefficient(pcm512x,"C93 (DRC_MB_1_DRC_DRCOFF1)",47,20);
	pcm512x_print_coefficient(pcm512x,"C94 (DRC_MB_1_DRC_DRCOFF2)",47,24);
	pcm512x_print_coefficient(pcm512x,"C95 (DRC_MB_1_MinusOne_Q22)",47,28);
	pcm512x_print_coefficient(pcm512x,"C96 (DRC_MB_1_MinusTwo_Q22)",47,32);
	pcm512x_print_coefficient(pcm512x,"C97 (DRC_MB_1_One_M2)",47,36);
	pcm512x_print_coefficient(pcm512x,"C98 (DRC_MB_1_Zero)",47,40);
	pcm512x_print_coefficient(pcm512x,"C99 (DRC_MB_1_En_dB)",47,44);
	pcm512x_print_coefficient(pcm512x,"C100 (DRC_MB_1_Minus__Zero_dB)",47,48);
	pcm512x_print_coefficient(pcm512x,"C101 (DRC_MB_1_60_dB)",47,52);
	pcm512x_print_coefficient(pcm512x,"C102 (DRC_MB_1_Minus_60_dB)",47,56);
	pcm512x_print_coefficient(pcm512x,"C103 (DRC_MB_1_12_dB)",47,60);
	pcm512x_print_coefficient(pcm512x,"C104 (DRC_MB_1_Offset)",47,64);
	pcm512x_print_coefficient(pcm512x,"C105 (DRC_MB_1_K)",47,68);
	pcm512x_print_coefficient(pcm512x,"C106 (DRC_MB_1_x / DRC_MB_1_DRC)",47,72);
	pcm512x_print_coefficient(pcm512x,"C107 (DRC_MB_1_48_dB)",47,76);
	pcm512x_print_coefficient(pcm512x,"C108 (DRC_MB_1_Minus_48_dB)",47,80);
	pcm512x_print_coefficient(pcm512x,"C109 (DRC_MB_1_c1_3)",47,84);
	pcm512x_print_coefficient(pcm512x,"C110 (DRC_MB_1_c1_2)",47,88);
	pcm512x_print_coefficient(pcm512x,"C111 (DRC_MB_1_c1_1)",47,92);
	pcm512x_print_coefficient(pcm512x,"C112 (DRC_MB_1_c1_0)",47,96);
	pcm512x_print_coefficient(pcm512x,"C113 (DRC_MB_1_O1_1)",47,100);
	pcm512x_print_coefficient(pcm512x,"C114 (DRC_MB_1_S1_1)",47,104);
	pcm512x_print_coefficient(pcm512x,"C115 (DRC_MB_1_O1_2)",47,108);
	pcm512x_print_coefficient(pcm512x,"C116 (DRC_MB_1_S1_2)",47,112);
	pcm512x_print_coefficient(pcm512x,"C117 (DRC_MB_1_O1_3)",47,116);
	pcm512x_print_coefficient(pcm512x,"C118 (DRC_MB_1_S1_3)",47,120);
	pcm512x_print_coefficient(pcm512x,"C119 (DRC_MB_1_One_1_Q17)",47,124);
	pcm512x_print_coefficient(pcm512x,"C120 (DRC_MB_1_Scale1)",48,8);
	pcm512x_print_coefficient(pcm512x,"C121 (DRC_MB_1_x1Coeff)",48,12);
	pcm512x_print_coefficient(pcm512x,"C122 (DRC_MB_1_c2_3)",48,16);
	pcm512x_print_coefficient(pcm512x,"C123 (DRC_MB_1_c2_2)",48,20);
	pcm512x_print_coefficient(pcm512x,"C124 (DRC_MB_1_c2_1)",48,24);
	pcm512x_print_coefficient(pcm512x,"C125 (DRC_MB_1_c2_0)",48,28);
	pcm512x_print_coefficient(pcm512x,"C126 (DRC_MB_1_O2_1)",48,32);
	pcm512x_print_coefficient(pcm512x,"C127 (DRC_MB_1_S2_1)",48,36);
	pcm512x_print_coefficient(pcm512x,"C128 (DRC_MB_1_O2_2)",48,40);
	pcm512x_print_coefficient(pcm512x,"C129 (DRC_MB_1_S2_2)",48,44);
	pcm512x_print_coefficient(pcm512x,"C130 (DRC_MB_1_O2_3)",48,48);
	pcm512x_print_coefficient(pcm512x,"C131 (DRC_MB_1_S2_3)",48,52);
	pcm512x_print_coefficient(pcm512x,"C132 (DRC_MB_1_One_2_Q17)",48,56);
	pcm512x_print_coefficient(pcm512x,"C133 (DRC_MB_1_Scale2)",48,60);
	pcm512x_print_coefficient(pcm512x,"C134 (DRC_MB_1_x2Coeff)",48,64);
	pcm512x_print_coefficient(pcm512x,"C135 (DRC_MB_1_R1_1)",48,68);
	pcm512x_print_coefficient(pcm512x,"C136 (DRC_MB_1_R1_2)",48,72);
	pcm512x_print_coefficient(pcm512x,"C137 (DRC_MB_1_R2_1)",48,76);
	pcm512x_print_coefficient(pcm512x,"C138 (DRC_MB_1_R2_2)",48,80);
	pcm512x_print_coefficient(pcm512x,"C139 (DRC_MB_1_Band1_GainC)",48,84);
	pcm512x_print_coefficient(pcm512x,"C140 (DRC_MB_1_Band2_GainC)",48,88);
	pcm512x_print_coefficient(pcm512x,"C141 (DRC_MB_1_Band3_GainC)",48,92);
	pcm512x_print_coefficient(pcm512x,"C142 (DRC_MB_1_MinusOne_M1)",48,96);
	pcm512x_print_coefficient(pcm512x,"C143 (DRC_MB_1_One_M1)",48,100);
	pcm512x_print_coefficient(pcm512x,"C144 (DRC_MB_1_Band1_GainE)",48,104);
	pcm512x_print_coefficient(pcm512x,"C145 (DRC_MB_1_Band2_GainE)",48,108);
	pcm512x_print_coefficient(pcm512x,"C146 (DRC_MB_1_Band3_GainE)",48,112);
	pcm512x_print_coefficient(pcm512x,"C147 (DRC_MB_1_minus_One_M2)",48,116);
}

static void snd_rpi_hifiberry_dump_stereo_coefficients(struct pcm512x_priv *pcm512x)
{
	printk(KERN_INFO "PCM512x Stereo Mixer Coefficients\n");
	pcm512x_print_coefficient(pcm512x,"C148 (Stereo_Mixer_1_MixGain1)",48,120);
	pcm512x_print_coefficient(pcm512x,"C149 (Stereo_Mixer_1_MixGain2)",48,124);
	pcm512x_print_coefficient(pcm512x,"C150 (Stereo_Mixer_1_MixGain3)",49,8);

	printk(KERN_INFO "PCM512x Stereo Multiplexer Select Coefficient\n");
	pcm512x_print_coefficient(pcm512x,"C152 (Stereo_Mux_1_MuxSelect)",49,16);

	printk(KERN_INFO "PCM512x Stereo Multiplexer Input Coefficient\n");
	pcm512x_print_coefficient(pcm512x,"C153 (C_to_D_1_Coefval C_to_D_2_Coefval)",49,20);

	printk(KERN_INFO "PCM512x Mono Mixer\n");
	pcm512x_print_coefficient(pcm512x,"C154 (Mono_Mixer_1_MixGain1)",49,24);
	pcm512x_print_coefficient(pcm512x,"C155 (Mono_Mixer_1_MixGain2)",49,28);
}

static void snd_rpi_hifiberry_dump_volume_coefficients(struct pcm512x_priv *pcm512x)
{
	printk(KERN_INFO "PCM512x Master Volume Control\n");
	pcm512x_print_coefficient(pcm512x,"C158 (Volume_ZeroX_1_volcmd)",49,40);
	pcm512x_print_coefficient(pcm512x,"C159 (Volume_ZeroX_1_volout)",49,44);
	pcm512x_print_coefficient(pcm512x,"C160 (Volume_ZeroX_1_volout_loudne)",49,48);
	pcm512x_print_coefficient(pcm512x,"C161 (Volume_ZeroX_1_MinusOne_M2)",49,52);
	pcm512x_print_coefficient(pcm512x,"C162 (Volume_ZeroX_1_workingval_1_ pre_CRAM)",49,56);
	pcm512x_print_coefficient(pcm512x,"C163 (Volume_ZeroX_1_volout_pre1)",49,60);
	pcm512x_print_coefficient(pcm512x,"C164 (Volume_ZeroX_1_workingval_2_ pre_CRAM)",49,64);
	pcm512x_print_coefficient(pcm512x,"C165 (Volume_ZeroX_1_volout_pre2)",49,68);
	pcm512x_print_coefficient(pcm512x,"C166 (Volume_ZeroX_1_workingval_3_ pre_CRAM)",49,72);
	pcm512x_print_coefficient(pcm512x,"C167 (Volume_ZeroX_1_volout_pre3)",49,76);
	pcm512x_print_coefficient(pcm512x,"C168 (Volume_ZeroX_1_One_M2)",49,80);
	pcm512x_print_coefficient(pcm512x,"C169 (Volume_ZeroX_1_Zero)",49,84);
	pcm512x_print_coefficient(pcm512x,"C170 (MinusOne_Int)",49,88);
	pcm512x_print_coefficient(pcm512x,"C171 (MinusOne_M1)",49,92);
	pcm512x_print_coefficient(pcm512x,"C172 (One_M2)",49,96);
	pcm512x_print_coefficient(pcm512x,"C173 (One_M1)",49,100);
	pcm512x_print_coefficient(pcm512x,"C174 (Zero)",49,104);
}

static void snd_rpi_hifiberry_dump_misc_coefficients(struct pcm512x_priv *pcm512x)
{
	printk(KERN_INFO "PCM512x Misc Coefficients\n");
	pcm512x_print_coefficient(pcm512x,"C175 (DRC_MB_1_DataBlock)",49,108);
	pcm512x_print_coefficient(pcm512x,"C176 (DRC_MB_1_CoeffBlock)",49,112);
	pcm512x_print_coefficient(pcm512x,"C177 (Volume_ZeroX_1_DataBlock)",49,116);
	pcm512x_print_coefficient(pcm512x,"C178 (Volume_ZeroX_1_CoeffBlock)",49,120);
	pcm512x_print_coefficient(pcm512x,"C179 (plus_one)",49,124);
	pcm512x_print_coefficient(pcm512x,"C180 (ADD_OF_filter_in_L)",50,8);
	pcm512x_print_coefficient(pcm512x,"C181 (ADD_OF_filter_in_R)",50,12);
}
*/

static void snd_rpi_hifiberry_dump(struct snd_soc_codec *codec)
{
	/*
	struct pcm512x_priv *pcm512x = snd_soc_codec_get_drvdata(codec);
	
	snd_rpi_hifiberry_dump_registers(pcm512x);
	
	snd_soc_update_bits(codec,PCM512x_POWER,0x10,0x10);
	msleep(2);
	
	snd_rpi_hifiberry_dump_biquad_coefficients(pcm512x);
	snd_rpi_hifiberry_dump_drc_coefficients(pcm512x);
	snd_rpi_hifiberry_dump_stereo_coefficients(pcm512x);
	snd_rpi_hifiberry_dump_volume_coefficients(pcm512x);
	snd_rpi_hifiberry_dump_misc_coefficients(pcm512x);
	
	snd_soc_update_bits(codec,PCM512x_POWER,0x10,0x00);
	msleep(2);
	*/
}

/*-------------------------------------------------------------------------------------------*/
/* Master Volume Controls */
/*-------------------------------------------------------------------------------------------*/

static void snd_rpi_hifiberry_dacplus_ctl_set_map(struct snd_kcontrol *kctrl)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kctrl);

	if(driver_data != NULL && component->regmap == NULL)
		component->regmap = driver_data->regmap;
}

static int snd_rpi_hifiberry_dacplus_switch_ctl_get(struct snd_kcontrol *kctrl,
                                                    struct snd_ctl_elem_value *uctrl)
{
	snd_rpi_hifiberry_dacplus_ctl_set_map(kctrl);
	return snd_soc_get_volsw_range(kctrl,uctrl);
}

static int snd_rpi_hifiberry_dacplus_switch_ctl_put(struct snd_kcontrol *kctrl,
                                                    struct snd_ctl_elem_value *uctrl)
{
	snd_rpi_hifiberry_dacplus_ctl_set_map(kctrl);
	return snd_soc_put_volsw_range(kctrl,uctrl);
}

static int snd_rpi_hifiberry_dacplus_mute_ctl_get(struct snd_kcontrol *kctrl,
                                                  struct snd_ctl_elem_value *uctrl)
{
	snd_rpi_hifiberry_dacplus_ctl_set_map(kctrl);
	return snd_soc_get_volsw(kctrl,uctrl);
}

static int snd_rpi_hifiberry_dacplus_mute_ctl_put(struct snd_kcontrol *kctrl,
                                                  struct snd_ctl_elem_value *uctrl)
{
	snd_rpi_hifiberry_dacplus_ctl_set_map(kctrl);
	return snd_soc_put_volsw(kctrl,uctrl);
}

static const DECLARE_TLV_DB_SCALE(digital_tlv,-10350,50,1);

/*-------------------------------------------------------------------------------------------*/
/* BiQuad Controls */
/*-------------------------------------------------------------------------------------------*/

static int snd_rpi_hifiberry_dacplus_coeff_index(struct snd_ctl_elem_value *uinfo)
{
	int filter_index,coeff_index,sub_index;
	
	filter_index = (int)(uinfo->id.name[7] - '1');
	switch(filter_index)
	{
		case 0:
		case 1:
			sub_index = (int)(uinfo->id.name[21] - 'A');
			coeff_index = (filter_index * 3) + sub_index;
			break;
			
		default:
			coeff_index = filter_index + 4;
			break;
	}
	return coeff_index;
}

static int snd_rpi_hifiberry_dacplus_biquad_ctl_get(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *uinfo)
{
	int cIndex = snd_rpi_hifiberry_dacplus_coeff_index(uinfo);
	
	if(!(cIndex>=0 && cIndex<12))
		return -EINVAL;
		
	if(driver_data == NULL)
		return -ENODEV;
	
	mutex_lock(&driver_data->dac_lock);
	memcpy(uinfo->value.bytes.data,driver_data->filters[cIndex].coeff,165);
	mutex_unlock(&driver_data->dac_lock);
	
	return 0;
}

static int snd_rpi_hifiberry_dacplus_biquad_ctl_put(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value *uinfo)
{
	int cIndex = snd_rpi_hifiberry_dacplus_coeff_index(uinfo);
	
	if(!(cIndex>=0 && cIndex<12))
		return -EINVAL;
		
	if(driver_data == NULL)
		return -ENODEV;
	
	mutex_lock(&driver_data->dac_lock);
	driver_data->filters[cIndex].is_active = true;
	memcpy(driver_data->filters[cIndex].coeff,uinfo->value.bytes.data,165);
	mutex_unlock(&driver_data->dac_lock);
	
	return 0;
}

static const struct snd_kcontrol_new rpi_hifiberry_dacplus_snd_controls[] = {
{
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Master Volume",
	.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ
		 | SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.tlv.p = digital_tlv,
	.info = snd_soc_info_volsw_range,
	.get = snd_rpi_hifiberry_dacplus_switch_ctl_get,
	.put = snd_rpi_hifiberry_dacplus_switch_ctl_put,
	.private_value = SOC_DOUBLE_R_RANGE_VALUE(
	                                          PCM512x_DIGITAL_VOLUME_2,
	                                          PCM512x_DIGITAL_VOLUME_3,
	                                          0,
	                                          48,
	                                          255,
	                                          1)
},
{
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Master Playback Switch",
	.info = snd_soc_info_volsw,
	.get = snd_rpi_hifiberry_dacplus_mute_ctl_get,
	.put = snd_rpi_hifiberry_dacplus_mute_ctl_put,
	.private_value = SOC_DOUBLE_VALUE(PCM512x_MUTE,
	                                  PCM512x_RQML_SHIFT,
	                                  PCM512x_RQMR_SHIFT,
	                                  1,
	                                  1,
	                                  0)
},

	SND_SOC_BYTES_EXT("BiQuad 1 Coefficient A",165,snd_rpi_hifiberry_dacplus_biquad_ctl_get,snd_rpi_hifiberry_dacplus_biquad_ctl_put), /*  0 */
	SND_SOC_BYTES_EXT("BiQuad 1 Coefficient B",165,snd_rpi_hifiberry_dacplus_biquad_ctl_get,snd_rpi_hifiberry_dacplus_biquad_ctl_put), /*  1 */
	SND_SOC_BYTES_EXT("BiQuad 1 Coefficient C",165,snd_rpi_hifiberry_dacplus_biquad_ctl_get,snd_rpi_hifiberry_dacplus_biquad_ctl_put), /*  2 */
	SND_SOC_BYTES_EXT("BiQuad 2 Coefficient A",165,snd_rpi_hifiberry_dacplus_biquad_ctl_get,snd_rpi_hifiberry_dacplus_biquad_ctl_put), /*  3 */
	SND_SOC_BYTES_EXT("BiQuad 2 Coefficient B",165,snd_rpi_hifiberry_dacplus_biquad_ctl_get,snd_rpi_hifiberry_dacplus_biquad_ctl_put), /*  4 */
	SND_SOC_BYTES_EXT("BiQuad 2 Coefficient C",165,snd_rpi_hifiberry_dacplus_biquad_ctl_get,snd_rpi_hifiberry_dacplus_biquad_ctl_put), /*  5 */
	SND_SOC_BYTES_EXT("BiQuad 3 Coefficient",165,snd_rpi_hifiberry_dacplus_biquad_ctl_get,snd_rpi_hifiberry_dacplus_biquad_ctl_put),   /*  6 */
	SND_SOC_BYTES_EXT("BiQuad 4 Coefficient",165,snd_rpi_hifiberry_dacplus_biquad_ctl_get,snd_rpi_hifiberry_dacplus_biquad_ctl_put),   /*  7 */
	SND_SOC_BYTES_EXT("BiQuad 5 Coefficient",165,snd_rpi_hifiberry_dacplus_biquad_ctl_get,snd_rpi_hifiberry_dacplus_biquad_ctl_put),   /*  8 */
	SND_SOC_BYTES_EXT("BiQuad 6 Coefficient",165,snd_rpi_hifiberry_dacplus_biquad_ctl_get,snd_rpi_hifiberry_dacplus_biquad_ctl_put),   /*  9 */
	SND_SOC_BYTES_EXT("BiQuad 7 Coefficient",165,snd_rpi_hifiberry_dacplus_biquad_ctl_get,snd_rpi_hifiberry_dacplus_biquad_ctl_put),   /* 10 */
	SND_SOC_BYTES_EXT("BiQuad 8 Coefficient",165,snd_rpi_hifiberry_dacplus_biquad_ctl_get,snd_rpi_hifiberry_dacplus_biquad_ctl_put)    /* 11 */
};

static int snd_rpi_hifiberry_dacplus_sample_rate_index(int sample_rate)
{
	int idx;
	
	switch(sample_rate)
	{
		case 8000:
			idx = 0; break;
		case 11025:
			idx = 1; break;
		case 16000:
			idx = 2; break;
		case 22050:
			idx = 3; break;
		case 32000:
			idx = 4; break;
		case 44100:
			idx = 5; break;
		case 48000:
			idx = 6; break;
		case 88200:
			idx = 7; break;
		case 96000:
			idx = 8; break;
		case 176400:
			idx = 9; break;
		case 192000:
			idx = 10; break;
		default:
			idx = -1; break;
	}
	return idx;
}

static int snd_rpi_hifiberry_dacplus_apply_coefficients(struct snd_soc_codec *codec,int fIndex)
{
	int i,filterIndex,coIndex;
	int page = 44,reg = 48,res = 0;
	struct pcm512x_priv *pcm512x = snd_soc_codec_get_drvdata(codec);
	
	mutex_lock(&driver_data->dac_lock);
		
	for(filterIndex = 0; filterIndex < 12 && !res; filterIndex++)
	{
		if(reg >= 128)
		{
			reg = 8;
			page++;
		}
		
		if(driver_data->filters[filterIndex].is_active)
		{
			for(coIndex = 0; coIndex < 5 && !res; coIndex++)
			{
				for(i = 0; i < 3; i++,reg++)
				{
					res = regmap_write(pcm512x->regmap,PCM512x_PAGE_BASE(page) + reg,(unsigned int)driver_data->filters[filterIndex].coeff[fIndex][coIndex][i]);
				}
				reg++;
			}
		}
	}
	
	snd_rpi_hifiberry_dump(codec);
	
	mutex_unlock(&driver_data->dac_lock);
	return res;
}

static void snd_rpi_hifiberry_dacplus_select_clk(struct snd_soc_codec *codec,
                                                 int clk_id)
{
	switch(clk_id) {
		case HIFIBERRY_DACPRO_NOCLOCK:
			snd_soc_update_bits(codec,PCM512x_GPIO_CONTROL_1,0x24,0x00);
			break;
		case HIFIBERRY_DACPRO_CLK44EN:
			snd_soc_update_bits(codec,PCM512x_GPIO_CONTROL_1,0x24,0x20);
			break;
		case HIFIBERRY_DACPRO_CLK48EN:
			snd_soc_update_bits(codec,PCM512x_GPIO_CONTROL_1,0x24,0x04);
			break;
	}
}

static void snd_rpi_hifiberry_dacplus_clk_gpio(struct snd_soc_codec *codec)
{
	snd_soc_update_bits(codec,PCM512x_GPIO_EN,0x24,0x24);
	snd_soc_update_bits(codec,PCM512x_GPIO_OUTPUT_3,0x0f,0x02);
	snd_soc_update_bits(codec,PCM512x_GPIO_OUTPUT_6,0x0f,0x02);
}

static bool snd_rpi_hifiberry_dacplus_is_sclk(struct snd_soc_codec *codec)
{
	int sck;

	sck = snd_soc_read(codec,PCM512x_RATE_DET_4);
	return (!(sck & 0x40));
}

static bool snd_rpi_hifiberry_dacplus_is_sclk_sleep(
                                                    struct snd_soc_codec *codec)
{
	msleep(2);
	return snd_rpi_hifiberry_dacplus_is_sclk(codec);
}

static bool snd_rpi_hifiberry_dacplus_is_pro_card(struct snd_soc_codec *codec)
{
	bool isClk44EN,isClk48En,isNoClk;

	snd_rpi_hifiberry_dacplus_clk_gpio(codec);

	snd_rpi_hifiberry_dacplus_select_clk(codec,HIFIBERRY_DACPRO_CLK44EN);
	isClk44EN = snd_rpi_hifiberry_dacplus_is_sclk_sleep(codec);

	snd_rpi_hifiberry_dacplus_select_clk(codec,HIFIBERRY_DACPRO_NOCLOCK);
	isNoClk = snd_rpi_hifiberry_dacplus_is_sclk_sleep(codec);

	snd_rpi_hifiberry_dacplus_select_clk(codec,HIFIBERRY_DACPRO_CLK48EN);
	isClk48En = snd_rpi_hifiberry_dacplus_is_sclk_sleep(codec);

	return (isClk44EN && isClk48En && !isNoClk);
}

static int snd_rpi_hifiberry_dacplus_clk_for_rate(int sample_rate)
{
	int type;

	switch(sample_rate) {
		case 11025:
		case 22050:
		case 44100:
		case 88200:
		case 176400:
			type = HIFIBERRY_DACPRO_CLK44EN;
			break;
		default:
			type = HIFIBERRY_DACPRO_CLK48EN;
			break;
	}
	return type;
}

static void snd_rpi_hifiberry_dacplus_set_sclk(struct snd_soc_codec *codec,
                                               int sample_rate)
{
	struct pcm512x_priv *pcm512x = snd_soc_codec_get_drvdata(codec);

	if(!IS_ERR(pcm512x->sclk)) {
		int ctype;

		ctype = snd_rpi_hifiberry_dacplus_clk_for_rate(sample_rate);
		clk_set_rate(pcm512x->sclk,
		             (ctype == HIFIBERRY_DACPRO_CLK44EN)
		             ? CLK_44EN_RATE : CLK_48EN_RATE);
		snd_rpi_hifiberry_dacplus_select_clk(codec,ctype);
	}
}

static int snd_rpi_hifiberry_dacplus_init_driver_data(struct device *dev,struct snd_soc_codec *codec)
{
	int i,j;

	if(driver_data != NULL)
		return 0;

	driver_data = kzalloc(sizeof(struct dacplus_driver_data),GFP_KERNEL);
	if(driver_data == NULL)
		return -ENOMEM;
	
	driver_data->regmap = codec->component.regmap;
	driver_data->is_dacpro = false;
	mutex_init(&driver_data->dac_lock);
	
	for(i=0;i<12;i++)
	{
		struct dacplus_filter_ctl *c = &driver_data->filters[i];
		
		c->is_active = false;
		for(j=0;j<11;j++)
		{
			c->coeff[j][0][0] = 0x7f;
			c->coeff[j][0][1] = 0xff;
			c->coeff[j][0][2] = 0xff;
		}
	}
	return 0;
}

static int snd_rpi_hifiberry_dacplus_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;
	struct pcm512x_priv *priv;

	err = snd_rpi_hifiberry_dacplus_init_driver_data(rtd->card->dev,codec);
	if(err)
		return err;

	driver_data->is_dacpro = snd_rpi_hifiberry_dacplus_is_pro_card(codec);

	if(driver_data->is_dacpro) {
		struct snd_soc_dai_link *dai = rtd->dai_link;

		dai->name = "HiFiBerry DAC+ Pro";
		dai->stream_name = "HiFiBerry DAC+ Pro HiFi";
		dai->dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF
			| SND_SOC_DAIFMT_CBM_CFM;

		snd_soc_update_bits(codec,PCM512x_BCLK_LRCLK_CFG,0x31,0x11);
		snd_soc_update_bits(codec,PCM512x_MASTER_MODE,0x03,0x03);
		snd_soc_update_bits(codec,PCM512x_MASTER_CLKDIV_2,0x7f,63);
	}
	else {
		priv = snd_soc_codec_get_drvdata(codec);
		priv->sclk = ERR_PTR(-ENOENT);
	}

	snd_soc_update_bits(codec,PCM512x_GPIO_EN,0x08,0x08);
	snd_soc_update_bits(codec,PCM512x_GPIO_OUTPUT_4,0x0f,0x02);
	snd_soc_update_bits(codec,PCM512x_GPIO_CONTROL_1,0x08,0x08);
	
	return err;
}

static int snd_rpi_hifiberry_dacplus_update_rate_den(
                                                     struct snd_pcm_substream *substream,
                                                     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct pcm512x_priv *pcm512x = snd_soc_codec_get_drvdata(codec);
	struct snd_ratnum *rats_no_pll;
	unsigned int num = 0,den = 0;
	int err;

	rats_no_pll = devm_kzalloc(rtd->dev,sizeof(*rats_no_pll),GFP_KERNEL);
	if(!rats_no_pll)
		return -ENOMEM;

	rats_no_pll->num = clk_get_rate(pcm512x->sclk) / 64;
	rats_no_pll->den_min = 1;
	rats_no_pll->den_max = 128;
	rats_no_pll->den_step = 1;

	err = snd_interval_ratnum(hw_param_interval(params,
	                                            SNDRV_PCM_HW_PARAM_RATE),
	                          1,
	                          rats_no_pll,
	                          &num,
	                          &den);
	if(err >= 0 && den) {
		params->rate_num = num;
		params->rate_den = den;
	}

	devm_kfree(rtd->dev,rats_no_pll);
	return 0;
}

static int snd_rpi_hifiberry_dacplus_set_bclk_ratio_pro(
                                                        struct snd_soc_dai *cpu_dai,
                                                        struct snd_pcm_hw_params *params)
{
	int bratio = snd_pcm_format_physical_width(params_format(params))
		* params_channels(params);
	return snd_soc_dai_set_bclk_ratio(cpu_dai,bratio);
}

static int snd_rpi_hifiberry_dacplus_hw_params(
                                               struct snd_pcm_substream *substream,
                                               struct snd_pcm_hw_params *params)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec;
	
	if(driver_data == NULL)
		return -ENODEV;
	
	if(driver_data->is_dacpro) {
		snd_rpi_hifiberry_dacplus_set_sclk(codec,
		                                   params_rate(params));

		ret = snd_rpi_hifiberry_dacplus_set_bclk_ratio_pro(cpu_dai,
		                                                   params);
		if(!ret)
			ret = snd_rpi_hifiberry_dacplus_update_rate_den(
			                                                substream,
			                                                params);
	}
	else {
		ret = snd_soc_dai_set_bclk_ratio(cpu_dai,64);
	}
	
	if(!ret)
	{
		int freqIndex = snd_rpi_hifiberry_dacplus_sample_rate_index(params_rate(params));
		if(freqIndex > 0)
		{
			ret = snd_rpi_hifiberry_dacplus_apply_coefficients(codec,freqIndex);
		}
	}
	
	return ret;
}

static int snd_rpi_hifiberry_dacplus_startup(
                                             struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;

	snd_soc_update_bits(codec,PCM512x_POWER,0x10,0x10);
	snd_soc_update_bits(codec,PCM512x_DSP_PROGRAM,0x1f,0x05);
	snd_soc_update_bits(codec,PCM512x_POWER,0x10,0x00);
	
	snd_soc_update_bits(codec,PCM512x_GPIO_CONTROL_1,0x08,0x08);
	
	return 0;
}

static void snd_rpi_hifiberry_dacplus_shutdown(
                                               struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;

	snd_soc_update_bits(codec,PCM512x_GPIO_CONTROL_1,0x08,0x00);
}

/* machine stream operations */
static struct snd_soc_ops snd_rpi_hifiberry_dacplus_ops = {
	.hw_params = snd_rpi_hifiberry_dacplus_hw_params,
	.startup = snd_rpi_hifiberry_dacplus_startup,
	.shutdown = snd_rpi_hifiberry_dacplus_shutdown,
};

static struct snd_soc_dai_link snd_rpi_hifiberry_dacplus_dai[] = {
{
	.name		= "HiFiBerry DAC+",
	.stream_name	= "HiFiBerry DAC+ HiFi",
	.cpu_dai_name	= "bcm2708-i2s.0",
	.codec_dai_name	= "pcm512x-hifi",
	.platform_name	= "bcm2708-i2s.0",
	.codec_name	= "pcm512x.1-004d",
	.dai_fmt	= SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBS_CFS,
	.ops		= &snd_rpi_hifiberry_dacplus_ops,
	.init		= snd_rpi_hifiberry_dacplus_init,
},
};

/* audio machine driver */
static struct snd_soc_card snd_rpi_hifiberry_dacplus = {
	.name         = "snd_rpi_hifiberry_dacplus",
	.dai_link     = snd_rpi_hifiberry_dacplus_dai,
	.num_links    = ARRAY_SIZE(snd_rpi_hifiberry_dacplus_dai),
	.controls = rpi_hifiberry_dacplus_snd_controls,
	.num_controls = ARRAY_SIZE(rpi_hifiberry_dacplus_snd_controls)
};

static int snd_rpi_hifiberry_dacplus_probe(struct platform_device *pdev)
{
	int ret = 0;

	snd_rpi_hifiberry_dacplus.dev = &pdev->dev;
	if(pdev->dev.of_node) {
		struct device_node *i2s_node;
		struct snd_soc_dai_link *dai;

		dai = &snd_rpi_hifiberry_dacplus_dai[0];
		i2s_node = of_parse_phandle(pdev->dev.of_node,
		                            "i2s-controller",
		                            0);

		if(i2s_node) {
			dai->cpu_dai_name = NULL;
			dai->cpu_of_node = i2s_node;
			dai->platform_name = NULL;
			dai->platform_of_node = i2s_node;
		}
	}

	ret = snd_soc_register_card(&snd_rpi_hifiberry_dacplus);
	if(ret)
	{
		dev_err(&pdev->dev,
		        "snd_soc_register_card() failed: %d\n",
		        ret);		
	}

	return ret;
}

static int snd_rpi_hifiberry_dacplus_remove(struct platform_device *pdev)
{
	if(driver_data != NULL)
	{
		kfree(driver_data);
		driver_data = NULL;
	}
	return snd_soc_unregister_card(&snd_rpi_hifiberry_dacplus);
}

static const struct of_device_id snd_rpi_hifiberry_dacplus_of_match[] = {
	{.compatible = "hifiberry,hifiberry-dacplus",},
{},
};
MODULE_DEVICE_TABLE(of,snd_rpi_hifiberry_dacplus_of_match);

static struct platform_driver snd_rpi_hifiberry_dacplus_driver = {
	.driver = {
	.name   = "snd-rpi-hifiberry-dacplus",
	.owner  = THIS_MODULE,
	.of_match_table = snd_rpi_hifiberry_dacplus_of_match,
},
	.probe          = snd_rpi_hifiberry_dacplus_probe,
	.remove         = snd_rpi_hifiberry_dacplus_remove,
};

module_platform_driver(snd_rpi_hifiberry_dacplus_driver);

MODULE_AUTHOR("Daniel Matuschek <daniel@hifiberry.com>");
MODULE_DESCRIPTION("ASoC Driver for HiFiBerry DAC+");
MODULE_LICENSE("GPL v2");
