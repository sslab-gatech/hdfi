#include <stdio.h>
#include <stdlib.h>

int creat(const char *path, mode_t mode) {
    printf("creat was called\n");
    exit(0);
}

class Good {
    public:
    virtual void print() {
        printf("Good was called\n");
    }
};

class Better : public Good {
    public:
    virtual void print() {
        printf("Better was called\n");
    }
};

int main() {
    void* fake_vtable[] = {(void *)creat};
    Good *good = new Better;

    good->print();

    *reinterpret_cast<void**>(good) = (void *)fake_vtable;

    good->print();

    return 0;
}
