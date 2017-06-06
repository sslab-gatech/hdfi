#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

double getTime() 
{
  struct timeval t;
  gettimeofday(&t, NULL);
  return (double) t.tv_sec + ((double) t.tv_usec / 1000000.0);
}

#ifdef __VTV__
#include "vtv_map.h"
#include "vtv_set.h"

static void* get_vtable_ptr(void *obj) {
  void* vtbl_ptr = (void*)((uint64_t*)obj)[0];
  // printf("vtable: %p -> %p \n", obj, vtbl_ptr);
  return vtbl_ptr;
}

static void* get_virtual_func_ptr(void *obj, int index) {
  void *vtbl_ptr = get_vtable_ptr(obj);
  void* virtual_func_ptr = (void*)((uint64_t*)vtbl_ptr)[index];  
  return virtual_func_ptr;
}

struct insert_only_hash_map_allocator {
    /* N is the number of bytes to allocate.  */
    void *
    alloc (size_t n) const
    {  
      return malloc (n);
    }

    /* P points to the memory to be deallocated; N is the number of
       bytes to deallocate.  */
    void
    dealloc (void *p, size_t) const
    {
      free (p);
    }
  };

typedef uintptr_t int_vptr;

struct vptr_set_alloc
  {
    /* Memory allocator operator.  N is the number of bytes to be
       allocated.  */
    void *
    operator() (size_t n) const
      {
	return malloc (n);
      }
  };

struct vptr_hash
  {
    /* Hash function, used to convert vtable pointer, V, (a memory
       address) into an index into the hash table.  */
    size_t
    operator() (int_vptr v) const
      {
	const uint32_t x = 0x7a35e4d9;
	const int shift = (sizeof (v) == 8) ? 23 : 21;
	v = x * v;
	return v ^ (v >> shift);
      }
  };

typedef insert_only_hash_map <void*,
                              insert_only_hash_map_allocator > s2s;
typedef const s2s::key_type  vtv_symbol_key;

typedef insert_only_hash_sets<int_vptr, vptr_hash, vptr_set_alloc> vtv_sets;
typedef vtv_sets::insert_only_hash_set vtv_set;
typedef vtv_set * vtv_set_handle;
typedef vtv_set_handle * vtv_set_handle_handle; 

s2s *vtv_table;

#define KEY_SIZE 4

void vtv_init() {
  vtv_table = s2s::create(1024);
}

void cps_get(void* func_ptr) {
  s2s::key_type key;
  const s2s::value_type *pValue;

  key.n = KEY_SIZE;
  key.hash = (uint64_t)func_ptr;

  pValue = vtv_table->get(&key);
  if (!pValue) {
    printf("(cps)ERROR:cannot find ptr : %p!\n", func_ptr);
  }
  return;
}

void cps_insert(void* func_ptr) {
  s2s::key_type* pKey = (s2s::key_type*)malloc(sizeof(s2s::key_type));
  s2s::value_type *pValue;

  pKey->n = KEY_SIZE;
  pKey->hash = (uint64_t)func_ptr;

  s2s::value_type *value_ptr;
  vtv_table->find_or_add_key(pKey, &value_ptr);
  *value_ptr = func_ptr;

  // printf("(cps) inserted ptr : %p\n", func_ptr);
  return;
}

void vtv_insert(void* vtbl_ptr, void* virtual_func_ptr) {
  // insert
  s2s::key_type* pKey = (s2s::key_type*)malloc(sizeof(s2s::key_type));
  s2s::value_type *pValue;

  pKey->n = KEY_SIZE;
  pKey->hash = (uint64_t)vtbl_ptr;

  s2s::value_type *value_ptr;
  vtv_table->find_or_add_key(pKey, &value_ptr);
  *value_ptr = vtbl_ptr;

  // printf("(vtv)inserted ptr : %p\n", vtbl_ptr);
  return;
}

void vtv_get(void* vtbl_ptr, void* virtual_func_ptr) {
  s2s::key_type* pKey = (s2s::key_type*)malloc(sizeof(s2s::key_type));
  const s2s::value_type *pValue;

  pKey->n = KEY_SIZE;
  pKey->hash = (uint64_t)vtbl_ptr;

  pValue = vtv_table->get(pKey);
  if (!pValue) {
    printf("(vtv) ERROR:cannot find ptr : %p!\n", vtbl_ptr);
  }
  return;
}

#endif // __VTV__

class A {
public:
  virtual int foo(int x) = 0;
};

class B: public A {
public:  
  virtual int foo(int x) { return x+1;}
  int b;
};

class C: public B {
public:
  virtual int foo(int x) { return x+2;}
  int c;
};


int main(int argc, char* argv[]) {
#ifdef __VTV__
  vtv_init();
#endif // __VTV__

  int num = 5;

  if (argc > 1) 
    num = atoi(argv[1]);    

  printf("num: %d\n", num);
  
  C* pC = new C;

#ifdef __VTV__
  // vtv_insert(get_vtable_ptr((void*)pC), 
  //            get_virtual_func_ptr((void*)pC, 0));
  cps_insert(get_virtual_func_ptr((void*)pC, 0));
#endif

  double before = getTime();
  int res = 0;
  for (int i=0; i<num; i++) {
#ifdef __VTV__
  // vtv_get(get_vtable_ptr((void*)pC), 
  //         get_virtual_func_ptr((void*)pC, 0));
  cps_get(get_virtual_func_ptr((void*)pC, 0));    
#endif
    res += pC->foo(i);
  }
  double after = getTime();

  printf("res: %d\n", res);
  printf("elapsed: %f\n", after-before);
  return 0;
}
