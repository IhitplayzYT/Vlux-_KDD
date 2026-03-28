#include "proc_read.h"
#include "asm-generic/errno-base.h"
#include "linux/atomic/atomic-instrumented.h"
#include "linux/cred.h"
#include "linux/dcache.h"
#include "linux/elf.h"
#include "linux/fdtable.h"
#include "linux/init.h"
#include "linux/list.h"
#include "linux/mm.h"
#include "linux/mm_types.h"
#include "linux/mmap_lock.h"
#include "linux/printk.h"
#include "linux/rcupdate.h"
#include "linux/sched.h"
#include "linux/sched/prio.h"
#include "linux/sched/signal.h"
#include "linux/spinlock.h"
#include "linux/sprintf.h"
#include "linux/sysctl.h"
#include "linux/types.h"
#include "linux/uaccess.h"
#include "linux/uidgid.h"
#include "vdso/page.h"

#ifndef OUR_BUFFS
#define OUR_BUFFS
static int major_id;
static char Kbuff[KBUFF_SZ];
static char __K_children_buff[BUFF_SZ];
static char __K_siblings_buff[BUFF_SZ];
static char __Fd_buff[BUFF_SZ];
static char __Nsproxy_buff[BUFF_SZ];
static char __Mmap_buff[BUFF_SZ];
static int cur_pos = 0;
#endif

static void get_related(struct task_struct * head) {
  struct task_struct * child;
  ssize_t c_curr = 0,s_curr = 0;
  size_t cnt = 0;
  struct task_struct *og = head;
  list_for_each_entry(child,&(head->children),sibling){
    c_curr += scnprintf(__K_children_buff+c_curr,BUFF_SZ > c_curr?BUFF_SZ - c_curr:0 ,"%d, ",child->pid);
    if (cnt > 3 || BUFF_SZ <= c_curr){
      break;
    }
    cnt += 1;
  }
  if (c_curr >= 2)
    __K_children_buff[c_curr - 2] = '\0';

  cnt = 0;
  list_for_each_entry(child,&(og->sibling),sibling){
    s_curr += scnprintf(__K_siblings_buff+s_curr,BUFF_SZ > s_curr?BUFF_SZ - s_curr:0 ,"%d, ",child->pid);
    if (cnt > 3 || BUFF_SZ <= s_curr){
      break;
    }
    cnt += 1;
  }
  cnt = 0;
  if (s_curr >= 2)
      __K_siblings_buff[s_curr - 2] = '\0';
}

static void get_mmap(struct task_struct * task) {
  rcu_read_lock();
  struct mm_struct * mmap = (task->mm)? task->mm:task->active_mm;
  mmap_read_lock(mmap);
  if (!scnprintf(__Mmap_buff, BUFF_SZ,
        "CLI args => strt=0x%lx end=0x%lx\n"
        "Env      => strt=0x%lx end=0x%lx\n"
        "Stack    => base=0x%lx\n"
        "Heap     => strt=0x%lx curr=0x%lx\n"
        "Data     => strt=0x%lx end=0x%lx\n"
        "Code     => strt=0x%lx end=0x%lx\n"
        "Phy Mem  => cur=%ld KB  peak=%ld KB\n"
        "Vir Mem  => cur=%ld KB Distrb=[Pinned(%llu KB) Stack(%ld KB) Data(%ld KB) Exec(%ld KB) Locked(%ld KB)]  peak=%ld KB\n"
        "Size = %ld KB\n"
        "Referenced by %d process\n"
        "\n",
    mmap->arg_start,mmap->arg_end,
    mmap->env_start,mmap->env_end,
    mmap->start_stack,
    mmap->start_brk,mmap->brk,
    mmap->start_data,mmap->end_data,
    mmap->start_code,mmap->end_code,
    (get_mm_rss(mmap) << PAGE_SHIFT) >> 10,(mmap->hiwater_rss << PAGE_SHIFT) >> 10,
    (mmap->total_vm << PAGE_SHIFT) >> 10,(atomic64_read(&mmap->pinned_vm) << PAGE_SHIFT) >> 10,(mmap->stack_vm << PAGE_SHIFT) >> 10,(mmap->data_vm << PAGE_SHIFT) >> 10,(mmap->exec_vm << PAGE_SHIFT) >> 10,(mmap->locked_vm << PAGE_SHIFT) >> 10,(mmap->hiwater_vm << PAGE_SHIFT) >> 10,
    (mmap->task_size << PAGE_SHIFT) >> 10,
    atomic_read(&mmap->mm_count)
  )) goto end;  
  mmap_read_unlock(mmap);

end:
  rcu_read_unlock();
}

