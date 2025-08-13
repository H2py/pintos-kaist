#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
#include "devices/disk.h"

struct page;
enum vm_type;

struct anon_page {
    enum vm_type type;
    void *aux;
    bool (*swap_initializer) (struct disk *, enum vm_type, void *kva);
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
