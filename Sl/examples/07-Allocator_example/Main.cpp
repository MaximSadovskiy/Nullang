#define SL_IMPLEMENTATION
#include "../../Sl.hpp"

using namespace Sl;

// Alocator is used in many parts of this library
// There are 4 implemented ones:
//   LinearAllocator
//   StackAllocator
//   PoolAllocator
//   ArenaAllocator
// Arena is only one that is endless

int main()
{
    ArenaAllocator a;
    // Free all the resources at the end of the function
    defer(a.cleanup());

    auto alignment = 8;
    void* ptr = a.allocate(10, alignment); // this will allocate 10 (+6 alignment) bytes
    log("Got ptr %p\n", ptr);

    log("\nBefore:\n");
    a.display_content();
    // Make a snapshot of current state of allocator
    auto* snapshot = a.snapshot();
    {
        // do some more allocations...
        a.allocate(123, 0); // allocate 123 bytes, with no alignment
        a.allocate(20); // allocate 20, with default alignment
        a.allocate(953);

        log("\nAllocated:\n");
        a.display_content();
    }
    // Rewind to state as it was before, effectivly restoring memory,
    //  that was allocated in between this two calls
    a.rewind(snapshot);

    log("\nAfter:\n");
    a.display_content();

    // Or you can just use ScopedAllocator
    {
        ScopedAllocator _(a);
        // do some more allocations...
        a.allocate(33, 0);
        a.allocate(45);
        a.allocate(441);
    }

    // Clear the allocator (but not free)
    a.reset();
    log("\nEnd:\n");
    a.display_content();
}