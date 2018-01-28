#include <linux/printk.h>
#include <linux/cred.h>

#include <asm/sections.h>

void hdfi_mark_cred(struct cred *cred)
{
	unsigned char *p = (unsigned char*)&cred->uid;
	int i;

	for (i = 0; i < 64; i += 8, p += 8) {
		asm volatile(
		"	ld	t0,(%0)\n"
		"	sdset1	t0,(%0)\n"
		: : "r" (p) : "t0");
	}
}
