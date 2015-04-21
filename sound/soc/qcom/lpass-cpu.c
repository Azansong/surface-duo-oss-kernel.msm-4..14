/*
 * Copyright (c) 2010-2011,2013-2015 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * lpass-cpu.c -- ALSA SoC CPU DAI driver for QTi LPASS
 */

#include <linux/clk.h>
#include <linux/compiler.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <linux/regmap.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include "lpass-lpaif-reg.h"
#include "lpass.h"

static int lpass_cpu_daiops_set_sysclk(struct snd_soc_dai *dai, int clk_id,
		unsigned int freq, int dir)
{
	struct lpass_data *drvdata = snd_soc_dai_get_drvdata(dai);
	int ret;

	if (!drvdata->mi2s_osr_clk)
		return 0;

	ret = clk_set_rate(drvdata->mi2s_osr_clk, freq);
	if (ret)
		dev_err(dai->dev, "%s() error setting mi2s osrclk to %u: %d\n",
				__func__, freq, ret);

	return ret;
}

static int lpass_cpu_daiops_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct lpass_data *drvdata = snd_soc_dai_get_drvdata(dai);
	int ret;

	if (drvdata->mi2s_osr_clk) {
		ret = clk_prepare_enable(drvdata->mi2s_osr_clk);
		if (ret) {
			dev_err(dai->dev, "%s() error in enabling mi2s osr clk: %d\n",
				__func__, ret);
			return ret;
		}
	}

	if (drvdata->mi2s_bit_clk) {
		ret = clk_prepare_enable(drvdata->mi2s_bit_clk);
		if (ret) {
			dev_err(dai->dev, "%s() error in enabling mi2s bit clk: %d\n",
				__func__, ret);
			if (drvdata->mi2s_osr_clk)
				clk_disable_unprepare(drvdata->mi2s_osr_clk);

			return ret;
		}
	}

	return 0;
}

static void lpass_cpu_daiops_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct lpass_data *drvdata = snd_soc_dai_get_drvdata(dai);

	clk_disable_unprepare(drvdata->mi2s_bit_clk);
	clk_disable_unprepare(drvdata->mi2s_osr_clk);
}

static int lpass_cpu_daiops_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct lpass_data *drvdata = snd_soc_dai_get_drvdata(dai);
	snd_pcm_format_t format = params_format(params);
	unsigned int channels = params_channels(params);
	unsigned int rate = params_rate(params);
	unsigned int regval;
	int bitwidth, ret;
	struct snd_soc_dai_driver *drv = dai->driver;

	bitwidth = snd_pcm_format_width(format);
	if (bitwidth < 0) {
		dev_err(dai->dev, "%s() invalid bit width given: %d\n",
				__func__, bitwidth);
		return bitwidth;
	}

	regval = LPAIF_I2SCTL_LOOPBACK_DISABLE |
			LPAIF_I2SCTL_WSSRC_INTERNAL;

	switch (bitwidth) {
	case 16:
		regval |= LPAIF_I2SCTL_BITWIDTH_16;
		break;
	case 24:
		regval |= LPAIF_I2SCTL_BITWIDTH_24;
		break;
	case 32:
		regval |= LPAIF_I2SCTL_BITWIDTH_32;
		break;
	default:
		dev_err(dai->dev, "%s() invalid bitwidth given: %d\n",
				__func__, bitwidth);
		return -EINVAL;
	}

	switch (channels) {
	case 1:
		regval |= LPAIF_I2SCTL_SPKMODE_SD0;
		regval |= LPAIF_I2SCTL_SPKMONO_MONO;
		break;
	case 2:
		regval |= LPAIF_I2SCTL_SPKMODE_SD0;
		regval |= LPAIF_I2SCTL_SPKMONO_STEREO;
		break;
	case 4:
		regval |= LPAIF_I2SCTL_SPKMODE_QUAD01;
		regval |= LPAIF_I2SCTL_SPKMONO_STEREO;
		break;
	case 6:
		regval |= LPAIF_I2SCTL_SPKMODE_6CH;
		regval |= LPAIF_I2SCTL_SPKMONO_STEREO;
		break;
	case 8:
		regval |= LPAIF_I2SCTL_SPKMODE_8CH;
		regval |= LPAIF_I2SCTL_SPKMONO_STEREO;
		break;
	default:
		dev_err(dai->dev, "%s() invalid channels given: %u\n",
				__func__, channels);
		return -EINVAL;
	}

	ret = regmap_write(drvdata->lpaif_map,
			LPAIF_I2SCTL_REG(drvdata->vdata, drv->id), regval);
	if (ret) {
		dev_err(dai->dev, "%s() error writing to i2sctl reg: %d\n",
				__func__, ret);
		return ret;
	}

	ret = clk_set_rate(drvdata->mi2s_bit_clk, rate * bitwidth * 2);
	if (ret) {
		dev_err(dai->dev, "%s() error setting mi2s bitclk to %u: %d\n",
				__func__, rate * bitwidth * 2, ret);
		return ret;
	}

	return 0;
}

