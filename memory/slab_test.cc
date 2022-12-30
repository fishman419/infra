#include "slab.h"

int main() {
    infra::Slab<uint32_t> slab(1024);
    slab.Init();
    uint32_t *a = slab.Get();
    std::cout << "a: " << a << std::endl;
    slab.Put(a);
    return 0;
}