static void get_fds(struct task_struct * task) {
  
struct files_struct * file;
struct fdtable * fdmap;
ssize_t cur_p = 0;
  rcu_read_lock();
  file = rcu_dereference(task->files);
  if (!file) {rcu_read_unlock();return;}
  spin_lock(&file->file_lock);
  fdmap = files_fdtable(file);
  for (size_t i = 0 ; i < fdmap->max_fds; i++) {
    char buff[256];
    struct file * f = rcu_dereference(fdmap->fd[i]);
    char * name = d_path(&f->f_path,buff,256);
    cur_p += scnprintf(__Fd_buff, BUFF_SZ > cur_p? BUFF_SZ - cur_p:0,"(%lu)%s,",i,name);
  }
  spin_unlock(&file->file_lock);
  rcu_read_unlock();
  if (cur_p > 2) {
    __Fd_buff[cur_p - 2] = 0;
  }
}

static size_t ns_id(struct ns_common * ns) {
  return ns?ns->inum:0;
}

static void dump_nsproxy(struct task_struct * task){
struct nsproxy * nsp;
rcu_read_lock();
nsp = rcu_dereference(task->nsproxy);
if (!nsp) {rcu_read_unlock();return;}
snprintf(__Nsproxy_buff, BUFF_SZ, "%lu, %lu, %lu, %lu, %lu, %lu %lu %lu",ns_id(&nsp->uts_ns->ns),ns_id(&nsp->ipc_ns->ns),ns_id((struct ns_common*)&nsp->mnt_ns),
ns_id(&nsp->pid_ns_for_children->ns),ns_id((struct ns_common*)&nsp->net_ns),ns_id(&nsp->time_ns->ns),ns_id(&nsp->time_ns_for_children->ns),ns_id(&nsp->cgroup_ns->ns));
rcu_read_unlock();
}

static int update_proc_tree(struct inode *ino, struct file *fp) {

  struct task_struct *task;
  for_each_process(task) { /* Fmt the data from kernel */
    const struct cred* ids = task->cred; 
    unsigned int state = task->__state;
    char other_state[10];
    scnprintf(other_state,10,"0x%x",state);

    char rpar[16],par[16],name[16],gleader[16];
    get_task_comm(rpar,task->real_parent);
    get_task_comm(par,task->parent);
    get_task_comm(name,task);
    get_task_comm(gleader,task->group_leader);
    get_related(task);
    get_fds(task);
    dump_nsproxy(task);
    get_mmap(task);
    cur_pos += scnprintf(Kbuff+cur_pos,KBUFF_SZ>cur_pos?KBUFF_SZ-cur_pos:0,"=======Process: <%s>======={\n"
      "Status                 => %s\n"
      "ProcessConfig          => Flags(0x%x) Policy(%d)\n ExitCode(%d) ExitSignal(%d) ExitState(%d)"
      "ProcessIds             => [PiD,TGiD]: [%d, %d]\n"
      "UserIds                => [UiD, EUiD, SUiD, FSUiD]: [%d, %d, %d, %d]\n"
      "GroupIds               => [GiD, EGiD, SGiD, FSGiD]: [%d, %d, %d, %d]\n"
      "Parents(Detached? %s)  => [K_Real, U_Parent, GroupLeader]: [[%d]%s, [%d]%s, [%d]%s]\n"
      "Childrens              => [%s, ...]\n"
      "Siblings               => [%s, ...]\n"
      "Scheduling             => [Niceness, Static, Normal, Net, RealTime]: [%d, %d, %d, %d, %d]\n"
      "Open FDs               => [%s]\n"
      "NamespaceIds             => [UTS,IPC,MNT,PID_C,NET,TIME,TIME_C,CGRP]: [%s]\n"
      "Memory Layout: {\n"
      "%s"
      "}}\n"
      "============================",
      name,
      state == TASK_RUNNING ? "RUNNING":state == TASK_INTERRUPTIBLE?"SLEEPING":state == TASK_UNINTERRUPTIBLE?"DEEP_SLEEP":state == __TASK_STOPPED?"STOPPED":state == __TASK_TRACED?"DEBUGMODE":state==TASK_DEAD?"KILLED":state == EXIT_ZOMBIE ?"ZOMBIE":state == EXIT_DEAD?"EXITING":state == TASK_PARKED?"K_STOPPED":state== TASK_WAKEKILL?"PRIVILEGED_SLEEP":state == TASK_WAKING?"WAKING":state == TASK_NOLOAD?"NO_CPU_UTILISE":state == TASK_NEW?"SPAWNED":state == TASK_RTLOCK_WAIT?"RT_MUTEX_WAIT":state == TASK_FREEZABLE?"SUSPENDABLE":state == TASK_FROZEN ?"SUSPENDED":state == TASK_KILLABLE?"KILLABLE":state == TASK_IDLE?"IDLE":other_state,
      task->flags,task->policy,task->exit_code,task->exit_signal,task->exit_state,
      task->pid,task->tgid,
      __kuid_val(ids->uid),__kuid_val(ids->euid),__kuid_val(ids->suid),__kuid_val(ids->fsuid),
      __kgid_val(ids->gid),__kgid_val(ids->egid),__kgid_val(ids->sgid),__kgid_val(ids->fsgid),
      task->pdeath_signal == 0?"Y":"N",task->real_parent->pid,rpar,task->parent->pid,par,task->group_leader->pid,gleader,
      __K_children_buff,
      __K_siblings_buff,
      PRIO_TO_NICE(task->prio),task->static_prio,task->normal_prio,task->prio,task->rt_priority,
      __Fd_buff,
      __Nsproxy_buff,
      __Mmap_buff
    );
  }
  return 0;
}