static int lpass_cpu_daiops_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct lpass_data *drvdata = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_dai_driver *drv = dai->driver;
	int ret;

	ret = regmap_write(drvdata->lpaif_map,
			LPAIF_I2SCTL_REG(drvdata->vdata, drv->id), 0);
	if (ret)
		dev_err(dai->dev, "%s() error writing to i2sctl reg: %d\n",
				__func__, ret);

	return ret;
}

static int lpass_cpu_daiops_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct lpass_data *drvdata = snd_soc_dai_get_drvdata(dai);
	int ret;
	struct snd_soc_dai_driver *drv = dai->driver;

	ret = regmap_update_bits(drvdata->lpaif_map,
			LPAIF_I2SCTL_REG(drvdata->vdata, drv->id),
			LPAIF_I2SCTL_SPKEN_MASK, LPAIF_I2SCTL_SPKEN_ENABLE);
	if (ret)
		dev_err(dai->dev, "%s() error writing to i2sctl reg: %d\n",
				__func__, ret);

	return ret;
}

static int lpass_cpu_daiops_trigger(struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai)
{
	struct lpass_data *drvdata = snd_soc_dai_get_drvdata(dai);
	int ret = 0;
	struct snd_soc_dai_driver *drv = dai->driver;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		ret = regmap_update_bits(drvdata->lpaif_map,
				LPAIF_I2SCTL_REG(drvdata->vdata, drv->id),
				LPAIF_I2SCTL_SPKEN_MASK,
				LPAIF_I2SCTL_SPKEN_ENABLE);
		if (ret)
			dev_err(dai->dev, "%s() error writing to i2sctl reg: %d\n",
					__func__, ret);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		ret = regmap_update_bits(drvdata->lpaif_map,
				LPAIF_I2SCTL_REG(drvdata->vdata, drv->id),
				LPAIF_I2SCTL_SPKEN_MASK,
				LPAIF_I2SCTL_SPKEN_DISABLE);
		if (ret)
			dev_err(dai->dev, "%s() error writing to i2sctl reg: %d\n",
					__func__, ret);
		break;
	}

	return ret;
}

static struct snd_soc_dai_ops lpass_cpu_dai_ops = {
	.set_sysclk	= lpass_cpu_daiops_set_sysclk,
	.startup	= lpass_cpu_daiops_startup,
	.shutdown	= lpass_cpu_daiops_shutdown,
	.hw_params	= lpass_cpu_daiops_hw_params,
	.hw_free	= lpass_cpu_daiops_hw_free,
	.prepare	= lpass_cpu_daiops_prepare,
	.trigger	= lpass_cpu_daiops_trigger,
};

static int lpass_cpu_dai_probe(struct snd_soc_dai *dai)
{
	struct lpass_data *drvdata = snd_soc_dai_get_drvdata(dai);
	int ret;
	struct snd_soc_dai_driver *drv = dai->driver;

	/* ensure audio hardware is disabled */
	ret = regmap_write(drvdata->lpaif_map,
			LPAIF_I2SCTL_REG(drvdata->vdata, drv->id), 0);
	if (ret)
		dev_err(dai->dev, "%s() error writing to i2sctl reg: %d\n",
				__func__, ret);

	return ret;
}

