/*
 *  odroid_wm8960.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#define DEBUG

#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>

#include "../codecs/wm8960.h"
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>

#include <mach/regs-pmu.h>

#include "i2s.h"
#include "i2s-regs.h"

/* ODROID use CLKOUT from AP */
#define ODROID_MCLK_FREQ		24000000
#define ODROID_AUD_PLL_FREQ	196608009

static struct snd_soc_card odroid;
static bool is_dummy_codec = false;

static int set_aud_pll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	printk("@@@ [%s][%s:%d]\n", __FILE__, __func__, __LINE__);

	fout_epll = __clk_lookup("fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return PTR_ERR(fout_epll);
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_set_rate(fout_epll, rate);
	printk("%s[%d] : aud_pll set_rate=%ld, get_rate = %ld\n",
		__func__,__LINE__,rate,clk_get_rate(fout_epll));
out:
	clk_put(fout_epll);

	return 0;
}

/*
 * ODROID WM8960 I2S DAI operations. (AP master)
 */
static int odroid_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int pll, div, sclk, bfs, psr, rfs, ret;
	unsigned long rclk;

        printk("@@@ [%s][%s:%d]\n", __FILE__, __func__, __LINE__);

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

	printk("        params_format(params)=%u\n", params_format(params));
	if(params_format(params) == SNDRV_PCM_FORMAT_U24)
	printk("        format=SNDRV_PCM_FORMAT_U24\n");
        if(params_format(params) == SNDRV_PCM_FORMAT_S24)
        printk("        format=SNDRV_PCM_FORMAT_U24\n");
        if(params_format(params) == SNDRV_PCM_FORMAT_U16_LE)
        printk("        format=SNDRV_PCM_FORMAT_U16_LE\n");
        if(params_format(params) == SNDRV_PCM_FORMAT_S16_LE)
        printk("        format=SNDRV_PCM_FORMAT_S16_LE\n");

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

	printk("        bfs=%d, rfs=%d, psr=%d, rclk=%lu\n", bfs, rfs, psr, rclk);

	/* Set AUD_PLL frequency */
	sclk = rclk * psr;
	printk("        sclk=%u\n", sclk);
	for (div = 2; div <= 16; div++) {
		if (sclk * div > ODROID_AUD_PLL_FREQ)
			break;
	}
	pll = sclk * (div - 1);
	printk("        pll=%u\n", pll);

	set_aud_pll_rate(pll);

	/* Set CPU DAI configuration */
	if(!is_dummy_codec) {
    		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
    			| SND_SOC_DAIFMT_NB_NF
    			| SND_SOC_DAIFMT_CBS_CFS);
    		if (ret < 0){
			printk("	[%s]ERROR(%d)\n", __func__, __LINE__);
    			return ret;
		}
    	}

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
			| SND_SOC_DAIFMT_NB_NF
			| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0){
                printk("        ERROR:[%s][%s]:%d\n", __FILE__,  __func__, __LINE__);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0){
                printk("        ERROR:[%s][%s]:%d\n", __FILE__,  __func__, __LINE__);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1,
					rclk, SND_SOC_CLOCK_OUT);
	if (ret < 0){
		printk("        ERROR:[%s][%s]:%d\n", __FILE__,  __func__, __LINE__);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					rfs, SND_SOC_CLOCK_OUT);
	if (ret < 0){
		printk("        ERROR:[%s][%s]:%d\n", __FILE__,  __func__, __LINE__);
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0){
		printk("        ERROR:[%s][%s]:%d\n", __FILE__,  __func__, __LINE__);
		return ret;
	}

	if(!is_dummy_codec) {
    		ret = snd_soc_dai_set_sysclk(codec_dai, 0, rclk, SND_SOC_CLOCK_IN);
    		if (ret < 0){
			printk("        ERROR:[%s][%s]:%d\n", __FILE__,  __func__, __LINE__);
    			return ret;
		}
    	}

	printk("        %s will return 0\n", __func__);
	return 0;
}



static struct snd_soc_ops odroid_ops = {
	.hw_params = odroid_hw_params,
//	.hw_params = wm8960_be_ops_hw_params,
};

static int smdk_wm8960_init_paiftx(struct snd_soc_pcm_runtime *rtd)
{
        struct snd_soc_codec *codec = rtd->codec;
        struct snd_soc_dapm_context *dapm = &codec->dapm;

        /* HeadPhone */
        snd_soc_dapm_enable_pin(dapm, "HP_R");
        snd_soc_dapm_enable_pin(dapm, "HP_L");

        /* MicIn */
        snd_soc_dapm_enable_pin(dapm, "LINPUT1");
        snd_soc_dapm_enable_pin(dapm, "RINPUT1");

        /* LineIn */
        snd_soc_dapm_enable_pin(dapm, "LINPUT2");
        snd_soc_dapm_enable_pin(dapm, "RINPUT2");

        /* Other pins NC */
        snd_soc_dapm_nc_pin(dapm, "LINPUT3");
        snd_soc_dapm_nc_pin(dapm, "RINPUT3");
        snd_soc_dapm_nc_pin(dapm, "SPK_RN");
        snd_soc_dapm_nc_pin(dapm, "SPK_LN");
        snd_soc_dapm_nc_pin(dapm, "SPK_RP");
        snd_soc_dapm_nc_pin(dapm, "SPK_LP");
        snd_soc_dapm_nc_pin(dapm, "OUT3");

        return 0;
}


