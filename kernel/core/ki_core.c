/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "ki.h"
#include "ki_kfunc.h"

extern struct ki_kfunc ki_uname_kfunc;

struct ki_state ki_state = {
	.safemode = false,
	.config_active = false,
};

static int __init ki_core_init(void)
{
	int ret;

	mutex_init(&ki_state.lock);

	ret = ki_kfunc_register(&ki_uname_kfunc);
	if (ret) {
		pr_err("KI: failed to register uname kfunc: %d\n", ret);
		return ret;
	}

	ret = ki_device_init();
	if (ret) {
		pr_err("KI: failed to register /dev/%s: %d\n",
		       KI_DEVICE_NAME, ret);
		ki_kfunc_unregister(&ki_uname_kfunc);
		return ret;
	}

	ret = ki_hook_init();
	if (ret) {
		pr_err("KI: hook backend initialization failed: %d\n", ret);
		ki_device_exit();
		ki_kfunc_unregister(&ki_uname_kfunc);
		return ret;
	}

	pr_info("KI: Kernel Informater v%s initialized\n",
		KI_VERSION_STRING);
	return 0;
}

static void __exit ki_core_exit(void)
{
	ki_hook_exit();
	ki_device_exit();
	ki_kfunc_unregister(&ki_uname_kfunc);
	pr_info("KI: Kernel Informater exited\n");
}

module_init(ki_core_init);
module_exit(ki_core_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Kernel Informater");
MODULE_VERSION(KI_VERSION_STRING);