static struct snd_soc_dai_driver lpass_cpu_dai_driver = {
	.playback = {
		.stream_name	= "lpass-cpu-playback",
		.formats	= SNDRV_PCM_FMTBIT_S16 |
					SNDRV_PCM_FMTBIT_S24 |
					SNDRV_PCM_FMTBIT_S32,
		.rates		= SNDRV_PCM_RATE_8000 |
					SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_32000 |
					SNDRV_PCM_RATE_48000 |
					SNDRV_PCM_RATE_96000,
		.rate_min	= 8000,
		.rate_max	= 96000,
		.channels_min	= 1,
		.channels_max	= 8,
	},
	.probe	= &lpass_cpu_dai_probe,
	.ops    = &lpass_cpu_dai_ops,
};


static struct snd_soc_dai_driver apq8016_lpass_cpu_dai_driver[] = {
	[APQ8016_LPAIF_MI2S_PRIMARY] =  {
		.id = APQ8016_LPAIF_MI2S_PRIMARY,
		.name = "Primary MI2S",
		.playback = {
			.stream_name	= "Primary Playback",
			.formats	= SNDRV_PCM_FMTBIT_S16 |
						SNDRV_PCM_FMTBIT_S24 |
						SNDRV_PCM_FMTBIT_S32,
			.rates		= SNDRV_PCM_RATE_8000 |
						SNDRV_PCM_RATE_16000 |
						SNDRV_PCM_RATE_32000 |
						SNDRV_PCM_RATE_48000 |
						SNDRV_PCM_RATE_96000,
			.rate_min	= 8000,
			.rate_max	= 96000,
			.channels_min	= 1,
			.channels_max	= 8,
		},
		.probe	= &lpass_cpu_dai_probe,
		.ops    = &lpass_cpu_dai_ops,
	},
	[APQ8016_LPAIF_MI2S_SECONDARY] =  { 
		.id = APQ8016_LPAIF_MI2S_SECONDARY,
		.name = "Secondary MI2S",
		.playback = {
			.stream_name	= "Secondary Playback",
			.formats	= SNDRV_PCM_FMTBIT_S16 |
						SNDRV_PCM_FMTBIT_S24 |
						SNDRV_PCM_FMTBIT_S32,
			.rates		= SNDRV_PCM_RATE_8000 |
						SNDRV_PCM_RATE_16000 |
						SNDRV_PCM_RATE_32000 |
						SNDRV_PCM_RATE_48000 |
						SNDRV_PCM_RATE_96000,
			.rate_min	= 8000,
			.rate_max	= 96000,
			.channels_min	= 1,
			.channels_max	= 8,
		},
		.probe	= &lpass_cpu_dai_probe,
		.ops    = &lpass_cpu_dai_ops,
	},
	[APQ8016_LPAIF_MI2S_TERTIARY] =  {
		.id = APQ8016_LPAIF_MI2S_TERTIARY,
		.name = "Tertiary MI2S",
		.playback = {
			.stream_name	= "Tertiary Capture",
			.formats	= SNDRV_PCM_FMTBIT_S16 |
						SNDRV_PCM_FMTBIT_S24 |
						SNDRV_PCM_FMTBIT_S32,
			.rates		= SNDRV_PCM_RATE_8000 |
						SNDRV_PCM_RATE_16000 |
						SNDRV_PCM_RATE_32000 |
						SNDRV_PCM_RATE_48000 |
						SNDRV_PCM_RATE_96000,
			.rate_min	= 8000,
			.rate_max	= 96000,
			.channels_min	= 1,
			.channels_max	= 8,
		},
		.probe	= &lpass_cpu_dai_probe,
		.ops    = &lpass_cpu_dai_ops,
	},
	[APQ8016_LPAIF_MI2S_QUATERNARY] =  {
		.id = APQ8016_LPAIF_MI2S_QUATERNARY,
		.name = "Quatenary MI2S",
		.playback = {
			.stream_name	= "Quatenary Playback",
			.formats	= SNDRV_PCM_FMTBIT_S16 |
						SNDRV_PCM_FMTBIT_S24 |
						SNDRV_PCM_FMTBIT_S32,
			.rates		= SNDRV_PCM_RATE_8000 |
						SNDRV_PCM_RATE_16000 |
						SNDRV_PCM_RATE_32000 |
						SNDRV_PCM_RATE_48000 |
						SNDRV_PCM_RATE_96000,
			.rate_min	= 8000,
			.rate_max	= 96000,
			.channels_min	= 1,
			.channels_max	= 8,
		},
		.capture =  {
		},
		.probe	= &lpass_cpu_dai_probe,
		.ops    = &lpass_cpu_dai_ops,
	},
};