static int smdk_set_bias_level_post(struct snd_soc_card *card,
                                struct snd_soc_dapm_context *dapm,
                                enum snd_soc_bias_level level)
{
        struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;
        printk("@@@ [%s][%s:%d]\n", __FILE__, __func__, __LINE__);

        if (dapm->dev != aif1_dai->dev){
		printk("	dapm->dev != aif1_dai->dev\n");
                return 0;
	}

        if ((level == SND_SOC_BIAS_OFF) && !aif1_dai->active) {
                printk("%s: SND_SOC_BIAS_OFF\n", __func__);
//                smdk_codec_fll_enable(aif1_dai, 0);
        }

        return 0;
}

static struct snd_soc_dai_link odroid_dai[] = {
	{
		.name = "WM8960 AIF1",
		.stream_name = "i2s0-pri",
                .codec_name = "wm8960",
                .codec_dai_name = "wm8960-hifi",
		.init = smdk_wm8960_init_paiftx,
		.ops = &odroid_ops,
	},
	{
                .name = "WM8960 AIF2",
                .stream_name = "i2s0-sec",
                .cpu_dai_name = "samsung-i2s-sec",
                .platform_name = "samsung-i2s-sec",
                .codec_name = "wm8960",
                .codec_dai_name = "wm8960-hifi",
                .ops = &odroid_ops,
	},
};

static struct snd_soc_card odroid = {
	.name = "odroid-audio",
	.owner = THIS_MODULE,
	.dai_link = odroid_dai,
	.num_links = ARRAY_SIZE(odroid_dai),
//	.set_bias_level_post = smdk_set_bias_level_post,
//	.dapm_widgets = wm8960_dapm_widgets
};

static int odroid_audio_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &odroid;
	card->dev = &pdev->dev;

        printk("@@@ [%s][%s:%d]\n", __FILE__, __func__, __LINE__);

	for (n = 0; np && n < ARRAY_SIZE(odroid_dai); n++) {
		if (!odroid_dai[n].cpu_dai_name) {
			odroid_dai[n].cpu_of_node = of_parse_phandle(np,
					"samsung,audio-cpu", n);

			if (!odroid_dai[n].cpu_of_node) {
				dev_err(&pdev->dev, "Property "
				"'samsung,audio-cpu' missing or invalid\n");
				ret = -EINVAL;
			}
		}

		if (!odroid_dai[n].platform_name)
			odroid_dai[n].platform_of_node = odroid_dai[n].cpu_of_node;

/*******
FROM smdk5422_wm8994.c
*******/

		if (!odroid_dai[n].codec_name) {
			odroid_dai[n].codec_of_node = of_parse_phandle(np,
				"samsung,audio-codec", n);
			if (!odroid_dai[0].codec_of_node) {
				dev_err(&pdev->dev, "Property "
				"'samsung,audio-codec' missing or invalid\n");
				ret = -EINVAL;
			}
		}


/*
		odroid_dai[n].codec_name = NULL;
		odroid_dai[n].codec_of_node = of_parse_phandle(np,
				"samsung,audio-codec", n);
		if (!odroid_dai[0].codec_of_node) {
			dev_err(&pdev->dev,
			"Property 'samsung,audio-codec' missing or invalid\n");
			ret = -EINVAL;
		}
*/
/**********
/FROM smdk5422_wm8994.c
**********/
	}

	ret = snd_soc_register_card(card);
	if (ret) { // WM8960 register failed.
	        dev_err(&pdev->dev, "snd_soc_register_card() failed(wm8960): %d\n", ret);

	        odroid_dai[0].name="DUMMY-SEC";
	        odroid_dai[0].stream_name = "i2s0-sec";
	        odroid_dai[0].codec_dai_name="dummy-aif1";

	        odroid_dai[1].name="DUMMY-PRI";
	        odroid_dai[1].stream_name = "i2s0-pri";
	        odroid_dai[1].codec_dai_name="dummy-aif2";

    		for (n = 0; np && n < ARRAY_SIZE(odroid_dai); n++) {
	    		odroid_dai[n].codec_name = NULL;
    			odroid_dai[n].codec_of_node = of_parse_phandle(np,
    				"samsung,audio-dummy", n);
    			if (!odroid_dai[n].codec_of_node) {
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
}

static int odroid_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

        printk("@@@ [%s][%s:%d]\n", __FILE__, __func__, __LINE__);

	snd_soc_unregister_card(card);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id odroid_wm8960_of_match[] = {
	{ .compatible = "hardkernel,odroid-wm8960", },
	{},
};
MODULE_DEVICE_TABLE(of, samsung_wm8960_of_match);
#endif /* CONFIG_OF */

static struct platform_driver odroid_audio_driver = {
	.driver		= {
		.name	= "odroid-audio",
		.owner	= THIS_MODULE,
		.pm = &snd_soc_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odroid_wm8960_of_match),
#endif
	},
	.probe		= odroid_audio_probe,
	.remove		= odroid_audio_remove,
};

module_platform_driver(odroid_audio_driver);

MODULE_DESCRIPTION("ALSA SoC ODROID WM8960");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:odroid-audio");
