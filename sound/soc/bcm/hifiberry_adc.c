/*
 * Driver for the HiFiBerry PCM186x ADC
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

#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>

#include <sound/soc/codecs/pcm512x.h>

/*----------------------------------------------------------------------------------*/

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

#define PCM186x_MAX_REGISTER                    (PCM186x_PAGE_BASE(253) + 20)

/*----------------------------------------------------------------------------------*/

static int hifiberry_ad_adc_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

/*----------------------------------------------------------------------------------*/
/*
static int pcm186x_set_pllclk_as_required(struct snd_soc_codec *codec,int pll_rate,int mclk_rate)
{
	int g,R,P,rate;
	
	g = (int)gcd((unsigned long)pll_rate,(unsigned long)mclk_rate);
	R = pll_rate / g;
	
	if (R==1) 
	{ 
		// Use PLL is not required
		rate = mclk_rate;
		// 1001 0000
		snd_soc_update_bits(codec, 32, 0xff, 0x90);
		// ---- -- 0 0
		snd_soc_update_bits(codec, 40, 0x03, 0x00);
	}
	else 
	{
		// Use PLL
		P = mclk_rate / g;
		// 1001 1110
		snd_soc_update_bits(codec, 32, 0xff, 0x9e);
		
		while (P > 16)
		{
			
		}
		
		if (P<16)
		{
			
		}
		
		// ---- -- 0 1
		snd_soc_update_bits(codec, 40, 0x03, 0x01);
	}
	return rate;
}
*/

static int hifiberry_ad_adc_hw_params(struct snd_pcm_substream *substream,struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	
	/* Fix configuration to master SCK clock (24.576MHz), with */
	snd_soc_update_bits(codec, 32, 0xff, 0x90);
	snd_soc_update_bits(codec, 33, 0x7f, 0x01);
	snd_soc_update_bits(codec, 34, 0x7f, 0x01);
	snd_soc_update_bits(codec, 35, 0x7f, 0x03);
	snd_soc_update_bits(codec, 37, 0x7f, 0x00);
	snd_soc_update_bits(codec, 38, 0x7f, 0x0f);
	snd_soc_update_bits(codec, 39, 0xff, 0x1f);
	
	snd_soc_update_bits(codec, 11, 0xff, 0xcc);
	
	return snd_soc_dai_set_bclk_ratio(cpu_dai,16*2);
}

/*----------------------------------------------------------------------------------*/
/* machine stream operations */
/*----------------------------------------------------------------------------------*/

static struct snd_soc_ops hifiberry_ad_adc_ops = 
{
	.hw_params = hifiberry_ad_adc_hw_params,
};

/*----------------------------------------------------------------------------------*/
/* DAC+ driver */
/*----------------------------------------------------------------------------------*/

struct pcm512x_priv {
	struct regmap *regmap;
	struct clk *sclk;
};

static int snd_rpi_hifiberry_ad_dac_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct pcm512x_priv *priv;

	priv = snd_soc_codec_get_drvdata(codec);
	priv->sclk = ERR_PTR(-ENOENT);

	snd_soc_update_bits(codec, PCM512x_GPIO_EN, 0x08, 0x08);
	snd_soc_update_bits(codec, PCM512x_GPIO_OUTPUT_4, 0x0f, 0x02);
	snd_soc_update_bits(codec, PCM512x_GPIO_CONTROL_1, 0x08, 0x08);
	return 0;
}

static int snd_rpi_hifiberry_ad_dac_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	return snd_soc_dai_set_bclk_ratio(cpu_dai, 64);
}

static int snd_rpi_hifiberry_ad_dac_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	snd_soc_update_bits(codec, PCM512x_GPIO_CONTROL_1, 0x08, 0x08);
	return 0;
}

static void snd_rpi_hifiberry_ad_dac_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	snd_soc_update_bits(codec, PCM512x_GPIO_CONTROL_1, 0x08, 0x00);
}

