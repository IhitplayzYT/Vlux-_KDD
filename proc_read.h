#include "linux/minmax.h"
#include "linux/sched.h"
#include "linux/types.h"
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched/signal.h>
#include <linux/uaccess.h>

#define kprintf(s, ...) printk(s##"\n", __VA_ARGS__);
#define LOG(a, b) printk("<1>[%s] %s\n", (a), (b))
#define DEBUG(a, b) printk(KERN_DEBUG "[%s] %s\n", (a), (b))
#define EP_name "vlux"
#define BUFF_SZ 4096

MODULE_AUTHOR("Ihit R Acharya");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A module to retrive detailed kernel Proc list");

