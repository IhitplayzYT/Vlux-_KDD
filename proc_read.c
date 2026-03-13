#include "linux/minmax.h"
#include "linux/sched.h"
#include "linux/types.h"
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched/signal.h>
#include <linux/uaccess.h>
#include <stdio.h>

#define kprintf(s, ...) printk(s##"\n", __VA_ARGS__);
#define LOG(a, b) printk("<1>[%s] %s\n", (a), (b))
#define WARN(a, b) printk(KERN_WARN "[%s] %s\n", (a), (b))
#define DEBUG(a, b) printk(KERN_DEBUG "[%s] %s\n", (a), (b))
#define EP_name "vlux"
#define BUFF_SZ 4096

MODULE_AUTHOR("Ihit R Acharya");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A module to retrive detailed kernel Proc list");

static int major_id;
static char buff[BUFF_SZ];
static int cur_pos = 0;

static int update_proc_tree(struct inode *ino, struct file *) {

  struct task_struct *task;
  for_each_process(task) {
    int len = snprintf(b, unsigned long, const char *restrict, ...)
  }
}

static ssize_t get_proc_tree(struct file *fp, char __user *usr, size_t cnt,
                             loff_t *offset) {
  ssize_t rem = BUFF_SZ - cur_pos;
  ssize_t bytesleft = min(cnt, rem);
  if (bytesleft <= 0)
    return 0;

  if (copy_to_user(usr, buff + cur_pos, bytesleft)) {
    return -EFAULT;
  }
  cur_pos += bytesleft;
  return bytesleft;
}
static struct file_operations fops = {
    .read = get_proc_tree,
    .open = update_proc_tree,
};

static int __init init(void) {
  major_id = register_chrdev(0, EP_name, &fops);
  if (major_id < 0) {
    LOG("FAULT", "Unable to add a chr device");
    return -1;
  }
  LOG("SUCCESS", "Module loaded into the kernel!");
  return 0;
}

static void __exit dinit(void) {
  unregister_chrdev(major_id, EP_name);
  LOG("SUCCESS", "Module successfully offloaded from the kernel!");
}

module_init(init);
module_exit(dinit);
