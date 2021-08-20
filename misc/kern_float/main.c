#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/tty.h>      
#include <linux/console.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/cpufreq.h>
#include <asm/fpu/api.h>

static uint32_t read_latency_ns = 150;
static uint32_t nvm_bandwidth = 8000;
static uint32_t dram_bandwidth = 50000;
static uint64_t cpu_freq_mhz = 2600UL;

static int __init kern_float_test_init(void) 
{
	float a, b, c;
	int size = 4096;
	uint32_t lat;

	printk("kern_float test init\n");

	kernel_fpu_begin();

#if 0
	a = 1.0;
	b = 2.0;
	c = a / b;

	if (c == 0.5)
		printk("OK!\n");

	if (c == 0.4)
		printk("Nah\n");

	if (c < 1 )
		printk("OK!\n");
#endif
	lat = (int)size * (1 - (float)(((float)nvm_bandwidth)/1000)/(((float)dram_bandwidth)/1000))/(((float)nvm_bandwidth)/1000);

	printk("cpufreq %lu\n", cpufreq_get(0) / 1000);

	printk("%d\n", lat);

	kernel_fpu_end();

	return 0;
}

static void __exit kern_float_test_exit(void) 
{
	printk("kern_float test exit\n");
	return;
}

MODULE_LICENSE("GPL");
module_init(kern_float_test_init);
module_exit(kern_float_test_exit);
