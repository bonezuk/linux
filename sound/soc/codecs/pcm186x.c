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

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>

#include <linux/pm.h>
#include <linux/regmap.h>

#include "pcm186x.h"

/*----------------------------------------------------------------------------------*/

struct pcm186x_priv
{
	struct regmap *regmap;
	int mclk_rate;
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

static int pcm186x_dai_startup(struct snd_pcm_substream *substream,struct snd_soc_dai *dai)
{
	int res,bitsPerSample;
	struct snd_soc_codec *codec = dai->codec;
	struct snd_pcm_runtime *runtime = substream->runtime;
	
	printk(KERN_INFO "pcm186x_dai_startup\n");
	
	pcm186x_power_device(codec,1);
	
	bitsPerSample = snd_pcm_format_physical_width(runtime->format);
	res = pcm186x_setup_clocks(codec,params_rate(runtime->rate),bitsPerSample,pcm186x->mclk_rate);
	if(res)
		printk(KERN_ERR "pcm186x: Error setting up ADC clocks\n");
	
	return res;
}

/*----------------------------------------------------------------------------------*/

static void pcm186x_dai_shutdown(struct snd_pcm_substream *substream,struct snd_soc_dai *dai)
{
	printk(KERN_INFO "pcm186x_dai_shutdown\n");
	pcm186x_power_device(codec,0);
}

/*----------------------------------------------------------------------------------*/

static int pcm186x_hw_params(struct snd_pcm_substream *substream,struct snd_pcm_hw_params *params,struct snd_soc_dai *dai)
{
	int res,bitsPerSample;
	struct snd_soc_codec *codec = dai->codec;
	struct pcm186x_priv *pcm186x = snd_soc_codec_get_drvdata(codec);
	
	printk(KERN_INFO "pcm186x_hw_params\n");
	
	bitsPerSample = snd_pcm_format_physical_width(params_format(params));
	res = pcm186x_setup_clocks(codec,params_rate(params),bitsPerSample,pcm186x->mclk_rate);
	if(!res)
		printk(KERN_ERR "pcm186x: Error setting up ADC clocks\n");
		
	return 
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

static int pcm186x_set_sysclk(struct snd_soc_dai *dai,int clk_id, unsigned int freq, int dir)
{
	int res;
	struct snd_soc_codec *codec = dai->codec;
	struct pcm186x_priv *pcm186x = snd_soc_codec_get_drvdata(codec);
	
	printk(KERN_INFO "pcm186x_set_sysclk\n");
	
	if(clk_id == PCM186X_MASTER_CLK)
	{
		pcm186x->mclk_rate = freq;
		res = 0;
	}
	else
	{
		printk(KERN_ERR "pcm186x: Unknown clock id\n");
		res = 1;
	}
	return res;
}

/*----------------------------------------------------------------------------------*/

static const struct snd_soc_dai_ops pcm186x_dai_ops = {
	.startup = pcm186x_dai_startup,
	.shutdown = pcm186x_dai_shutdown,
	.hw_params = pcm186x_hw_params,
	.set_fmt = pcm186x_set_fmt,
	.set_sysclk = pcm186x_set_sysclk,
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
		.channels_min = 2,
		.channels_max = 2,
		.rates = PCM186X_RATES,
		.formats = PCM186X_FORMATS,
	},
	.ops = &pcm186x_dai_ops,
	.symmetric_rates = 1
};

static const struct snd_soc_codec_driver soc_codec_dev_pcm186x = {
	.idle_bias_off = false,
};

/*----------------------------------------------------------------------------------*/

static int pcm186x_i2c_probe(struct i2c_client *i2c,const struct i2c_device_id *id)
{
	int ret;
	struct regmap *regmap;
	struct pcm186x_priv *pcm186x;
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
	
	pcm186x = devm_kzalloc(&i2c->dev,sizeof(struct pcm186x_priv),GFP_KERNEL);
	if(pcm186x==NULL)
	{
		return -ENOMEM;
	}
	
	dev_set_drvdata(&i2c->dev,pcm186x);
	pcm186x->regmap = regmap;
	pcm186x->powered = 0;

	ret = snd_soc_register_codec(&i2c->dev,&soc_codec_dev_pcm186x,&pcm186x_dai,1);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to register CODEC: %d\n", ret);
	}
	return ret;
}

/*----------------------------------------------------------------------------------*/

static int pcm186x_i2c_remove(struct i2c_client *i2c)
{
	printk(KERN_INFO "pcm186x_i2c_remove\n");
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
