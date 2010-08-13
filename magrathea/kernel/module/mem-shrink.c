/* $Id: mem-shrink.c,v 1.2 2008/05/29 14:51:49 xdenemar Exp $ */

/** @file
 * Kernel memory shrinking helper for Magrathea.
 * @author Jiri Denemark
 * @date 2008
 * @version $Revision: 1.2 $ $Date: 2008/05/29 14:51:49 $
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/swap.h>
#include <linux/mm.h>
#include <linux/freezer.h>

#define REVISION    "$Revision: 1.2 $"

#define MIN_MEMORY      10240
#define MAX_ITERATIONS  20

#define print_info(fmt, args...) \
    printk(KERN_INFO "mem-shrink: " fmt ".\n", ## args)

#define print_error(fmt, args...) \
    printk(KERN_ERR "mem-shrink: " fmt ".\n", ## args)


static struct proc_dir_entry *proc_magrathea = NULL;
static struct proc_dir_entry *proc_shrink = NULL;
static int user_space_freezing = 0;


#define shrink_pages(si, target)                        \
    (((si).totalram - (si).freeram <= (target))         \
        ? 0                                             \
        : ((si).totalram - (si).freeram - (target)))


static int shrink_memory(int mem, int retry)
{
    struct sysinfo si;
    unsigned long target_pages;
    unsigned long pages;
    unsigned long done;
    int frozen = 0;

    if (user_space_freezing) {
        print_info("Stopping user space..");

        if (try_to_freeze_tasks(1)) {
            print_info("User space could not be stopped");
            thaw_tasks(1);
            return -EBUSY;
        }

        frozen = 1;
        print_info("User space stopped");
    }

    print_info("Shrinking memory to %d kB in %d round(s)",
               mem, retry);

    si_meminfo(&si);
    target_pages = mem >> (PAGE_SHIFT - 10);
    pages = shrink_pages(si, target_pages);

    print_info("Occupied pages: %lu, target: %lu, shrinking by: %lu",
               si.totalram - si.freeram, target_pages, pages);

    for ( ; retry; retry--) {
        done = shrink_all_memory(pages);

        si_meminfo(&si);
        pages = shrink_pages(si, target_pages);

        if (retry > 1) {
            print_info("%lu pages freed; %lu to be done in next %d round(s)",
                       done, pages, retry - 1);
        }
    }

    si_meminfo(&si);
    print_info("%lu occupied pages; %lu not freed",
               si.totalram - si.freeram,
               shrink_pages(si, target_pages));

    if (frozen) {
        print_info("Restarting user space..");
        thaw_tasks(1);
        print_info("User spaces restarted");
    }

    return 0;
}


static int proc_perm(struct inode *inode, int op, struct nameidata *idata)
{
    /* read by anybody, write by admin */
    if (op == 4 || capable(CAP_SYS_ADMIN))
        return 0;

    return -EACCES;
}


static int proc_open(struct inode *inode, struct file *file)
{
    try_module_get(THIS_MODULE);
    return 0;
}


static int proc_close(struct inode *inode, struct file *file)
{
    module_put(THIS_MODULE);
    return 0;
}


#define CMD_FREEZE  "freeze"

static ssize_t proc_read(struct file *file,
                         char *buffer,
                         size_t length,
                         loff_t *offset)
{
    static int finished = 0;
    int len;

    if (finished) {
        finished = 0;
        return 0;
    }

    /* return EOF to next read */
    finished = 1;

    len = snprintf(buffer, length,
        "user space freezing: %s\n"
        "\n"
        "commands:\t<kB>,<retries> (<kB> target size of occupied memory (>= %d),\n"
        "         \t                <retries> number of iterations (1..%d)\n"
        "         \t" CMD_FREEZE " on|off  (enable/disable user space freezing)\n",
        (user_space_freezing) ? "enabled" : "disabled",
        MIN_MEMORY, MAX_ITERATIONS);

    return len;
}


static ssize_t proc_write(struct file *file,
                          const char *buffer,
                          size_t length,
                          loff_t *offset)
{
    char mem[64];
    int params[3] = { 0, 0, 0 };
    int error;

    if (length <= 1)
        return -EBADMSG;

    if (length >= sizeof(mem))
        return -EFBIG;

    if (copy_from_user(mem, buffer, length))
        return -EFAULT;

    if (mem[length - 1] == '\n')
        mem[length - 1] = '\0';
    else
        mem[length] = '\0';

    if (strncmp(mem, CMD_FREEZE " ", strlen(CMD_FREEZE) + 1) == 0) {
        int new_user_space_freezing = 0;
        char *p = mem + strlen(CMD_FREEZE) + 1;

        if (strcmp(p, "on") == 0)
            new_user_space_freezing = 1;
        else if (strcmp(p, "off") == 0)
            new_user_space_freezing = 0;
        else
            return -EBADMSG;

        if (new_user_space_freezing == user_space_freezing) {
            print_info("User space freezing already %s",
                       (user_space_freezing) ? "enabled" : "disabled");
        }
        else {
            user_space_freezing = new_user_space_freezing;
            print_info("%s user space freezing",
                       (user_space_freezing) ? "Enabling" : "Disabling");
        }
    }
    else {
        get_options(mem, 3, params);

        if (params[0] < 1 || params[0] > MAX_ITERATIONS
            || params[1] < MIN_MEMORY)
            return -EBADMSG;

        if (params[2] == 0)
            params[2] = 1;

        error = shrink_memory(params[1], params[2]);
        if (error)
            return error;
    }

    return length;
}


static struct inode_operations proc_inode_ops = {
    .permission = proc_perm
};

static struct file_operations proc_file_ops = {
    .open = proc_open,
    .release = proc_close,
    .read = proc_read,
    .write = proc_write
};

static int __init shrinker_init(void)
{
    print_info("Memory shrinker for Magrathea, " REVISION);

    proc_magrathea = proc_mkdir("magrathea", NULL);
    if (proc_magrathea != NULL)
        proc_magrathea->owner = THIS_MODULE;
    else
        goto err;

    proc_shrink = create_proc_entry("shrink", 0644, proc_magrathea);
    if (proc_shrink == NULL)
        goto err;

    proc_shrink->owner = THIS_MODULE;
    proc_shrink->proc_iops = &proc_inode_ops;
    proc_shrink->proc_fops = &proc_file_ops;

    return 0;

err:
    print_error("Unable to create /proc/magrathea/shrink");

    return -1;
}


static void __exit shrinker_exit(void)
{
    if (proc_magrathea != NULL) {
        if (proc_shrink != NULL)
            remove_proc_entry("shrink", proc_magrathea);

        remove_proc_entry("magrathea", NULL);
    }

    print_info("Memory shrinker for Magrathea unloaded");
}


module_init(shrinker_init);
module_exit(shrinker_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jiri Denemark <jirka@ics.muni.cz>");
MODULE_DESCRIPTION("Memory shrinker for Magrathea to speedup memory ballooning");
MODULE_VERSION(REVISION);

