#define SL_IMPLEMENTATION
#include "../../Sl.hpp"

using namespace Sl;

struct CustomAllocator : Allocator
{
    void* allocate(usize size, u16 alignment = sizeof(void*)) override {
        log("[CustomAlloc] allocated %zu bytes!\n", size);
        return malloc(size);
    }
    void* reallocate(void* ptr, usize old_size, usize new_size) override {
        log("[CustomAlloc] reallocated %zu bytes!\n", new_size);
        return malloc(new_size);
    }
    Snapshot* snapshot() override { return nullptr; };
    void rewind(Snapshot* snapshot) override {};
    void reset() override {};
    void cleanup() override {};
    void display_content() override {};
};

int main()
{
    // You are supposed to free resources of global allocator ONLY at the end of you program.
    // (Or you can just leave it and it gonna be deallocated by the Operating system anyway).
    defer(cleanup_global_allocator());

    auto alignment = 8;
    // this will allocate 10 (+6 alignment) bytes from global allocator
    void* ptr = temp_alloc(10, alignment);
    log("Got ptr %p\n", ptr);

    // Make a snapshot of current state of global allocator
    auto* snapshot = temp_begin();
    {
        // do some more allocations...
        temp_alloc(123, 0); // allocate 123 bytes, with no alignment from global allocator
        temp_alloc(20); // allocate 20, with default alignment from global allocator
        temp_alloc(953);
    }
    // Rewind to state as it was before, effectivly restoring memory,
    //  that was allocated in between this two calls
    temp_end(snapshot);

    // Or you can just use ScopedAllocator
    {
        auto* global_alloc_ptr = get_global_allocator();
        ScopedAllocator _(*global_alloc_ptr);
        // do some more allocations...
        temp_alloc(33, 0);
        temp_alloc(45);
        temp_alloc(441);
    }

    // Clear the global allocator (but not free)
    temp_reset();

    //You can also change the current global allocator to the custom one
    CustomAllocator custom_alloc;
    set_global_allocator(custom_alloc);

    // Now it will use your implementation
    temp_alloc(123456);
}