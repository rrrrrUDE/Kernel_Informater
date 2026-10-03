/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include "ki.h"
#include "ki_kfunc.h"
#include "ki_process.h"

extern struct ki_kfunc ki_uname_kfunc;
extern struct ki_kfunc ki_module_kfunc;
extern struct ki_kfunc ki_mount_kfunc;

bool ki_debug = IS_ENABLED(CONFIG_KI_DEBUG);
struct ki_state ki_state = {
	.safemode = false,
	.config_active = false,
};

static int __init ki_core_init(void)
{
	int ret;
	mutex_init(&ki_state.lock);

	ret = ki_kfunc_register(&ki_uname_kfunc);
	if (ret) return ret;

	ret = ki_kfunc_register(&ki_module_kfunc);
	if (ret) {
		ki_kfunc_unregister(&ki_uname_kfunc);
		return ret;
	}

	ret = ki_kfunc_register(&ki_mount_kfunc);
	if (ret) {
		ki_kfunc_unregister(&ki_module_kfunc);
		ki_kfunc_unregister(&ki_uname_kfunc);
		return ret;
	}

	ret = ki_kfunc_register(&ki_process_kfunc);
	if (ret) {
		ki_kfunc_unregister(&ki_mount_kfunc);
		ki_kfunc_unregister(&ki_module_kfunc);
		ki_kfunc_unregister(&ki_uname_kfunc);
		return ret;
	}

#ifdef CONFIG_KI_BOOTAPPLY
	ret = ki_config_active();
	if (ret < 0)
		pr_warn("KI: initial persistent configuration apply failed: %d\n", ret);
#endif
	ret = ki_safemode_init();
	if (ret)
		goto err_process;
	ret = ki_device_init();
	if (ret)
		goto err_safe;
	ret = ki_hook_init();
	if (ret)
		goto err_device;

	pr_info("KI: Kernel Informater v%s initialized\n", KI_VERSION_STRING);
	return 0;

err_device:
	ki_device_exit();
err_safe:
	ki_safemode_exit();
err_process:
	ki_kfunc_unregister(&ki_process_kfunc);
	ki_kfunc_unregister(&ki_mount_kfunc);
	ki_kfunc_unregister(&ki_module_kfunc);
	ki_kfunc_unregister(&ki_uname_kfunc);
	return ret;
}

static void __exit ki_core_exit(void)
{
	ki_hook_exit();
	ki_device_exit();
	ki_safemode_exit();
	ki_kfunc_unregister(&ki_process_kfunc);
	ki_kfunc_unregister(&ki_mount_kfunc);
	ki_kfunc_unregister(&ki_module_kfunc);
	ki_kfunc_unregister(&ki_uname_kfunc);
}

module_init(ki_core_init);
module_exit(ki_core_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Kernel Informater");
MODULE_VERSION(KI_VERSION_STRING);
