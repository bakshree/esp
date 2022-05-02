// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0
#include <linux/of_device.h>
#include <linux/mm.h>

#include <asm/io.h>

#include <esp_accelerator.h>
#include <esp.h>

#include "rt_accel_stratus.h"

#define DRV_NAME	"rt_accel_stratus"

/* <<--regs-->> */
#define RT_ACCEL_IMG_WIDTH_REG 0x44
#define RT_ACCEL_IMG_HEIGHT_REG 0x40

struct rt_accel_stratus_device {
	struct esp_device esp;
};

static struct esp_driver rt_accel_driver;

static struct of_device_id rt_accel_device_ids[] = {
	{
		.name = "SLD_RT_ACCEL_STRATUS",
	},
	{
		.name = "eb_027",
	},
	{
		.compatible = "sld,rt_accel_stratus",
	},
	{ },
};

static int rt_accel_devs;

static inline struct rt_accel_stratus_device *to_rt_accel(struct esp_device *esp)
{
	return container_of(esp, struct rt_accel_stratus_device, esp);
}

static void rt_accel_prep_xfer(struct esp_device *esp, void *arg)
{
	struct rt_accel_stratus_access *a = arg;

	/* <<--regs-config-->> */
	iowrite32be(a->img_width, esp->iomem + RT_ACCEL_IMG_WIDTH_REG);
	iowrite32be(a->img_height, esp->iomem + RT_ACCEL_IMG_HEIGHT_REG);
	iowrite32be(a->src_offset, esp->iomem + SRC_OFFSET_REG);
	iowrite32be(a->dst_offset, esp->iomem + DST_OFFSET_REG);

}

static bool rt_accel_xfer_input_ok(struct esp_device *esp, void *arg)
{
	/* struct rt_accel_stratus_device *rt_accel = to_rt_accel(esp); */
	/* struct rt_accel_stratus_access *a = arg; */

	return true;
}

static int rt_accel_probe(struct platform_device *pdev)
{
	struct rt_accel_stratus_device *rt_accel;
	struct esp_device *esp;
	int rc;

	rt_accel = kzalloc(sizeof(*rt_accel), GFP_KERNEL);
	if (rt_accel == NULL)
		return -ENOMEM;
	esp = &rt_accel->esp;
	esp->module = THIS_MODULE;
	esp->number = rt_accel_devs;
	esp->driver = &rt_accel_driver;
	rc = esp_device_register(esp, pdev);
	if (rc)
		goto err;

	rt_accel_devs++;
	return 0;
 err:
	kfree(rt_accel);
	return rc;
}

static int __exit rt_accel_remove(struct platform_device *pdev)
{
	struct esp_device *esp = platform_get_drvdata(pdev);
	struct rt_accel_stratus_device *rt_accel = to_rt_accel(esp);

	esp_device_unregister(esp);
	kfree(rt_accel);
	return 0;
}

static struct esp_driver rt_accel_driver = {
	.plat = {
		.probe		= rt_accel_probe,
		.remove		= rt_accel_remove,
		.driver		= {
			.name = DRV_NAME,
			.owner = THIS_MODULE,
			.of_match_table = rt_accel_device_ids,
		},
	},
	.xfer_input_ok	= rt_accel_xfer_input_ok,
	.prep_xfer	= rt_accel_prep_xfer,
	.ioctl_cm	= RT_ACCEL_STRATUS_IOC_ACCESS,
	.arg_size	= sizeof(struct rt_accel_stratus_access),
};

static int __init rt_accel_init(void)
{
	return esp_driver_register(&rt_accel_driver);
}

static void __exit rt_accel_exit(void)
{
	esp_driver_unregister(&rt_accel_driver);
}

module_init(rt_accel_init)
module_exit(rt_accel_exit)

MODULE_DEVICE_TABLE(of, rt_accel_device_ids);

MODULE_AUTHOR("Emilio G. Cota <cota@braap.org>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("rt_accel_stratus driver");
