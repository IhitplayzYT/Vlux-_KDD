#include "proc_read.h"
#include "asm-generic/errno-base.h"
#include "linux/cred.h"
#include "linux/elf.h"
#include "linux/list.h"
#include "linux/sched.h"
#include "linux/sched/prio.h"
#include "linux/sched/signal.h"
#include "linux/sprintf.h"
#include "linux/sysctl.h"
#include "linux/types.h"
#include "linux/uaccess.h"
#include "linux/uidgid.h"

static int major_id;
static char Kbuff[BUFF_SZ];
static char __K_children_buff[BUFF_SZ];
static char __K_siblings_buff[BUFF_SZ];
static char __Fd_buff[BUFF_SZ];
static char __Nsproxy_buff[BUFF_SZ];
static int cur_pos = 0;


// TODO:
// FIXME:
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


static void get_fds(struct task_struct * task) {
  



}

static void get_nsproxy(struct task_struct * task){

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
    get_nsproxy(task);
    cur_pos += scnprintf(Kbuff+cur_pos,BUFF_SZ,"=======Process: <%s>======={\n"
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
      "NameSpaces             => [%s]\n"
      "}\n"
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
      __Nsproxy_buff
    );
  }
  return 0;
}




static ssize_t get_proc_tree(struct file *fp, char __user *usr, size_t mx_cpy,
                             loff_t *offset) {
  ssize_t remaining = BUFF_SZ - cur_pos;
  if (*offset >= BUFF_SZ) {
    return 0;
  }

  if (copy_to_user(usr, Kbuff, remaining <= mx_cpy ? remaining : mx_cpy)) {
    return -EFAULT;
  }

  return 0;
}

static ssize_t get_entry(struct file *fd, const char __user *target, size_t len,
                         loff_t *off) {
struct task_struct * task;
for_each_process(task) {


}



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
