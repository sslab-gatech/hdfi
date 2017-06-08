// See LICENSE for license details.

//**************************************************************************
// Towers of Hanoi benchmark
//--------------------------------------------------------------------------
//
// Towers of Hanoi is a classic puzzle problem. The game consists of
// three pegs and a set of discs. Each disc is a different size, and
// initially all of the discs are on the left most peg with the smallest
// disc on top and the largest disc on the bottom. The goal is to move all
// of the discs onto the right most peg. The catch is that you are only
// allowed to move one disc at a time and you can never place a larger
// disc on top of a smaller disc.
//
// This implementation starts with NUM_DISC discs and uses a recursive
// algorithm to sovel the puzzle.  The smips-gcc toolchain does not support
// system calls so printf's can only be used on a host system, not on the
// smips processor simulator itself. You should not change anything except
// the HOST_DEBUG and PREALLOCATE macros for your timing run.

#include "util.h"

//--------------------------------------------------------------------------
// Main
extern char* malloc();

void store_set_tag(unsigned long* addr, unsigned long data) {

  __asm__ __volatile__("sdset1\t%1, 0(%0)" : : "r" (addr), "r" (data): "memory" );
  //printf("store_set_tag:\t%p\t<-\t%lx\n",addr,data); 

}


unsigned long load_check_tag(unsigned long* addr) {

  unsigned long ret;

  __asm__ __volatile__("ldchk1\t%0, 0(%1)" : "=r" (ret) : "r" (addr) );
  //printf("load_check_tag:\t%p:\t%lx\n",addr,ret); 

  return ret;


}

unsigned long load_check_tag0(unsigned long* addr) {

  unsigned long ret;

  __asm__ __volatile__("ldchk0\t%0, 0(%1)" : "=r" (ret) : "r" (addr) );
  //printf("load_check_tag:\t%p:\t%lx\n",addr,ret); 

  return ret;


}


unsigned long addr_to_mtt_entry(unsigned long base, unsigned long addr) {

  unsigned long offset = (addr >> 18) << 6;
  return base + offset;


}


unsigned long addr_to_tag_entry(unsigned long base, unsigned long addr) {

  unsigned long offset = (addr >> 12) << 6;
  return base + offset;


}

  #define FILL	8
  //#define LOOP  768
  #define LOOP 8
  #define INTERVAL 1

 
  unsigned long arr_alloc[LOOP][FILL];

  unsigned long mtt_base = 0x700000;
  volatile unsigned long* scr_base = 0x40008000;
  unsigned long wb[16][2048];