static struct snd_soc_ops snd_rpi_hifiberry_ad_dac_ops = {
	.hw_params = snd_rpi_hifiberry_ad_dac_hw_params,
	.startup = snd_rpi_hifiberry_ad_dac_startup,
	.shutdown = snd_rpi_hifiberry_ad_dac_shutdown,
};

/*----------------------------------------------------------------------------------*/

static struct snd_soc_dai_link hifiberry_ad_dai[] = {
	{
		.name		= "HiFiBerry DAC/ADC PCM512x",
		.stream_name	= "HiFiBerry DAC/ADC PCM512 HiFi",
		.cpu_dai_name	= "bcm2708-i2s.0",
		.codec_dai_name	= "pcm512x-hifi",
		.platform_name	= "bcm2708-i2s.0",
		.codec_name	= "pcm512x.1-004d",
		.dai_fmt	= SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
		.ops		= &snd_rpi_hifiberry_ad_dac_ops,
		.init		= snd_rpi_hifiberry_ad_dac_init,
	},
	{
		.name		= "HiFiBerry DAC/ADC PCM186x",
		.stream_name	= "HiFiBerry ADC HiFi",
		.cpu_dai_name	= "bcm2708-i2s.0",
		.codec_dai_name	= "pcm186x-hifi",
		.platform_name	= "bcm2708-i2s.0",
		.codec_name	= "pcm186x.1-004a",
		.dai_fmt	= SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM,
		.ops		= &hifiberry_ad_adc_ops,
		.init		= hifiberry_ad_adc_init,
	},
};

/*----------------------------------------------------------------------------------*/

static struct snd_soc_card hifiberry_ad = {
	.name         = "hifiberry_ad",
	.dai_link     = hifiberry_ad_dai,
	.num_links    = ARRAY_SIZE(hifiberry_ad_dai),
};

/*----------------------------------------------------------------------------------*/

static int hifiberry_ad_probe(struct platform_device *pdev)
{
	int ret = 0;

	hifiberry_ad.dev = &pdev->dev;
	
	if(pdev->dev.of_node) 
	{
		struct device_node *i2s_node;
		struct snd_soc_dai_link *dai = &hifiberry_ad_dai[0];
		i2s_node = of_parse_phandle(pdev->dev.of_node,"i2s-controller",0);

		if(i2s_node) 
		{
			dai->cpu_dai_name = NULL;
			dai->cpu_of_node = i2s_node;
			dai->platform_name = NULL;
			dai->platform_of_node = i2s_node;
		}
	}
	
	ret = snd_soc_register_card(&hifiberry_ad);
	if(ret)
	{
		dev_err(&pdev->dev,"snd_soc_register_card() failed: %d\n",ret);
	}
	return ret;
}

/*----------------------------------------------------------------------------------*/

static int hifiberry_ad_remove(struct platform_device *pdev)
{
	return snd_soc_unregister_card(&hifiberry_ad);
}

/*----------------------------------------------------------------------------------*/

static const struct of_device_id hifiberry_ad_of_match[] = {
	{.compatible = "hifiberry_adc",},
{},
};
MODULE_DEVICE_TABLE(of,hifiberry_ad_of_match);

/*----------------------------------------------------------------------------------*/

static struct platform_driver hifiberry_ad_driver = {
	.driver = {
	.name   = "snd-hifiberry-adc",
	.owner  = THIS_MODULE,
	.of_match_table = hifiberry_ad_of_match,
},
	.probe          = hifiberry_ad_probe,
	.remove         = hifiberry_ad_remove,
};

/*----------------------------------------------------------------------------------*/

module_platform_driver(hifiberry_ad_driver);

MODULE_DESCRIPTION("HiFiBerry AD Driver");
MODULE_AUTHOR("Stuart MacLean <stuart@hifiberry.com>");
MODULE_LICENSE("GPL v2");

/*----------------------------------------------------------------------------------*/
