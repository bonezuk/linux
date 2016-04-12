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

#define HIFIBERRY_ADC_MASTERCLOCK 24576000

/*----------------------------------------------------------------------------------*/

static int snd_rpi_hifiberry_adc_init(struct snd_soc_pcm_runtime *rtd)
{
	printk(KERN_INFO "snd_rpi_hifiberry_adc_init\n");
	return 0;
}

/*----------------------------------------------------------------------------------*/

static int snd_rpi_hifiberry_adc_hw_params(struct snd_pcm_substream *substream,struct snd_pcm_hw_params *params)
{
	int ret;
	unsigned int sample_bits;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	printk(KERN_INFO "snd_rpi_hifiberry_adc_hw_params\n");

	ret = snd_soc_dai_set_sysclk(codec_dai,PCM186X_MASTER_CLK,HIFIBERRY_ADC_MASTERCLOCK,SND_SOC_CLOCK_OUT);

	sample_bits = snd_pcm_format_physical_width(params_format(params));

	return snd_soc_dai_set_bclk_ratio(cpu_dai,sample_bits * 2);
}

/*----------------------------------------------------------------------------------*/

static int snd_rpi_hifiberry_adc_startup(struct snd_pcm_substream *substream)
{
	printk(KERN_INFO "snd_rpi_hifiberry_adc_startup\n");
	return 0;
}

/*----------------------------------------------------------------------------------*/

static void snd_rpi_hifiberry_adc_shutdown(struct snd_pcm_substream *substream)
{
	printk(KERN_INFO "snd_rpi_hifiberry_adc_shutdown\n");
}

/*----------------------------------------------------------------------------------*/
/* machine stream operations */
/*----------------------------------------------------------------------------------*/

static struct snd_soc_ops snd_rpi_hifiberry_adc_ops = 
{
	.hw_params = snd_rpi_hifiberry_adc_hw_params,
	.startup = snd_rpi_hifiberry_adc_startup,
	.shutdown = snd_rpi_hifiberry_adc_shutdown,
};

/*----------------------------------------------------------------------------------*/

static struct snd_soc_dai_link snd_rpi_hifiberry_adc_dai[] = {
{
	.name		= "HiFiBerry ADC",
	.stream_name	= "HiFiBerry ADC HiFi",
	.cpu_dai_name	= "bcm2708-i2s.0",
	.codec_dai_name	= "pcm186x-hifi",
	.platform_name	= "bcm2708-i2s.0",
	.codec_name	= "pcm186x.1-004a",
	.dai_fmt	= SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM,
	.ops		= &snd_rpi_hifiberry_adc_ops,
	.init		= snd_rpi_hifiberry_adc_init,
},
};

/*----------------------------------------------------------------------------------*/

static struct snd_soc_card snd_rpi_hifiberry_adc = {
	.name         = "snd_rpi_hifiberry_adc",
	.owner        = THIS_MODULE,
	.dai_link     = snd_rpi_hifiberry_adc_dai,
	.num_links    = ARRAY_SIZE(snd_rpi_hifiberry_adc_dai),
};

/*----------------------------------------------------------------------------------*/

static int snd_rpi_hifiberry_adc_probe(struct platform_device *pdev)
{
	int ret = 0;

	printk(KERN_INFO "snd_rpi_hifiberry_adc_probe\n");

	snd_rpi_hifiberry_adc.dev = &pdev->dev;
	
	if(pdev->dev.of_node) 
	{
		struct device_node *i2s_node;
		struct snd_soc_dai_link *dai = &snd_rpi_hifiberry_adc_dai[0];
		i2s_node = of_parse_phandle(pdev->dev.of_node,"i2s-controller",0);

		if(i2s_node) 
		{
			dai->cpu_dai_name = NULL;
			dai->cpu_of_node = i2s_node;
			dai->platform_name = NULL;
			dai->platform_of_node = i2s_node;
		}
	}
	
	ret = snd_soc_register_card(&snd_rpi_hifiberry_adc);
	if(ret)
	{
		dev_err(&pdev->dev,"snd_soc_register_card() failed: %d\n",ret);
	}
	return ret;
}

/*----------------------------------------------------------------------------------*/

static int snd_rpi_hifiberry_adc_remove(struct platform_device *pdev)
{
	printk(KERN_INFO "snd_rpi_hifiberry_adc_remove\n");
	return snd_soc_unregister_card(&snd_rpi_hifiberry_adc);
}

/*----------------------------------------------------------------------------------*/

static const struct of_device_id snd_rpi_hifiberry_adc_of_match[] = {
	{.compatible = "hifiberry,hifiberry-adc",},
	{},
};
MODULE_DEVICE_TABLE(of,snd_rpi_hifiberry_adc_of_match);

/*----------------------------------------------------------------------------------*/

static struct platform_driver snd_rpi_hifiberry_adc_driver = {
	.driver = {
		.name   = "snd-rpi-hifiberry-adc",
		.owner  = THIS_MODULE,
		.of_match_table = snd_rpi_hifiberry_adc_of_match,
	},
	.probe          = snd_rpi_hifiberry_adc_probe,
	.remove         = snd_rpi_hifiberry_adc_remove,
};

/*----------------------------------------------------------------------------------*/

module_platform_driver(snd_rpi_hifiberry_adc_driver);

MODULE_DESCRIPTION("HiFiBerry ADC Driver");
MODULE_AUTHOR("Stuart MacLean <stuart@hifiberry.com>");
MODULE_LICENSE("GPL v2");

/*----------------------------------------------------------------------------------*/
