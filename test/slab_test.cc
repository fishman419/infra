#include "slab.h"

int main() {
    infra::Slab<uint32_t> slab(1024);
    slab.init();
    uint32_t *a = slab.get();
    std::cout << "a: " << a << std::endl;
    slab.put(a);
    return 0;
}
