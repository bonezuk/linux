/*
 * Driver for the HiFiBerry CC3200 Audio Bridge DAC
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
#include <linux/platform_device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>

/*----------------------------------------------------------------------------------*/

static int hifiberry_cc3200_dac_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

/*----------------------------------------------------------------------------------*/

static int hifiberry_cc3200_dac_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	return snd_soc_dai_set_bclk_ratio(cpu_dai, 32*2);
}

/*----------------------------------------------------------------------------------*/
/* machine stream operations */
/*----------------------------------------------------------------------------------*/

static struct snd_soc_ops hifiberry_cc3200_dac_ops = 
{
	.hw_params = hifiberry_cc3200_dac_hw_params,
};

/*----------------------------------------------------------------------------------*/

static struct snd_soc_dai_link hifiberry_cc3200_dac_dai[] = {
{
	.name		= "HiFiBerry CC3200",
	.stream_name	= "HiFiBerry CC3200 HiFi",
	.cpu_dai_name	= "bcm2708-i2s.0",
	.codec_dai_name	= "cc3200-wab.hifi",
	.platform_name	= "bcm2708-i2s.0",
	.codec_name	= "cc3200-wab.codec",
	.dai_fmt	= SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
	.ops		= &hifiberry_cc3200_dac_ops,
	.init		= hifiberry_cc3200_dac_init,
},
};

/*----------------------------------------------------------------------------------*/

/* audio machine driver */
static struct snd_soc_card hifiberry_cc3200_dac = {
	.name         = "hifiberry_cc3200_dac",
	.dai_link     = hifiberry_cc3200_dac_dai,
	.num_links    = ARRAY_SIZE(hifiberry_cc3200_dac_dai),
};

/*----------------------------------------------------------------------------------*/

static int hifiberry_cc3200_dac_probe(struct platform_device *pdev)
{
	int ret = 0;

	hifiberry_cc3200_dac.dev = &pdev->dev;
	
	if(pdev->dev.of_node) 
	{
		struct device_node *i2s_node;
		struct snd_soc_dai_link *dai = &hifiberry_cc3200_dac_dai[0];
		i2s_node = of_parse_phandle(pdev->dev.of_node, "i2s-controller", 0);

		if (i2s_node) 
		{
			dai->cpu_dai_name = NULL;
			dai->cpu_of_node = i2s_node;
			dai->platform_name = NULL;
			dai->platform_of_node = i2s_node;
		}
	}
	
	ret = snd_soc_register_card(&hifiberry_cc3200_dac);
	if(ret)
	{
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);
	}
	return ret;
}

/*----------------------------------------------------------------------------------*/

static int hifiberry_cc3200_dac_remove(struct platform_device *pdev)
{
	return snd_soc_unregister_card(&hifiberry_cc3200_dac);
}

/*----------------------------------------------------------------------------------*/

static const struct of_device_id hifiberry_cc3200_dac_of_match[] = {
	{ .compatible = "hifiberry_cc3200,hifiberry_cc3200_dac", },
	{},
};
MODULE_DEVICE_TABLE(of, hifiberry_cc3200_dac_of_match);

/*----------------------------------------------------------------------------------*/

static struct platform_driver hifiberry_cc3200_dac_driver = {
        .driver = {
                .name   = "hifiberry-cc3200-dac",
                .owner  = THIS_MODULE,
                .of_match_table = hifiberry_cc3200_dac_of_match,
        },
        .probe          = hifiberry_cc3200_dac_probe,
        .remove         = hifiberry_cc3200_dac_remove,
};

/*----------------------------------------------------------------------------------*/

module_platform_driver(hifiberry_cc3200_dac_driver);

MODULE_DESCRIPTION("HiFiBerry CC3200 DAC Driver");
MODULE_AUTHOR("Stuart MacLean <stuart@hifiberry.com>");
MODULE_LICENSE("GPL v2");

/*----------------------------------------------------------------------------------*/
