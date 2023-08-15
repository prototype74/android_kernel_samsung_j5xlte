#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/sec_class.h>
#include "ccic_sysfs.h"

struct device *ccic_device;
static int ccic_notifier_init_done = 0;
int ccic_notifier_init(void);

int ccic_notifier_init(void)
{
	int ret = 0;

	pr_info("%s\n", __func__);
	if(ccic_notifier_init_done) {
		pr_err("%s already registered\n", __func__);
		goto out;	
	}
	ccic_notifier_init_done = 1;
	ccic_device = device_create(sec_class, NULL, 0, NULL, "ccic");
	if (IS_ERR(ccic_device)) {
		pr_err("%s Failed to create device(switch)!\n", __func__);
		ret = -ENODEV;
		goto out;
	}

	/* create sysfs group */
	ret = sysfs_create_group(&ccic_device->kobj, &ccic_sysfs_group);
	if (ret) {
		pr_err("%s: ccic sysfs fail, ret %d", __func__, ret);
	}
out:
	return ret;
}

static void __exit ccic_notifier_exit(void)
{
	pr_info("usb: [M] %s exit\n", __func__);
}

device_initcall(ccic_notifier_init);
module_exit(ccic_notifier_exit);
