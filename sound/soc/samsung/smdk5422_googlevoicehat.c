/*
 * ASoC Driver for Google voiceHAT SoundCard
 *
 * Author: Peter Malkin <petermalkin@google.com>
 *         Copyright 2016
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

#include <linux/of.h>
#include <linux/clk.h>
#include <linux/io.h>  
#include <linux/clk.h> 
#include <linux/clkdev.h> 
#include <linux/clk-provider.h> 
#include <sound/initval.h> 
#include <mach/regs-pmu.h>

#include "i2s.h"    
#include "i2s-regs.h"    

#define ODROID_MCLK_FREQ                24000000
#define ODROID_AUD_PLL_FREQ     196608009 

static bool is_dummy_codec = false;

static int snd_rpi_googlevoicehat_soundcard_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

/*
static int snd_rpi_googlevoicehat_soundcard_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	unsigned int sample_bits =
		snd_pcm_format_physical_width(params_format(params));

	return snd_soc_dai_set_bclk_ratio(cpu_dai, sample_bits * 2);
}
*/

static int set_aud_pll_rate(unsigned long rate)
{
        printk("[odroid_max98090][%s]", __func__);
        struct clk *fout_epll;
        fout_epll = __clk_lookup("fout_epll");
        if (IS_ERR(fout_epll)) {
                printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
               return PTR_ERR(fout_epll);
	}
	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_set_rate(fout_epll, rate);
	printk("%s[%d] : aud_pll set_rate=%ld, get_rate = %ld\n", __func__,__LINE__,rate,clk_get_rate(fout_epll));

out:
	clk_put(fout_epll);

	return 0;
}

/*
 * ODROID MAX98090 I2S DAI operations. (AP master)
 */
static int odroid_hw_params(struct snd_pcm_substream *substream,
        struct snd_pcm_hw_params *params)
{
        printk("[odroid_max98090][%s]", __func__);
        struct snd_soc_pcm_runtime *rtd = substream->private_data;
        struct snd_soc_dai *codec_dai = rtd->codec_dai;
        struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
        int pll, div, sclk, bfs, psr, rfs, ret;
        unsigned long rclk;

        switch (params_format(params)) {
        case SNDRV_PCM_FORMAT_U24:
        case SNDRV_PCM_FORMAT_S24:
                bfs = 48;
                break;
        case SNDRV_PCM_FORMAT_U16_LE:
        case SNDRV_PCM_FORMAT_S16_LE:
                bfs = 32;
                break;
        default:
                return -EINVAL;
        }

	switch (params_rate(params)) {
        case 16000:
        case 22050:
        case 24000:
        case 32000:
        case 44100:
        case 48000:
        case 88200:
        case 96000:
                if (bfs == 48)
                        rfs = 384;
                else
                        rfs = 256;
                break;
        case 64000:
                rfs = 384;
                break;
        case 8000:
        case 11025:
        case 12000:
                if (bfs == 48)
                        rfs = 768;
                else
                        rfs = 512;
                break;
        default:
                return -EINVAL;
        }

        rclk = params_rate(params) * rfs;

        switch (rclk) {
        case 4096000:
        case 5644800:
        case 6144000:
        case 8467200:
        case 9216000:
                psr = 8;
                break;
        case 8192000:
        case 11289600:
        case 12288000:
        case 16934400:
        case 18432000:
                psr = 4;
                break;
        case 22579200:
        case 24576000:
        case 33868800:
        case 36864000:
                psr = 2;
                break;
        case 67737600:
        case 73728000:
                psr = 1;
                break;
        default:
                printk("Not yet supported!\n");
                return -EINVAL;
        }

        /* Set AUD_PLL frequency */
        sclk = rclk * psr;
        for (div = 2; div <= 16; div++) {
                if (sclk * div > ODROID_AUD_PLL_FREQ)
                        break;
        }
        pll = sclk * (div - 1);

        set_aud_pll_rate(pll);