int main( int argc, char* argv[] )
{

/*
  plain tag table:
  
  byteAddrBits = 3
  entryAddrBits = 9
  tableAddrBits = 20
  512bit -> 512words -> 512*8=4KB
  512 * 2^20 bits->2^20B
  2^29 bits = 2^26 bytes
  64MB

 */

  
  unsigned long* arr = 0x300000;
  unsigned long *critical;

  


  volatile unsigned long* tagger_base = scr_base + 8;
  volatile unsigned long tagger_control_val = *tagger_base;

  printf("nCores\t@%p:\t%d\n",scr_base,*scr_base);
  //printf("MMIOSize(MB)\t@%p:\t%d\n",scr_base+1,*(scr_base+1));
  printf("tagger_control\t@%p:\t%lx\n",scr_base+8,*tagger_base);
  



  //unsigned long* tagtable_base = (unsigned long*)malloc(4*1024*1024*1024);
  unsigned long* tagtable_base = 0x800000;
  unsigned long tagtable_mask = 0xffffffffl;
  *tagger_base = (tagger_control_val & 0xffffffff00000000l) | ((unsigned long)tagtable_base & tagtable_mask);
  printf("tagger_control should be:%lx\n",(unsigned long) & tagtable_mask);
  printf("tagger_control(updated)\t@%p:\t%lx\n",scr_base,*tagger_base);
  *tagger_base = *tagger_base | (0x200000000l); // WB enable
  *tagger_base = *tagger_base | (0x100000000l); // enable




  long j = 1;
  long i = 0;
  unsigned long val = 0xCC11ul;
  unsigned long* entry;
//{0x12000000, 0x10100000, 0x13000000, 0x15000000, 0x11000000};

  unsigned int mm = 0;
  int kkk = 0;
  mm = 0;
  for(kkk = 0; kkk < LOOP; kkk += INTERVAL){
  arr = &arr_alloc[kkk][0];
  //arr = &arr_alloc[0];
 
  printf("execute sdset1s\n");

  for(j = 0; j < FILL ; j = j +  32/32) {
    critical = &(arr[j]);
    //printf("critical:\t%x\n",critical);
    store_set_tag(critical,val);
   // printf("stored?\t%lx=%lx\n",val,load_check_tag(critical));
    if(val == load_check_tag(critical)) mm++;
  }
  printf("written %u out of %u\n",mm,FILL);
  mm = 0;

  }

  for(kkk = 0; kkk < LOOP; kkk += INTERVAL){
  //arr = &arr_alloc[0];
    //arr = arr_list[kkk];

  arr = &arr_alloc[kkk][0];

  for(unsigned int kkkk = 0; kkkk < 2; kkkk++){
 printf("1st stage lchk test to flush tag cache\n");
  mm = 0;
  for(j = 0; j < FILL ; j = j +  32/32) {
    critical = &(arr[j]);
    //printf("critical:\t%x\n",critical);
    //printf("stored?\t%lx=%lx\n",val,load_check_tag(critical));
    if(val == load_check_tag(critical)) mm++;
  }
  printf("mm:\t%lu\n",mm);
  }
  }


  printf("let's make them written back\n");


  long k = 0;
  for(k = 0; k < 16; k++){
    for(j = 128 ; j < 2048 ; j+=1) {
	wb[k][j+k] = wb[k][j-16+k-64];
    }
  }

  printf("wait for a moment..\n");
  for(j=0; j< 10240000; j++);



  for(kkk = 0; kkk < LOOP; kkk += INTERVAL){
  //arr = &arr_alloc[0];
    //arr = arr_list[kkk];

  arr = &arr_alloc[kkk][0];


 
  printf("check the tag entries\n");
  for(j = 0; j < 1 ; j = j + 32/32) {

    critical = &(arr[j]);
    //printf("critical:\t%x\n",critical);
    //printf("block for critical:\t%lx\n",((unsigned long)critical) >> 7);
    entry = addr_to_tag_entry((unsigned long)tagtable_base, (unsigned long)critical);
    //printf("block for entry:\t%lx\n",((unsigned long)entry) >> 7);
    printf("tag @ %p:\t",entry);
    mm = 0;
    for(i = 7; i >=0 ; i -= 1) {
      printf("%016lx",*(entry + i));
      for(unsigned int lll = 0; lll < 64; lll++){
        if( (( (*(entry+i) << (64-lll+1)) >> 63) == 1)) mm++;
      }
    }
    printf("\n"); 
    printf("%u tags found\n",mm);
  }
  printf("check the mtt entries\n");
   for(j = 0; j < 1 ; j = j + 32/32) {

    critical = &(arr[j]);
    //printf("critical:\t%x\n",critical);
    //printf("block for critical:\t%lx\n",((unsigned long)critical) >> 7);
    entry = addr_to_mtt_entry((unsigned long)mtt_base, (unsigned long)critical);
    //printf("block for entry:\t%lx\n",((unsigned long)entry) >> 7);
    printf("tag @ %p:\t",entry);
    mm = 0;
    for(i = 7; i >=0 ; i -= 1) {
      printf("%016lx",*(entry + i));
      for(unsigned int lll = 0; lll < 64; lll++){
        if( (( (*(entry+i) << (64-lll+1)) >> 63) == 1)) mm++;
      }
    }
    printf("\n"); 
    printf("%u mts found\n",mm);
  }
 

 
  printf("2nd stage lchk test after artificail cache flush\n");
  mm = 0;
  for(j = 0; j < FILL ; j = j +  32/32) {
    critical = &(arr[j]);
    //printf("critical:\t%x\n",critical);
    //printf("stored?\t%lx=%lx\n",val,load_check_tag(critical));
    if(val == load_check_tag(critical)) mm++;
  }
  printf("mm:\t%lu\n",mm);
  }

  val = 0xcc12;
  mm = 0;
  printf("force clear tags\n");
  for(kkk = 0; kkk < LOOP; kkk += INTERVAL){
  arr = &arr_alloc[kkk][0];
  //arr = &arr_alloc[0];
 
  for(j = 0; j < FILL ; j = j +  32/32) {
    critical = &(arr[j]);
    //printf("critical:\t%x\n",critical);
    //store_set_tag(critical,val);
   *critical = val;
   // printf("stored?\t%lx=%lx\n",val,load_check_tag(critical));
    if(val == load_check_tag0(critical)) mm++;
  }
  printf("written %u out of %u\n",mm,FILL);
  mm = 0;

  }


  for(unsigned int kkkk = 0; kkkk < 2; kkkk++){
 printf("1st stage lchk test to flush tag cache\n");
  mm = 0;
  for(j = 0; j < FILL ; j = j +  32/32) {
    critical = &(arr[j]);
    //printf("critical:\t%x\n",critical);
    //printf("stored?\t%lx=%lx\n",val,load_check_tag(critical));
    if(val == load_check_tag0(critical)) mm++;
  }
  printf("mm:\t%lu\n",mm);
  }


  printf("let's make them written back\n");


  for(k = 0; k < 16; k++){
    for(j = 128 ; j < 2048 ; j+=1) {
	wb[k][j+k] = wb[k][j-16+k-64];
    }
  }

  printf("wait for a moment..\n");
  for(j=0; j< 10240000; j++);



  for(kkk = 0; kkk < LOOP; kkk += INTERVAL){
  //arr = &arr_alloc[0];
    //arr = arr_list[kkk];

  arr = &arr_alloc[kkk][0];


 
  printf("check the tag entries\n");
  for(j = 0; j < 1 ; j = j + 32/32) {

    critical = &(arr[j]);
    //printf("critical:\t%x\n",critical);
    //printf("block for critical:\t%lx\n",((unsigned long)critical) >> 7);
    entry = addr_to_tag_entry((unsigned long)tagtable_base, (unsigned long)critical);
    //printf("block for entry:\t%lx\n",((unsigned long)entry) >> 7);
    printf("tag @ %p:\t",entry);
    mm = 0;
    for(i = 7; i >=0 ; i -= 1) {
      printf("%016lx",*(entry + i));
      for(unsigned int lll = 0; lll < 64; lll++){
        if( (( (*(entry+i) << (64-lll+1)) >> 63) == 1)) mm++;
      }
    }
    printf("\n"); 
    printf("%u tags found\n",mm);
  }
  printf("check the mtt entries\n");
   for(j = 0; j < 1 ; j = j + 32/32) {

    critical = &(arr[j]);
    //printf("critical:\t%x\n",critical);
    //printf("block for critical:\t%lx\n",((unsigned long)critical) >> 7);
    entry = addr_to_mtt_entry((unsigned long)mtt_base, (unsigned long)critical);
    //printf("block for entry:\t%lx\n",((unsigned long)entry) >> 7);
    printf("tag @ %p:\t",entry);
    mm = 0;
    for(i = 7; i >=0 ; i -= 1) {
      printf("%016lx",*(entry + i));
      for(unsigned int lll = 0; lll < 64; lll++){
        if( (( (*(entry+i) << (64-lll+1)) >> 63) == 1)) mm++;
      }
    }
    printf("\n"); 
    printf("%u mts found\n",mm);
  }
 

 
  printf("2nd stage lchk test after artificail cache flush\n");
  mm = 0;
  for(j = 0; j < FILL ; j = j +  32/32) {
    critical = &(arr[j]);
    //printf("critical:\t%x\n",critical);
    //printf("stored?\t%lx=%lx\n",val,load_check_tag(critical));
    if(val == load_check_tag0(critical)) mm++;
  }
  printf("mm:\t%lu\n",mm);
  }





  printf("the end of main\n");
  return 0;
}



