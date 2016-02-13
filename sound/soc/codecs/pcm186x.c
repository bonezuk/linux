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

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>

#include "pcm186x.h"

/*----------------------------------------------------------------------------------*/

struct pcm186x_priv
{
	struct regmap *regmap;
};

/*----------------------------------------------------------------------------------*/
/* I2C Address definition */
/*----------------------------------------------------------------------------------*/

static const struct i2c_device_id pcm186x_i2c_id[] = {
	{ "pcm1861" },
	{ "pcm1863" },
	{ "pcm1865" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pcm186x_i2c_id);

static const struct of_device_id pcm186x_of_match[] = {
	{ .compatible = "ti,pcm1861", },
	{ .compatible = "ti,pcm1863", },
	{ .compatible = "ti,pcm1865", },
	{ }
};
MODULE_DEVICE_TABLE(of, pcm186x_of_match);

static const struct reg_default pcm186x_reg_defaults[] = {
	{ PCM186x_PGA_VAL_CH1_L, 0x00 },
	{ PCM186x_PGA_VAL_CH1_R, 0x00 },
	{ PCM186x_PGA_VAL_CH2_L, 0x00 },
	{ PCM186x_PGA_VAL_CH2_R, 0x00 },
	{ PCM186x_PGA_CLIP_CONTROL, 0x86 },
	{ PCM186x_ADC1_INPUT_SELECT_L, 0x41 },
	{ PCM186x_ADC1_INPUT_SELECT_R, 0x41 },	
	{ PCM186x_ADC2_INPUT_SELECT_L, 0x42 },
	{ PCM186x_ADC2_INPUT_SELECT_R, 0x42 },	
	{ PCM186x_ADC2_INPUT_CHANNEL, 0x00 },
	{ PCM186x_PCM_FORMAT, 0x44 },
	{ PCM186x_TDM_SELECT, 0x00 },
	{ PCM186x_TX_TDM_OFFSET, 0x00 },
	{ PCM186x_RX_TDM_OFFSET, 0x00 },
	{ PCM186x_DPGA_VAL_CH1_L, 0x00 },
	{ PCM186x_GPIO_0_AND_1_CTRL, 0x01 },
	{ PCM186x_GPIO_2_AND_3_CTRL, 0x20 },
	{ PCM186x_GPIO_0_AND_1_DIR, 0x00 },
	{ PCM186x_GPIO_2_AND_3_DIR, 0x00 },
	{ PCM186x_GPIO_STATE, 0x00 },
	{ PCM186x_GPIO_PULL_DOWN, 0x00 },
	{ PCM186x_DPGA_VAL_CH1_R, 0x00 },
	{ PCM186x_DPGA_VAL_CH2_L, 0x00 },
	{ PCM186x_DPGA_VAL_CH2_R, 0x00 },
	{ PCM186x_PGA_GAIN_MAP, 0x00 },
	{ PCM186x_DIGMIC_INPUT, 0x00 },
	{ PCM186x_DIN_RESAMPLE, 0x00 },
	{ PCM186x_CLK_SELECT, 0x01 },
	{ PCM186x_DSP1_CLK_DIVIDER, 0x01 },
	{ PCM186x_DSP2_CLK_DIVIDER, 0x01 },
	{ PCM186x_ADC_CLK_DIVIDER, 0x03 },
	{ PCM186x_PLL_SCK_DIVIDER, 0x07 },
	{ PCM186x_SCK_TO_BCK_DIVIDER, 0x03 },
	{ PCM186x_SCK_TO_LRCK_DIVIDER, 0x3f },
	{ PCM186x_PLL_STATUS, 0x01 },
	{ PCM186x_PLL_P_DIVIDER, 0x00 },
	{ PCM186x_PLL_R_DIVIDER, 0x00 },
	{ PCM186x_PLL_J_DIVIDER, 0x01 },
	{ PCM186x_PLL_D1_DIVIDER, 0x00 },
	{ PCM186x_PLL_D2_DIVIDER, 0x00 },
	{ PCM186x_SIGDET_CHANNELS, 0x00 },
	{ PCM186x_SIGDET_INTR_MASK, 0x00 },
	{ PCM186x_SIGDET_STATUS, 0x00 },
	{ PCM186x_SIGDET_LOSS_TIME, 0x00 },
	{ PCM186x_SIGDET_SCAN_TIME, 0x00 },
	{ PCM186x_SIGDET_INT_INTVL, 0x01 },
	{ PCM186x_SIGDET_DC_REF_CH1_L, 0x80 },
	{ PCM186x_SIGDET_DC_DIFF_CH1_L, 0x7f },
	{ PCM186x_SIGDET_DC_LEVEL_CH1_L, 0x00 },
	{ PCM186x_SIGDET_DC_REF_CH1_R, 0x80 },
	{ PCM186x_SIGDET_DC_DIFF_CH1_R, 0x7f },
	{ PCM186x_SIGDET_DC_LEVEL_CH1_R, 0x00 },
	{ PCM186x_SIGDET_DC_REF_CH2_L, 0x80 },
	{ PCM186x_SIGDET_DC_DIFF_CH2_L, 0x7f },
	{ PCM186x_SIGDET_DC_LEVEL_CH2_L, 0x00 },
	{ PCM186x_SIGDET_DC_REF_CH2_R, 0x80 },
	{ PCM186x_SIGDET_DC_DIFF_CH2_R, 0x7f },
	{ PCM186x_SIGDET_DC_LEVEL_CH2_R, 0x00 },
	{ PCM186x_SIGDET_DC_REF_CH3_L, 0x80 },
	{ PCM186x_SIGDET_DC_DIFF_CH3_L, 0x7f },
	{ PCM186x_SIGDET_DC_LEVEL_CH3_L, 0x00 },
	{ PCM186x_SIGDET_DC_REF_CH3_R, 0x80 },
	{ PCM186x_SIGDET_DC_DIFF_CH3_R, 0x7f },
	{ PCM186x_SIGDET_DC_LEVEL_CH3_R, 0x00 },
	{ PCM186x_SIGDET_DC_REF_CH4_L, 0x80 },
	{ PCM186x_SIGDET_DC_DIFF_CH4_L, 0x7f },
	{ PCM186x_SIGDET_DC_LEVEL_CH4_L, 0x00 },
	{ PCM186x_SIGDET_DC_REF_CH4_R, 0x80 },
	{ PCM186x_SIGDET_DC_DIFF_CH4_R, 0x7f },
	{ PCM186x_SIGDET_DC_LEVEL_CH4_R, 0x00 },
	{ PCM186x_AUXADC_DATA_CTRL, 0x00 },
	{ PCM186x_AUXADC_DATA_LSB, 0x00 },
	{ PCM186x_AUXADC_DATA_MSB, 0x00 },
	{ PCM186x_INTR_MASK, 0x01 },
	{ PCM186x_INTR_STATUS, 0x00 },
	{ PCM186x_INTR_PROPERTIES, 0x10 },
	{ PCM186x_POWER_CTRL, 0x70 },
	{ PCM186x_DSP_CTRL, 0x10 },
	{ PCM186x_DEVICE_STATUS, 0x00 },
	{ PCM186x_CURRENT_FREQUENCY, 0x00 },
	{ PCM186x_CURRENT_CLK_RATIO, 0x00 },
	{ PCM186x_CLK_STATUS, 0x00 },
	{ PCM186x_DVDD_STATUS, 0x00 }
};

static bool pcm186x_readable(struct device *dev, unsigned int reg)
{
	return reg < 0xff;
}

static bool pcm186x_volatile(struct device *dev, unsigned int reg)
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

static int pcm186x_dai_startup(struct snd_pcm_substream *substream,struct snd_soc_dai *dai)
{
	return 0;
}

static int pcm186x_hw_params(struct snd_pcm_substream *substream,struct snd_pcm_hw_params *params,struct snd_soc_dai *dai)
{
	return 0;
}

static int pcm186x_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	return 0;
}

