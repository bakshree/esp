// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0
#include <linux/of_device.h>
#include <linux/mm.h>

#include <asm/io.h>

#include <esp_accelerator.h>
#include <esp.h>

#include "tiled_acc_stratus.h"

#define DRV_NAME	"tiled_acc_stratus"

/* <<--regs-->> */
#define TILED_ACC_NUM_TILES_REG 0x44
#define TILED_ACC_TILE_SIZE_REG 0x40

struct tiled_acc_stratus_device {
	struct esp_device esp;
};

static struct esp_driver tiled_acc_driver;

static struct of_device_id tiled_acc_device_ids[] = {
	{
		.name = "SLD_TILED_ACC_STRATUS",
	},
	{
		.name = "eb_027",
	},
	{
		.compatible = "sld,tiled_acc_stratus",
	},
	{ },
};

static int tiled_acc_devs;

static inline struct tiled_acc_stratus_device *to_tiled_acc(struct esp_device *esp)
{
	return container_of(esp, struct tiled_acc_stratus_device, esp);
}

static void tiled_acc_prep_xfer(struct esp_device *esp, void *arg)
{
	struct tiled_acc_stratus_access *a = arg;

	/* <<--regs-config-->> */
	iowrite32be(a->num_tiles, esp->iomem + TILED_ACC_NUM_TILES_REG);
	iowrite32be(a->tile_size, esp->iomem + TILED_ACC_TILE_SIZE_REG);
	iowrite32be(a->src_offset, esp->iomem + SRC_OFFSET_REG);
	iowrite32be(a->dst_offset, esp->iomem + DST_OFFSET_REG);

}

static bool tiled_acc_xfer_input_ok(struct esp_device *esp, void *arg)
{
	/* struct tiled_acc_stratus_device *tiled_acc = to_tiled_acc(esp); */
	/* struct tiled_acc_stratus_access *a = arg; */

	return true;
}

static int tiled_acc_probe(struct platform_device *pdev)
{
	struct tiled_acc_stratus_device *tiled_acc;
	struct esp_device *esp;
	int rc;

	tiled_acc = kzalloc(sizeof(*tiled_acc), GFP_KERNEL);
	if (tiled_acc == NULL)
		return -ENOMEM;
	esp = &tiled_acc->esp;
	esp->module = THIS_MODULE;
	esp->number = tiled_acc_devs;
	esp->driver = &tiled_acc_driver;
	rc = esp_device_register(esp, pdev);
	if (rc)
		goto err;

	tiled_acc_devs++;
	return 0;
 err:
	kfree(tiled_acc);
	return rc;
}

static int __exit tiled_acc_remove(struct platform_device *pdev)
{
	struct esp_device *esp = platform_get_drvdata(pdev);
	struct tiled_acc_stratus_device *tiled_acc = to_tiled_acc(esp);

	esp_device_unregister(esp);
	kfree(tiled_acc);
	return 0;
}

static struct esp_driver tiled_acc_driver = {
	.plat = {
		.probe		= tiled_acc_probe,
		.remove		= tiled_acc_remove,
		.driver		= {
			.name = DRV_NAME,
			.owner = THIS_MODULE,
			.of_match_table = tiled_acc_device_ids,
		},
	},
	.xfer_input_ok	= tiled_acc_xfer_input_ok,
	.prep_xfer	= tiled_acc_prep_xfer,
	.ioctl_cm	= TILED_ACC_STRATUS_IOC_ACCESS,
	.arg_size	= sizeof(struct tiled_acc_stratus_access),
};

static int __init tiled_acc_init(void)
{
	return esp_driver_register(&tiled_acc_driver);
}

static void __exit tiled_acc_exit(void)
{
	esp_driver_unregister(&tiled_acc_driver);
}

module_init(tiled_acc_init)
module_exit(tiled_acc_exit)

MODULE_DEVICE_TABLE(of, tiled_acc_device_ids);

MODULE_AUTHOR("Emilio G. Cota <cota@braap.org>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("tiled_acc_stratus driver");
