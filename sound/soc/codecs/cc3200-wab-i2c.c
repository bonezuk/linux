/*
 * Driver for the CC3200-WAB codec
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

/*----------------------------------------------------------------------------------*/

#define CC3200_WAB_VIRT_BASE 0x100
#define CC3200_WAB_PAGE_LEN  0x100
#define CC3200_WAB_PAGE_BASE(n)  (CC3200_WAB_VIRT_BASE + (CC3200_WAB_PAGE_LEN * n))
#define CC3200_WAB_PAGE              0
#define CC3200_WAB_MAX_REGISTER      (CC3200_WAB_PAGE_BASE(253) + 64)

// virtual registry definitions
#define CC3200_WAB_RESET (CC3200_WAB_PAGE_BASE(0) +   1)

/*----------------------------------------------------------------------------------*/

struct cc3200_wab_priv
{
	struct regmap *regmap;
};

/*----------------------------------------------------------------------------------*/

static const struct i2c_device_id cc3200_wab_i2c_id[] = {
	{ "cc3200-wab", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cc3200_wab_i2c_id);

static const struct of_device_id cc3200_wab_of_match[] = {
	{ .compatible = "ti,cc3200-wab", },
	{ }
};
MODULE_DEVICE_TABLE(of, cc3200_wab_of_match);

/*----------------------------------------------------------------------------------*/

static const struct reg_default cc3200_wab_reg_defaults[] = {
	{ CC3200_WAB_RESET, 0x00 },
};

static bool cc3200_wab_readable(struct device *dev, unsigned int reg)
{
	return true;
}

static bool cc3200_wab_volatile(struct device *dev, unsigned int reg)
{
	return true;
}

static const struct regmap_range_cfg cc3200_wab_range = {
	.name = "Pages", 
	.range_min = CC3200_WAB_VIRT_BASE,
	.range_max = CC3200_WAB_MAX_REGISTER,
	.selector_reg = CC3200_WAB_PAGE,
	.selector_mask = 0xff,
	.window_start = 0,
	.window_len = 0x100,
};

const struct regmap_config cc3200_wab_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.readable_reg = cc3200_wab_readable,
	.volatile_reg = cc3200_wab_volatile,

	.ranges = &cc3200_wab_range,
	.num_ranges = 1,

	.max_register = CC3200_WAB_MAX_REGISTER,
	.reg_defaults = cc3200_wab_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(cc3200_wab_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
};

/*----------------------------------------------------------------------------------*/

static int cc3200_wab_dai_startup(struct snd_pcm_substream *substream,struct snd_soc_dai *dai)
{
	return -EINVAL;
}

static int cc3200_wab_hw_params(struct snd_pcm_substream *substream,struct snd_pcm_hw_params *params,struct snd_soc_dai *dai)
{
	return 0;
}

static int cc3200_wab_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	return 0;
}

/*----------------------------------------------------------------------------------*/

static const struct snd_soc_dai_ops cc3200_wab_dai_ops = {
	.startup = cc3200_wab_dai_startup,
	.hw_params = cc3200_wab_hw_params,
	.set_fmt = cc3200_wab_set_fmt,
};

static struct snd_soc_dai_driver cc3200_wab_dai = {
	.name = "cc3200-wab.hifi",
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_44100,
		.formats = SNDRV_PCM_FMTBIT_S16_LE
	},
	.ops = &cc3200_wab_dai_ops,
};

static struct snd_soc_codec_driver cc3200_wab_soc_codec_dev;

/*----------------------------------------------------------------------------------*/

static int cc3200_wab_i2c_probe(struct i2c_client *i2c,const struct i2c_device_id *id)
{
	struct regmap *regmap;
	struct cc3200_wab_priv *cc3200;
	struct regmap_config config = cc3200_wab_regmap;

	/* msb needs to be set to enable auto-increment of addresses */
	config.read_flag_mask = 0x80;
	config.write_flag_mask = 0x80;

	regmap = devm_regmap_init_i2c(i2c, &config);
	if(IS_ERR(regmap))
	{
		return PTR_ERR(regmap);
	}
	
	cc3200 = devm_kzalloc(&i2c->dev,sizeof(struct cc3200_wab_priv),GFP_KERNEL);
	if(cc3200==NULL)
	{
		return -ENOMEM;
	}
	
	dev_set_drvdata(&i2c->dev,cc3200);
	cc3200->regmap = regmap;

	return snd_soc_register_codec(&i2c->dev, &cc3200_wab_soc_codec_dev, &cc3200_wab_dai, 1);

}

/*----------------------------------------------------------------------------------*/

static int cc3200_wab_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);
	return 0;
}

/*----------------------------------------------------------------------------------*/

static struct i2c_driver cc3200_wab_i2c_driver = {
	.driver		= {
		.name	= "cc3200-wab.codec",
		.owner	= THIS_MODULE,
		.of_match_table = cc3200_wab_of_match,
	},
	.probe 		= cc3200_wab_i2c_probe,
	.remove 	= cc3200_wab_i2c_remove,
	.id_table	= cc3200_wab_i2c_id,
};

/*----------------------------------------------------------------------------------*/

module_i2c_driver(cc3200_wab_i2c_driver);

MODULE_DESCRIPTION("CC3200 WAB codec driver - I2C");
MODULE_AUTHOR("Stuart MacLean <stuart@hifiberry.com>");
MODULE_LICENSE("GPL v2");