static const struct snd_soc_component_driver lpass_cpu_comp_driver = {
	.name = "lpass-cpu",
};

static bool lpass_cpu_regmap_writeable(struct device *dev, unsigned int reg)
{
	int i;
	struct lpass_data *drvdata = dev_get_drvdata(dev);
	struct lpass_variant_data *v = drvdata->vdata;

	for (i = 0; i < v->i2s_ports; ++i)
		if (reg == LPAIF_I2SCTL_REG(v, i))
			return true;

	for (i = 0; i < v->irq_ports; ++i) {
		if (reg == LPAIF_IRQEN_REG(v, i))
			return true;
		if (reg == LPAIF_IRQCLEAR_REG(v, i))
			return true;
	}

	for (i = 0; i < v->rdma_channels; ++i) {
		if (reg == LPAIF_RDMACTL_REG(v, i))
			return true;
		if (reg == LPAIF_RDMABASE_REG(v, i))
			return true;
		if (reg == LPAIF_RDMABUFF_REG(v, i))
			return true;
		if (reg == LPAIF_RDMAPER_REG(v, i))
			return true;
	}

	return false;
}

static bool lpass_cpu_regmap_readable(struct device *dev, unsigned int reg)
{
	int i;
	struct lpass_data *drvdata = dev_get_drvdata(dev);
	struct lpass_variant_data *v = drvdata->vdata;

	for (i = 0; i < v->i2s_ports; ++i)
		if (reg == LPAIF_I2SCTL_REG(v, i))
			return true;

	for (i = 0; i < v->irq_ports; ++i) {
		if (reg == LPAIF_IRQEN_REG(v, i))
			return true;
		if (reg == LPAIF_IRQSTAT_REG(v, i))
			return true;
	}

	for (i = 0; i < v->rdma_channels; ++i) {
		if (reg == LPAIF_RDMACTL_REG(v, i))
			return true;
		if (reg == LPAIF_RDMABASE_REG(v, i))
			return true;
		if (reg == LPAIF_RDMABUFF_REG(v,i))
			return true;
		if (reg == LPAIF_RDMACURR_REG(v, i))
			return true;
		if (reg == LPAIF_RDMAPER_REG(v, i))
			return true;
	}

	return false;
}

static bool lpass_cpu_regmap_volatile(struct device *dev, unsigned int reg)
{
	int i;

	struct lpass_data *drvdata = dev_get_drvdata(dev);
	struct lpass_variant_data *v = drvdata->vdata;

	for (i = 0; i < v->irq_ports; ++i)
		if (reg == LPAIF_IRQSTAT_REG(v, i))
			return true;

	for (i = 0; i < v->rdma_channels; ++i)
		if (reg == LPAIF_RDMACURR_REG(v, i))
			return true;

	return false;
}

static const struct regmap_config lpass_cpu_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
//	.max_register = LPAIF_RDMAPER_REG(LPAIF_RDMA_CHAN_MAX),
	.writeable_reg = lpass_cpu_regmap_writeable,
	.readable_reg = lpass_cpu_regmap_readable,
	.volatile_reg = lpass_cpu_regmap_volatile,
	.cache_type = REGCACHE_FLAT,
};


