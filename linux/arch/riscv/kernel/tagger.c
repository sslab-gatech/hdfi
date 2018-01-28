#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/bootmem.h>
#include <tagger.h>

#include <asm/io.h>


static void* tag_table_base;
static void* mtt_base;
volatile static long* tag_control_base;

#define TAG_TABLE_SIZE_IN_MB	(8ul)
#define MTT_SIZE_IN_KB 		(128)
#define SCR_BASE	(0x40008000)
#define TAGGER_CONTROL_OFFSET	(8)
void __init tagger_init(void) {

  unsigned long size = TAG_TABLE_SIZE_IN_MB << 20; 
  unsigned long mtt_size = MTT_SIZE_IN_KB << 10;
  pr_notice("Allocating memory for the tag table\n");
  tag_table_base = alloc_bootmem_low_pages(size); 
  pr_notice("Allocated tag table (%lxBytes=%dMB) at:\n", size, size >> 20);
  pr_notice("phys=%p,\tvirt=%p\n",virt_to_phys(tag_table_base),tag_table_base);
  pr_notice("Allocating memory for the mtt\n");
  mtt_base = alloc_bootmem_low_pages(mtt_size); 
  pr_notice("Allocated mtt (%lxBytes=%dKB) at:\n", mtt_size, mtt_size >> 10);
  pr_notice("phys=%p,\tvirt=%p\n",virt_to_phys(mtt_base),mtt_base);


}

void __init tagger_init_late(void) {

  tag_control_base = (unsigned int)SCR_BASE;
  tag_control_base = (unsigned long*)ioremap_nocache(tag_control_base,PAGE_SIZE);
  tag_control_base = tag_control_base + TAGGER_CONTROL_OFFSET;
  pr_notice("tagger_control is mapped at:\t%p\n",tag_control_base);
  unsigned int repl;

  //pr_notice("magic:\t%lx\n",(*tag_control_base) & 0xFFFF000000000000);
  //pr_notice("magic:\t%lx\n",((*tag_control_base) & 0xFFFF000000000000) >> 48);
  if(((*tag_control_base) & 0x0FFF000000000000ul) >> 48 == 0xDF1){
    pr_notice("detected tagger\n");
    pr_notice("\tTagCacheLines:\t%u\n",(*tag_control_base >> 40) & 0x3F);
    pr_notice("\thasL2Cache:\t%x\n",(*tag_control_base >> 46) & 0x1);
    pr_notice("\thasTVB:\t%x\n",(*tag_control_base >> 36) & 0x1);
    pr_notice("\thasTagger:\t%x\n",(*tag_control_base >> 35) & 0x1);
    pr_notice("\tenableMetaTag:\t%x\n",(*tag_control_base >> 34) & 0x1);
    repl = (*tag_control_base >> 37) & 0x3;
    pr_notice("\treplacementPolicy:\t%x\n",repl);
    if(repl == 0) {
      pr_notice("\t\tplru\n"); 
    }
    else if (repl == 1) {
      pr_notice("\t\trandom\n");
    }
    else {
      pr_notice("\t\tfifo\n");
    }
    pr_notice("\thasDebugCounters:\t%x\n",(*tag_control_base >> 39) & 0x1);
    pr_notice("tagger_control before:\t%lx\n",*tag_control_base);
    *tag_control_base = (*tag_control_base &  0xFFFFFFFF00000000) | ((unsigned long)virt_to_phys(tag_table_base) & 0xFFFFFFFF);
    pr_notice("configured tag_table_base as:\t%lx\n",*tag_control_base & 0xFFFFFFFF);
    pr_notice("tagger_control_2 before:\t%lx\n",*(tag_control_base+24));
    *(tag_control_base+24) = (*(tag_control_base+24) &  0xFFFFFFFF00000000) | ((unsigned long)virt_to_phys(mtt_base) & 0xFFFFFFFF);
    pr_notice("configured mtt_base as:\t%lx\n",*(tag_control_base+24) & 0xFFFFFFFF);
    pr_notice("enabling tagger\n"); 
    pr_notice("tagger_control after updating the base addr:\t%lx\n",*tag_control_base);
    *tag_control_base = (*tag_control_base) | 0x200000000ul; //enable writeback
    *tag_control_base = (*tag_control_base) | 0x100000000ul; //enable reroute
    pr_notice("tagger enable\n");

    pr_notice("tagger_control after:\t%lx\n",*tag_control_base);
    pr_notice("(wb_enable,enable):\t(%x,%x)\n",(*(tag_control_base) & 0x0000000200000000) >> 33 , ((*tag_control_base) & 0x0000000100000000) >> 32   )    ;

  }
  else {
    pr_notice("cannot find the tagger,\t%x\n",*tag_control_base);
  }


}