/*----------------------------------------------------------------------------------*/

static const struct snd_soc_dai_ops pcm186x_dai_ops = {
	.startup = pcm186x_dai_startup,
	.hw_params = pcm186x_hw_params,
	.set_fmt = pcm186x_set_fmt,
};

static struct snd_soc_dai_driver pcm186x_dai = {
	.name = "pcm186x-hifi",
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_44100,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE
	},
	.ops = &pcm186x_dai_ops,
};

static struct snd_soc_codec_driver pcm186x_soc_codec_dev;

/*----------------------------------------------------------------------------------*/

static int pcm186x_i2c_probe(struct i2c_client *i2c,const struct i2c_device_id *id)
{
	struct regmap *regmap;
	struct pcm186x_priv *pcm186x;
	struct regmap_config config = pcm186x_regmap;

	/* msb needs to be set to enable auto-increment of addresses */
	config.read_flag_mask = 0x80;
	config.write_flag_mask = 0x80;

	regmap = devm_regmap_init_i2c(i2c, &config);
	if(IS_ERR(regmap))
	{
		return PTR_ERR(regmap);
	}
	
	pcm186x = devm_kzalloc(&i2c->dev,sizeof(struct pcm186x_priv),GFP_KERNEL);
	if(pcm186x==NULL)
	{
		return -ENOMEM;
	}
	
	dev_set_drvdata(&i2c->dev,pcm186x);
	pcm186x->regmap = regmap;

	return snd_soc_register_codec(&i2c->dev, &pcm186x_soc_codec_dev, &pcm186x_dai, 1);

}

/*----------------------------------------------------------------------------------*/

static int pcm186x_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);
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
