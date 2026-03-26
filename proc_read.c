#include "proc_read.h"
#include "asm-generic/errno-base.h"
#include "linux/elf.h"
#include "linux/types.h"
#include "linux/uaccess.h"

static int major_id;
static char buff[BUFF_SZ];
static int cur_pos = 0;

static int update_proc_tree(struct inode *ino, struct file *fp) {

  struct task_struct *task;
  for_each_process(task) { /* Fmt the data from kernel */ }
  return 0;
}

static ssize_t get_proc_tree(struct file *fp, char __user *usr, size_t mx_cpy,
                             loff_t *offset) {
  ssize_t remaining = BUFF_SZ - cur_pos;
  if (*offset >= BUFF_SZ) {
    return 0;
  }

  if (copy_to_user(usr, buff, remaining <= mx_cpy ? remaining : mx_cpy)) {
    return -EFAULT;
  }

  return 0;
}

static ssize_t get_entry(struct file *fd, const char __user *target, size_t len,
                         loff_t *off) {

  return 0;
}

static struct file_operations fops = {
    .read = get_proc_tree, .open = update_proc_tree, .write = get_entry};

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
