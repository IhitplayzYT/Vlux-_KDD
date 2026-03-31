#include "linux/minmax.h"
#include "linux/sched.h"
#include "linux/types.h"
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched/signal.h>
#include <linux/uaccess.h>
#include <linux/fdtable.h> 
#include <linux/ns_common.h>
#include <linux/nsproxy.h>
#include <linux/cgroup_namespace.h>
#include <linux/ipc_namespace.h>
#include <linux/mnt_namespace.h>
#include <linux/pid_namespace.h>
#include <linux/time_namespace.h>
#include <linux/user_namespace.h>
#include <linux/uts_namespace.h>
#include <linux/net_namespace.h>
#include <linux/proc_ns.h>
#include <linux/string.h>

#define kprintf(s, ...) printk(s##"\n", __VA_ARGS__);
#define LOG(a, b) printk("<1>[%s] %s\n", (a), (b))
#define DEBUG(a, b) printk(KERN_DEBUG "[%s] %s\n", (a), (b))
#define EP_name "vlux"
#define BUFF_SZ 4096
#define KBUFF_SZ (1 << 15)
#define MAX_ARGS 5

MODULE_AUTHOR("Ihit R Acharya");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A module to retrive detailed kernel Proc list");