static ssize_t get_proc_tree(struct file *fp, char __user *usr, size_t mx_cpy,loff_t *offset) {
  ssize_t remaining = KBUFF_SZ - cur_pos;
  if (*offset >= KBUFF_SZ) {
    return 0;
  }

  if (copy_to_user(usr, Kbuff, remaining <= mx_cpy ? remaining : mx_cpy)) {
    return -EFAULT;
  }

  return 0;
}

static ssize_t get_entry(struct file *fd, const char __user *target, size_t len,loff_t *off) {
struct task_struct * task;
char find[256];
if (copy_from_user(find, target, 256)) {
 return -EFAULT;
}

for_each_process(task) {
char name[16];
get_task_comm(name, task);
if (strcmp(name,target) == 0) {
    const struct cred* ids = task->cred; 
    unsigned int state = task->__state;
    char other_state[10];
    scnprintf(other_state,10,"0x%x",state);

    char rpar[16],par[16],name[16],gleader[16];
    get_task_comm(rpar,task->real_parent);
    get_task_comm(par,task->parent);
    get_task_comm(name,task);
    get_task_comm(gleader,task->group_leader);
    get_related(task);
    get_fds(task);
    dump_nsproxy(task);
    get_mmap(task);
    printk("=======Process: <%s>======={\n"
      "Status                 => %s\n"
      "ProcessConfig          => Flags(0x%x) Policy(%d)\n ExitCode(%d) ExitSignal(%d) ExitState(%d)"
      "ProcessIds             => [PiD,TGiD]: [%d, %d]\n"
      "UserIds                => [UiD, EUiD, SUiD, FSUiD]: [%d, %d, %d, %d]\n"
      "GroupIds               => [GiD, EGiD, SGiD, FSGiD]: [%d, %d, %d, %d]\n"
      "Parents(Detached? %s)  => [K_Real, U_Parent, GroupLeader]: [[%d]%s, [%d]%s, [%d]%s]\n"
      "Childrens              => [%s, ...]\n"
      "Siblings               => [%s, ...]\n"
      "Scheduling             => [Niceness, Static, Normal, Net, RealTime]: [%d, %d, %d, %d, %d]\n"
      "Open FDs               => [%s]\n"
      "NamespaceIds             => [UTS,IPC,MNT,PID_C,NET,TIME,TIME_C,CGRP]: [%s]\n"
      "Memory Layout: {\n"
      "%s"
      "}}\n"
      "============================",
      name,
      state == TASK_RUNNING ? "RUNNING":state == TASK_INTERRUPTIBLE?"SLEEPING":state == TASK_UNINTERRUPTIBLE?"DEEP_SLEEP":state == __TASK_STOPPED?"STOPPED":state == __TASK_TRACED?"DEBUGMODE":state==TASK_DEAD?"KILLED":state == EXIT_ZOMBIE ?"ZOMBIE":state == EXIT_DEAD?"EXITING":state == TASK_PARKED?"K_STOPPED":state== TASK_WAKEKILL?"PRIVILEGED_SLEEP":state == TASK_WAKING?"WAKING":state == TASK_NOLOAD?"NO_CPU_UTILISE":state == TASK_NEW?"SPAWNED":state == TASK_RTLOCK_WAIT?"RT_MUTEX_WAIT":state == TASK_FREEZABLE?"SUSPENDABLE":state == TASK_FROZEN ?"SUSPENDED":state == TASK_KILLABLE?"KILLABLE":state == TASK_IDLE?"IDLE":other_state,
      task->flags,task->policy,task->exit_code,task->exit_signal,task->exit_state,
      task->pid,task->tgid,
      __kuid_val(ids->uid),__kuid_val(ids->euid),__kuid_val(ids->suid),__kuid_val(ids->fsuid),
      __kgid_val(ids->gid),__kgid_val(ids->egid),__kgid_val(ids->sgid),__kgid_val(ids->fsgid),
      task->pdeath_signal == 0?"Y":"N",task->real_parent->pid,rpar,task->parent->pid,par,task->group_leader->pid,gleader,
      __K_children_buff,
      __K_siblings_buff,
      PRIO_TO_NICE(task->prio),task->static_prio,task->normal_prio,task->prio,task->rt_priority,
      __Fd_buff,
      __Nsproxy_buff,
      __Mmap_buff);
}
}
  return 0;
}

static struct file_operations fops = {
    .read = get_proc_tree, 
    .open = update_proc_tree, 
    .write = get_entry,
    .owner = THIS_MODULE
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