struct lpass_variant_data apq8016_data = {
	.i2sctrl_reg_base	= 0x1000,
	.i2sctrl_reg_stride	= 0x1000,
	.i2s_ports		= 4,
	.irq_reg_base		= 0x6000,
	.irq_reg_stride		= 0x1000,
	.irq_ports		= 3,
	.rdma_reg_base		= 0x8400,
	.rdma_reg_stride 	= 0x1000,
	.rdma_channels		= 2,
};

struct lpass_variant_data ipq8064_data = {
	.i2sctrl_reg_base	= 0x0010,
	.i2sctrl_reg_stride 	= 0x04,
	.i2s_ports		= 5,
	.irq_reg_base		= 0x3000,
	.irq_reg_stride		= 0x1000,
	.irq_ports		= 3,
	.rdma_reg_base		= 0x6000,
	.rdma_reg_stride 	= 0x1000,
	.rdma_channels		= 4,
};

#ifdef CONFIG_OF
static const struct of_device_id lpass_cpu_device_id[] = {
	{ .compatible = "qcom,lpass-cpu", .data = &ipq8064_data },
	{ .compatible = "qcom,lpass-cpu-apq8016", .data = &apq8016_data },
	{}
};
MODULE_DEVICE_TABLE(of, lpass_cpu_device_id);
#endif

