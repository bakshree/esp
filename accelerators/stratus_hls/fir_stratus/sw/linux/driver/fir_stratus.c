// Copyright (c) 2011-2022 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0
#include <linux/of_device.h>
#include <linux/mm.h>

#include <asm/io.h>

#include <esp_accelerator.h>
#include <esp.h>

#include "fir_stratus.h"

#define DRV_NAME	"fir_stratus"

/* <<--regs-->> */
#define FIR_DATA_LENGTH_REG 0x40

struct fir_stratus_device {
	struct esp_device esp;
};

static struct esp_driver fir_driver;

static struct of_device_id fir_device_ids[] = {
	{
		.name = "SLD_FIR_STRATUS",
	},
	{
		.name = "eb_144",
	},
	{
		.compatible = "sld,fir_stratus",
	},
	{ },
};

static int fir_devs;

static inline struct fir_stratus_device *to_fir(struct esp_device *esp)
{
	return container_of(esp, struct fir_stratus_device, esp);
}

static void fir_prep_xfer(struct esp_device *esp, void *arg)
{
	struct fir_stratus_access *a = arg;

	/* <<--regs-config-->> */
	iowrite32be(a->data_length, esp->iomem + FIR_DATA_LENGTH_REG);
	iowrite32be(a->src_offset, esp->iomem + SRC_OFFSET_REG);
	iowrite32be(a->dst_offset, esp->iomem + DST_OFFSET_REG);

}

static bool fir_xfer_input_ok(struct esp_device *esp, void *arg)
{
	/* struct fir_stratus_device *fir = to_fir(esp); */
	/* struct fir_stratus_access *a = arg; */

	return true;
}

static int fir_probe(struct platform_device *pdev)
{
	struct fir_stratus_device *fir;
	struct esp_device *esp;
	int rc;

	fir = kzalloc(sizeof(*fir), GFP_KERNEL);
	if (fir == NULL)
		return -ENOMEM;
	esp = &fir->esp;
	esp->module = THIS_MODULE;
	esp->number = fir_devs;
	esp->driver = &fir_driver;
	rc = esp_device_register(esp, pdev);
	if (rc)
		goto err;

	fir_devs++;
	return 0;
 err:
	kfree(fir);
	return rc;
}

static int __exit fir_remove(struct platform_device *pdev)
{
	struct esp_device *esp = platform_get_drvdata(pdev);
	struct fir_stratus_device *fir = to_fir(esp);

	esp_device_unregister(esp);
	kfree(fir);
	return 0;
}

static struct esp_driver fir_driver = {
	.plat = {
		.probe		= fir_probe,
		.remove		= fir_remove,
		.driver		= {
			.name = DRV_NAME,
			.owner = THIS_MODULE,
			.of_match_table = fir_device_ids,
		},
	},
	.xfer_input_ok	= fir_xfer_input_ok,
	.prep_xfer	= fir_prep_xfer,
	.ioctl_cm	= FIR_STRATUS_IOC_ACCESS,
	.arg_size	= sizeof(struct fir_stratus_access),
};

static int __init fir_init(void)
{
	return esp_driver_register(&fir_driver);
}

static void __exit fir_exit(void)
{
	esp_driver_unregister(&fir_driver);
}

module_init(fir_init)
module_exit(fir_exit)

MODULE_DEVICE_TABLE(of, fir_device_ids);

MODULE_AUTHOR("Emilio G. Cota <cota@braap.org>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("fir_stratus driver");
