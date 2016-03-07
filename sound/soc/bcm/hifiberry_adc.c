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

#include "../codecs/pcm186x.h"

/*----------------------------------------------------------------------------------*/

static int hifiberry_ad_adc_init(struct snd_soc_pcm_runtime *rtd)
{
	printk(KERN_INFO "hifiberry_ad_adc_init\n");
	return 0;
}

/*----------------------------------------------------------------------------------*/

static int hifiberry_ad_adc_hw_params(struct snd_pcm_substream *substream,struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	
	printk(KERN_INFO "hifiberry_ad_adc_hw_params\n");
	
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

static struct snd_soc_dai_link hifiberry_ad_dai[] = {
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

	printk(KERN_INFO "hifiberry_ad_probe\n");

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
	printk(KERN_INFO "hifiberry_ad_remove\n");
	return snd_soc_unregister_card(&hifiberry_ad);
}

/*----------------------------------------------------------------------------------*/

static const struct of_device_id hifiberry_ad_of_match[] = {
	{.compatible = "hifiberry,hifiberry-adc",},
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