        /* Set CPU DAI configuration */
        if(!is_dummy_codec) {
        ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
                        | SND_SOC_DAIFMT_NB_NF
                        | SND_SOC_DAIFMT_CBS_CFS);
        if (ret < 0)
                return ret;
    	}

        ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
                        | SND_SOC_DAIFMT_NB_NF
                        | SND_SOC_DAIFMT_CBS_CFS);
        if (ret < 0)
                return ret;

        ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
                                        0, MOD_OPCLK_PCLK);
        if (ret < 0)
                return ret;

        ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1,
                                        rclk, SND_SOC_CLOCK_OUT);
        if (ret < 0)
                return ret;

        ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
                                        rfs, SND_SOC_CLOCK_OUT);
        if (ret < 0)
                return ret;

        ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
        if (ret < 0)
                return ret;

        if(!is_dummy_codec) {
	        ret = snd_soc_dai_set_sysclk(codec_dai, 0, rclk, SND_SOC_CLOCK_IN);
        	if (ret < 0)
	                return ret;
    	}

        return 0;
}



/* machine stream operations */
static struct snd_soc_ops snd_rpi_googlevoicehat_soundcard_ops = {
//	.hw_params = snd_rpi_googlevoicehat_soundcard_hw_params,
	.hw_params = odroid_hw_params,
};

static struct snd_soc_dai_link snd_rpi_googlevoicehat_soundcard_dai[] = {
	{
		.name		= "Google voiceHAT SoundCard",
		.stream_name	= "i2s0-sec",
		.cpu_dai_name	= "samsung-i2s-sec",
		.codec_dai_name	= "voicehat-hifi",
		.platform_name	= "samsung-i2s-sec",
		.codec_name	= "voicehat-codec",
		.dai_fmt	= SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS,
		.ops		= &snd_rpi_googlevoicehat_soundcard_ops,
		.init		= snd_rpi_googlevoicehat_soundcard_init,
	},
	{
		.name           = "Google voiceHAT SoundCard",
		.stream_name    = "i2s0-pri",
		.codec_dai_name = "voicehat-hifi",
		.ops            = &snd_rpi_googlevoicehat_soundcard_ops,
		.init           = snd_rpi_googlevoicehat_soundcard_init,
	},
};

/* audio machine driver */
static struct snd_soc_card snd_rpi_googlevoicehat_soundcard = {
	.name         = "snd_rpi_googlevoicehat_soundcard",
	.owner        = THIS_MODULE,
	.dai_link     = snd_rpi_googlevoicehat_soundcard_dai,
	.num_links    = ARRAY_SIZE(snd_rpi_googlevoicehat_soundcard_dai),
};