static int lpass_cpu_platform_probe(struct platform_device *pdev)
{
	struct lpass_data *drvdata;
	struct device_node *dsp_of_node;
	struct resource *res;
	int ret;

	dsp_of_node = of_parse_phandle(pdev->dev.of_node, "qcom,adsp", 0);
	if (dsp_of_node) {
		dev_err(&pdev->dev, "%s() DSP exists and holds audio resources\n",
				__func__);
		return -EBUSY;
	}

	drvdata = devm_kzalloc(&pdev->dev, sizeof(struct lpass_data),
			GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	platform_set_drvdata(pdev, drvdata);
	dev_set_drvdata(&pdev->dev, drvdata);

	drvdata->vdata = (struct lpass_variant_data *)of_match_node(
						lpass_cpu_device_id, pdev->dev.of_node)->data;
	if (!drvdata->vdata) {
		dev_err(&pdev->dev, "%s() no variant data\n", __func__);
		return -EINVAL;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "lpass-lpaif");
	if (!res) {
		dev_err(&pdev->dev, "%s() error getting resource\n", __func__);
		return -ENODEV;
	}

	drvdata->lpaif = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR((void const __force *)drvdata->lpaif)) {
		dev_err(&pdev->dev, "%s() error mapping reg resource: %ld\n",
				__func__,
				PTR_ERR((void const __force *)drvdata->lpaif));
		return PTR_ERR((void const __force *)drvdata->lpaif);
	}

	drvdata->lpaif_map = devm_regmap_init_mmio(&pdev->dev, drvdata->lpaif,
			&lpass_cpu_regmap_config);
	//FIXME set max register
	if (IS_ERR(drvdata->lpaif_map)) {
		dev_err(&pdev->dev, "%s() error initializing regmap: %ld\n",
				__func__, PTR_ERR(drvdata->lpaif_map));
		return PTR_ERR(drvdata->lpaif_map);
	}

#if 0
//FIXME
	drvdata->mi2s_osr_clk = devm_clk_get(&pdev->dev, "mi2s-osr-clk");
	if (IS_ERR(drvdata->mi2s_osr_clk)) {
		dev_err(&pdev->dev, "%s() error getting mi2s-osr-clk: %ld\n",
				__func__, PTR_ERR(drvdata->mi2s_osr_clk));
		return PTR_ERR(drvdata->mi2s_osr_clk);
	}
#endif

	drvdata->mi2s_bit_clk = devm_clk_get(&pdev->dev, "mi2s-bit-clk");
	if (IS_ERR(drvdata->mi2s_bit_clk)) {
		dev_err(&pdev->dev, "%s() error getting mi2s-bit-clk: %ld\n",
				__func__, PTR_ERR(drvdata->mi2s_bit_clk));
		return PTR_ERR(drvdata->mi2s_bit_clk);
	}

	drvdata->ahbix_clk = devm_clk_get(&pdev->dev, "ahbix-clk");
	if (IS_ERR(drvdata->ahbix_clk)) {
		dev_err(&pdev->dev, "%s() error getting ahbix-clk: %ld\n",
				__func__, PTR_ERR(drvdata->ahbix_clk));
		return PTR_ERR(drvdata->ahbix_clk);
	}

	ret = clk_set_rate(drvdata->ahbix_clk, LPASS_AHBIX_CLOCK_FREQUENCY);
	if (ret) {
		dev_err(&pdev->dev, "%s() error setting rate on ahbix_clk: %d\n",
				__func__, ret);
		return ret;
	}
	dev_dbg(&pdev->dev, "%s() set ahbix_clk rate to %lu\n", __func__,
			clk_get_rate(drvdata->ahbix_clk));

	ret = clk_prepare_enable(drvdata->ahbix_clk);
	if (ret) {
		dev_err(&pdev->dev, "%s() error enabling ahbix_clk: %d\n",
				__func__, ret);
		return ret;
	}
	drvdata->pcnoc_mport_clk = of_clk_get_by_name(pdev->dev.of_node, "pcnoc-mport-clk");
	if (IS_ERR(drvdata->pcnoc_mport_clk)) {
		dev_err(&pdev->dev, "%s() error getting pcnoc-mport-clk: %ld\n",
				__func__, PTR_ERR(drvdata->pcnoc_mport_clk));
		return PTR_ERR(drvdata->pcnoc_mport_clk);
	}

	ret= clk_prepare_enable(drvdata->pcnoc_mport_clk);
	if (ret) {
		dev_err(&pdev->dev, "%s() Error enabling ahbix_clk: %d\n",
				__func__, ret);
		return ret;
	}

	drvdata->pcnoc_sway_clk = of_clk_get_by_name(pdev->dev.of_node, "pcnoc-sway-clk");
	if (IS_ERR(drvdata->pcnoc_sway_clk)) {
		dev_err(&pdev->dev, "%s() error getting pcnoc-sway-clk: %ld\n",
				__func__, PTR_ERR(drvdata->pcnoc_sway_clk));
		return PTR_ERR(drvdata->pcnoc_sway_clk);
	}

	ret = clk_prepare_enable(drvdata->pcnoc_sway_clk);
	if (ret) {
		dev_err(&pdev->dev, "%s() Error enabling ahbix_clk: %d\n",
				__func__, ret);
		return ret;
	}

	if (of_device_is_compatible(pdev->dev.of_node, "qcom,lpass-cpu-apq8016"))
		ret = devm_snd_soc_register_component(&pdev->dev,
			&lpass_cpu_comp_driver, apq8016_lpass_cpu_dai_driver,
			ARRAY_SIZE(apq8016_lpass_cpu_dai_driver));
	else
		ret = devm_snd_soc_register_component(&pdev->dev,
			&lpass_cpu_comp_driver, &lpass_cpu_dai_driver, 1);
	if (ret) {
		dev_err(&pdev->dev, "%s() error registering cpu driver: %d\n",
				__func__, ret);
		goto err_clk;
	}

	ret = asoc_qcom_lpass_platform_register(pdev);
	if (ret) {
		dev_err(&pdev->dev, "%s() error registering platform driver: %d\n",
				__func__, ret);
		goto err_clk;
	}

	return 0;

err_clk:
	clk_disable_unprepare(drvdata->ahbix_clk);
	return ret;
}

static int lpass_cpu_platform_remove(struct platform_device *pdev)
{
	struct lpass_data *drvdata = platform_get_drvdata(pdev);

	clk_disable_unprepare(drvdata->ahbix_clk);

	return 0;
}

static struct platform_driver lpass_cpu_platform_driver = {
	.driver	= {
		.name		= "lpass-cpu",
		.of_match_table	= of_match_ptr(lpass_cpu_device_id),
	},
	.probe	= lpass_cpu_platform_probe,
	.remove	= lpass_cpu_platform_remove,
};
module_platform_driver(lpass_cpu_platform_driver);

MODULE_DESCRIPTION("QTi LPASS CPU Driver");
MODULE_LICENSE("GPL v2");