static int snd_rpi_googlevoicehat_soundcard_probe(struct platform_device *pdev)
{
	int ret = 0;

	/**
	 *TEST
	 */
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &snd_rpi_googlevoicehat_soundcard;
	card->dev = &pdev->dev;
	int n;

	for (n = 0; np && n < ARRAY_SIZE(snd_rpi_googlevoicehat_soundcard_dai); n++) {
	if (!snd_rpi_googlevoicehat_soundcard_dai[n].cpu_dai_name) {
		snd_rpi_googlevoicehat_soundcard_dai[n].cpu_of_node = of_parse_phandle(np,
				"samsung,audio-cpu", n);

		if (!snd_rpi_googlevoicehat_soundcard_dai[n].cpu_of_node) {
			dev_err(&pdev->dev, "Property "
					"'samsung,audio-cpu' missing or invalid\n");
			ret = -EINVAL;
		}

		if (!snd_rpi_googlevoicehat_soundcard_dai[n].platform_name)
			snd_rpi_googlevoicehat_soundcard_dai[n].platform_of_node = snd_rpi_googlevoicehat_soundcard_dai[n].cpu_of_node;
		
		snd_rpi_googlevoicehat_soundcard_dai[n].codec_of_node = of_parse_phandle(np,
				"samsung,audio-codec", n);
		snd_rpi_googlevoicehat_soundcard_dai[n].codec_name = NULL;

		if (!snd_rpi_googlevoicehat_soundcard_dai[0].codec_of_node) {
			dev_err(&pdev->dev, "Property 'samsung,audio-codec' missing or invalid\n");
			ret = -EINVAL;
		}
	}
	}


	ret = snd_soc_register_card(card);
	if (ret) { // MAX98090 register failed.
		dev_err(&pdev->dev, "snd_soc_register_card() failed(max98090): %d\n", ret);

		snd_rpi_googlevoicehat_soundcard_dai[0].name="DUMMY-SEC";
		snd_rpi_googlevoicehat_soundcard_dai[0].stream_name = "i2s0-sec";
		snd_rpi_googlevoicehat_soundcard_dai[0].codec_dai_name="dummy-aif1";

		snd_rpi_googlevoicehat_soundcard_dai[1].name="DUMMY-PRI";                    
		snd_rpi_googlevoicehat_soundcard_dai[1].stream_name = "i2s0-pri";
		snd_rpi_googlevoicehat_soundcard_dai[1].codec_dai_name="dummy-aif2";

		for (n = 0; np && n < ARRAY_SIZE(snd_rpi_googlevoicehat_soundcard_dai); n++) {
			snd_rpi_googlevoicehat_soundcard_dai[n].codec_name = NULL;
			snd_rpi_googlevoicehat_soundcard_dai[n].codec_of_node = of_parse_phandle(np,
					"samsung,audio-dummy", n);
			if (!snd_rpi_googlevoicehat_soundcard_dai[n].codec_of_node) {
				dev_err(&pdev->dev,
						"Property 'samsung,audio-dummy' missing or invalid\n");
				ret = -EINVAL;
			}
		}
		ret = snd_soc_register_card(card);
		if (ret) {				
			dev_err(&pdev->dev, "snd_soc_register_card() failed(dummy_codec): %d\n", ret);
			is_dummy_codec=false;
		}
		else is_dummy_codec=true;

	}

	return ret;


	/*********************************/





	snd_rpi_googlevoicehat_soundcard.dev = &pdev->dev;

	if (pdev->dev.of_node) {
		struct device_node *i2s_node;
		struct snd_soc_dai_link *dai = &snd_rpi_googlevoicehat_soundcard_dai[0];
		i2s_node = of_parse_phandle(pdev->dev.of_node,
					"i2s-controller", 0);

		if (i2s_node) {
			dai->cpu_dai_name = NULL;
			dai->cpu_of_node = i2s_node;
			dai->platform_name = NULL;
			dai->platform_of_node = i2s_node;
		}
	}

	ret = snd_soc_register_card(&snd_rpi_googlevoicehat_soundcard);
	if (ret)
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);

	return ret;
}

static int snd_rpi_googlevoicehat_soundcard_remove(struct platform_device *pdev)
{
	return snd_soc_unregister_card(&snd_rpi_googlevoicehat_soundcard);
}

static const struct of_device_id snd_rpi_googlevoicehat_soundcard_of_match[] = {
	{ .compatible = "googlevoicehat,googlevoicehat-soundcard", },
	{},
};
MODULE_DEVICE_TABLE(of, snd_rpi_googlevoicehat_soundcard_of_match);

static struct platform_driver snd_rpi_googlevoicehat_soundcard_driver = {
	.driver = {
		.name   = "snd-googlevoicehat-soundcard",
		.owner  = THIS_MODULE,
		.of_match_table = snd_rpi_googlevoicehat_soundcard_of_match,
	},
	.probe          = snd_rpi_googlevoicehat_soundcard_probe,
	.remove         = snd_rpi_googlevoicehat_soundcard_remove,
};

module_platform_driver(snd_rpi_googlevoicehat_soundcard_driver);

MODULE_AUTHOR("Peter Malkin <petermalkin@google.com>");
MODULE_DESCRIPTION("ASoC Driver for Google voiceHAT SoundCard");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:snd-googlevoicehat-soundcard");
