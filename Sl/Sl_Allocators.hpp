#ifndef SL_ALLOCATORS_H
#define SL_ALLOCATORS_H

#include "Sl_Defines.hpp"

namespace Sl
{
    // If you want to use snapshot()/rewind() feature:
    //  you must create your custom snapshot and extend from this class
    struct Snapshot
    {
        virtual ~Snapshot() = default;
    };

    // If you want to use custom allocator, you must extend from this base class
    //  or you can use already implemented ones below
    struct Allocator
    {
        virtual ~Allocator() = default;

        virtual void* allocate(usize size, u16 alignment = sizeof(void*)) = 0;
        virtual void* reallocate(void* ptr, usize old_size, usize new_size) = 0;
        virtual Snapshot* snapshot() = 0;
        virtual void rewind(Snapshot* snapshot) = 0;
        virtual void reset() = 0;
        virtual void cleanup() = 0;
        virtual void display_content() = 0;
    };
}
#include "Sl_Array.hpp"
namespace Sl
{
    Allocator* get_global_allocator() noexcept;
    void set_global_allocator(Allocator& allocator) noexcept;
    // Deallocates global allocator. You don't need to call it before calling set_global_allocator(), it will be done for you
    void cleanup_global_allocator() noexcept;
    // Allocates some memory from the global allocator. 0 alignment equals no alignment
    void* temp_alloc(usize size, u16 alignment = sizeof(void*)) noexcept;
    // Make a "snapshot" of current state of the global allocator,
    //  in order to restore it later (basically deallocates used resources) by calling temp_end(snapshot);
    Snapshot* temp_begin() noexcept;
    // Restores state of the global allocator of when temp_begin() was called
    //  (!!! This doesnt actually free's any resources)
    void temp_end(Snapshot* snapshot) noexcept;
    // Resets (but not free's!!) global allocator
    //  Only recomended if you know that you not use resources allocated by global allocator.
    //  Some ezbuild.h functions uses global_allocator, for example to strdup file names.
    // Better do:
    //   Snapshot* snapshot = temp_begin();
    //   {
    //     // do some temporary allocations...
    //   }
    //   temp_end(snapshot);
    //
    // Or:
    //   {
    //      ScopedAllocator temp; // will call temp_end() at the end of the scope automatically
    //      // do some temporary allocations...
    //   }
    void temp_reset() noexcept;

    // Saves state of the local/global allocator in the Constructor and restores state at the Destructor.
    struct ScopedAllocator
    {
        Allocator* _allocator_ref;
        Snapshot* _snapshot;

        ScopedAllocator(Allocator& allocator)
        {
            _allocator_ref = &allocator;
            _snapshot = _allocator_ref->snapshot();
        }
        ScopedAllocator()
        {
            _allocator_ref = get_global_allocator();
            _snapshot = _allocator_ref->snapshot();
        }
        ~ScopedAllocator()
        {
            _allocator_ref->rewind(_snapshot);
        }
    };

    struct ArenaSnapshot : Snapshot
    {
        usize region_index = 0;
        usize index = 0;
    };

    struct ArenaRegion
    {
        char* data = nullptr;
        usize cursor = 0;
        usize capacity = 0;
    };

    struct ArenaAllocator : Allocator
    {
    private:
        Array<ArenaRegion> _regions;
        usize _current_region_index;
        usize _region_size;
    public:
        ArenaAllocator(usize default_region_size = SL_ALLOCATOR_INIT_SIZE)
        {
            _region_size = default_region_size;
            _current_region_index = 0;
        }

        inline Array<ArenaRegion>& regions() { return _regions; }
        inline usize current_region_index() { return _current_region_index; }
        inline usize region_size() { return _region_size; }

        void* allocate(usize size, u16 alignment = sizeof(void*)) override;
        void* reallocate(void* ptr, usize old_size, usize new_size) override;
        Snapshot* snapshot() override;
        void rewind(Snapshot* snapshot) override;
        void reset() override;
        void cleanup() override;
        void display_content() override;
    };

    struct LinearSnapshot : Snapshot
    {
        usize index = 0;
    };
    struct LinearAllocator : Allocator
    {
        char* data;
        usize cursor;
        usize total_size;

        LinearAllocator(usize total_size = SL_ALLOCATOR_INIT_SIZE)
        {
            this->data = nullptr;
            this->cursor = 0;
            this->total_size = total_size;
        }

        void* allocate(usize size, u16 alignment = sizeof(void*)) override;
        void* reallocate(void* ptr, usize old_size, usize new_size) override;
        Snapshot* snapshot() override;
        void rewind(Snapshot* snapshot) override;
        void reset() override;
        void cleanup() override;
        void display_content() override;
    };

    struct StackSnapshot : Snapshot
    {
        void* current = nullptr;
    };
    struct StackAllocator : Allocator
    {
        void* begin;
        void* end;
        void* current;
        usize total_size;

        StackAllocator(usize total_size = SL_ALLOCATOR_INIT_SIZE)
        {
            this->begin = nullptr;
            this->current = nullptr;
            this->end = nullptr;
            this->total_size = total_size;
        }
        void* allocate(usize size, u16 alignment = sizeof(void*)) override;
        void* reallocate(void* ptr, usize old_size, usize new_size) override;
        Snapshot* snapshot() override;
        void rewind(Snapshot* snapshot) override;
        void reset() override;
        void cleanup() override;
        void display_content() override;
    };

    typedef struct PoolChunk {
        struct PoolChunk* next;
    } PoolChunk;
    struct PoolSnapshot : Snapshot
    {
        PoolChunk* current_chunk = nullptr;
    };
    struct PoolAllocator : Allocator
    {
        PoolChunk* root;
        PoolChunk* pool;
        usize chunk_count;
        usize chunk_size;

        PoolAllocator(usize chunk_count = 32, usize chunk_size = SL_ALLOCATOR_INIT_SIZE / 32)
        {
            this->root = nullptr;
            this->pool = nullptr;
            this->chunk_count = chunk_count;
            this->chunk_size = chunk_size;
        }

        void* allocate(usize size, u16 alignment = sizeof(void*)) override; // Alignment is ignore in this one
        void* reallocate(void* ptr, usize old_size, usize new_size) override;
        Snapshot* snapshot() override;
        void rewind(Snapshot* snapshot) override;
        void reset() override;
        void cleanup() override;
        void display_content() override;
    };
} // namespace Sl
#endif // !SL_ALLOCATORS_H

#ifdef SL_IMPLEMENTATION
#include <new>
namespace Sl
{
    static SL_THREAD_LOCAL Allocator* _global_alloc = nullptr;

    Allocator* get_global_allocator() noexcept
    {
        if (_global_alloc == nullptr) {
            static SL_THREAD_LOCAL ArenaAllocator _default_global_alloc(SL_GLOBAL_ALLOCATOR_INIT_SIZE);
            _global_alloc = &_default_global_alloc;
        }
        return _global_alloc;
    }

    void set_global_allocator(Allocator& new_allocator) noexcept
    {
        get_global_allocator()->cleanup();
        _global_alloc = &new_allocator;
    }

    void cleanup_global_allocator() noexcept
    {
        get_global_allocator()->cleanup();
    }

    void* temp_alloc(usize size, u16 alignment) noexcept
    {
        return get_global_allocator()->allocate(size, alignment);
    }

    Snapshot* temp_begin() noexcept
    {
        return get_global_allocator()->snapshot();
    }

    void temp_end(Snapshot* snapshot) noexcept
    {
        get_global_allocator()->rewind(snapshot);
    }

    void temp_reset() noexcept
    {
        get_global_allocator()->reset();
    }

    void memory_copy(void* dest, usize dest_size, const void* src, usize src_size) noexcept;

    void log(const char* const format, ...);

    void* LinearAllocator::allocate(usize size, u16 alignment)
    {
        size = ALIGNMENT(size, alignment);
        if (data == nullptr) {
            if (total_size == 0) total_size = SL_GLOBAL_ALLOCATOR_INIT_SIZE;
            const auto allocated_size = MAX(total_size, size);
            this->total_size = allocated_size;
            this->data = (char*)SL_ALLOCATOR_MALLOC(allocated_size);
            ASSERT_DEBUG(data != nullptr);
        }
        ASSERT(cursor <= total_size, "Cursor went past total size (possible memory corruption)");

        if (cursor + size > total_size)
            return (void*)NULL;

        void* new_ptr = data + cursor;
        cursor += size;
        return new_ptr;
    }

    void* LinearAllocator::reallocate(void* ptr, usize old_size, usize new_size)
    {
        void* new_ptr = allocate(new_size);
        if (ptr == nullptr) return new_ptr;

        memory_copy(new_ptr, new_size, ptr, old_size);
        return new_ptr;
    }

    void LinearAllocator::reset()
    {
        cursor = 0;
    }

    void LinearAllocator::cleanup()
    {
        SL_ALLOCATOR_FREE(data);
        data = nullptr;
        cursor = 0;
        total_size = 0;
    }

    void LinearAllocator::rewind(Snapshot* _snapshot)
    {
        if (_snapshot == nullptr) return;
        LinearSnapshot* snapshot = nullptr;
        // try {
            snapshot = dynamic_cast<LinearSnapshot*>(_snapshot);
        // } catch(...) {}
        if (snapshot == nullptr) {
            ASSERT(snapshot != nullptr, "Failed to cast snapshot, when trying to rewind");
            return;
        }
        this->cursor = snapshot->index;
    }

    Snapshot* LinearAllocator::snapshot()
    {
        auto current_cursor = cursor;
        auto* snapshot = (LinearSnapshot*) temp_alloc(sizeof(LinearSnapshot));
        ASSERT_NOT_NULL(snapshot);
        ::new (snapshot) LinearSnapshot; // Initilize RTTI for virtual methods

        snapshot->index = current_cursor;
        return snapshot;
    }

    void LinearAllocator::display_content()
    {
        log("|---------------------\n");
        log("|Linear allocator:\n");
        log("|---------------------\n");
        log("|  capacity: %zu\n", total_size);
        log("|  cursor: %zu\n", cursor);
        log("|---------------------\n");
    }

    void* StackAllocator::allocate(usize size, u16 alignment)
    {
        size = ALIGNMENT(size, alignment);
        if (begin == nullptr) {
            if (total_size < 1) total_size = SL_GLOBAL_ALLOCATOR_INIT_SIZE;
            begin = (char*)SL_ALLOCATOR_MALLOC(total_size);
            this->current = this->begin;
            this->end = ((char*)this->begin) + total_size;
        }

        if ((char*)current + size > end)
            return nullptr;

        void* ptr = (void*)current;
        current = (char*)current + size;
        return ptr;
    }

    void* StackAllocator::reallocate(void* ptr, usize old_size, usize new_size)
    {
        void* new_ptr = allocate(new_size);
        if (ptr == nullptr) return new_ptr;

        memory_copy(new_ptr, new_size, ptr, old_size);
        return new_ptr;
    }

    Snapshot* StackAllocator::snapshot()
    {
        if (this->begin == nullptr || this->end == nullptr)  // initilize memory if empty
            allocate(0, 0);

        auto* current_ptr = this->current;
        auto* snapshot = (StackSnapshot*) temp_alloc(sizeof(StackSnapshot));
        ASSERT_NOT_NULL(snapshot);
        ::new (snapshot) StackSnapshot; // Initilize RTTI for virtual methods

        if (current_ptr == nullptr) current_ptr = this->begin;
        snapshot->current = current_ptr;
        return snapshot;
    }

    void StackAllocator::rewind(Snapshot* _snapshot)
    {
        if (_snapshot == nullptr) return;
        StackSnapshot* snapshot = nullptr;
        // try {
            snapshot = dynamic_cast<StackSnapshot*>(_snapshot);
        // } catch(...) {}
        if (snapshot == nullptr) {
            ASSERT(snapshot != nullptr, "Failed to cast snapshot, when trying to rewind");
            return;
        }

        ASSERT_TRUE(snapshot->current >= (void*)this->begin);
        ASSERT_TRUE(snapshot->current <= (void*)this->end);
        this->current = snapshot->current;
    }

    void StackAllocator::reset()
    {
        current = begin;
    }

    void StackAllocator::cleanup()
    {
        SL_ALLOCATOR_FREE(begin);
        begin = nullptr;
        end = nullptr;
        current = nullptr;
    }

    void StackAllocator::display_content()
    {
        log("|---------------------\n");
        log("|Stack allocator:\n");
        log("|---------------------\n");
        log("|  current: %p\n", current);
        log("|  begin: %p\n", begin);
        log("|  end: %p\n", end);
        log("|---------------------\n");
    }

    void* PoolAllocator::allocate(usize size, u16 alignment)
    {
        UNUSED(alignment);
        if (this->root == nullptr) {
            const usize total_size = (usize)chunk_count * (usize)chunk_size;

            if(this->chunk_count < 1) this->chunk_count = 32;
            if(this->chunk_size < 1) this->chunk_size = SL_GLOBAL_ALLOCATOR_INIT_SIZE / 32;
            this->pool = nullptr;
            this->root = (PoolChunk*)SL_ALLOCATOR_MALLOC(total_size);
            ASSERT_DEBUG(this->root != nullptr);

            reset();
        }
        ASSERT(chunk_size >= size, "Requested size bigger than chunk size of Pool");
        ASSERT(chunk_count > 0, "Chunk count must be bigger than 0");
        ASSERT(chunk_size > 0, "Chunk size must be bigger than 0");

        if (pool == NULL
            || size > chunk_size
            || chunk_count == 0
            || chunk_size == 0
        )
            return (void*)NULL;

        PoolChunk* ptr = pool;
        pool = ptr->next;
        return ptr;
    }

    void* PoolAllocator::reallocate(void* ptr, usize old_size, usize new_size)
    {
        void* new_ptr = allocate(new_size);
        if (ptr == nullptr) return new_ptr;

        memory_copy(new_ptr, new_size, ptr, old_size);
        return new_ptr;
    }

    Snapshot* PoolAllocator::snapshot()
    {
        auto* current_chunk = this->pool;
        auto* snapshot = (PoolSnapshot*) temp_alloc(sizeof(PoolSnapshot));
        ASSERT_NOT_NULL(snapshot);
        ::new (snapshot) PoolSnapshot; // Initilize RTTI for virtual methods
        if (current_chunk == nullptr) {
            if (this->root == nullptr) allocate(0, 0); // initilize memory
            current_chunk = this->root;
        }

        snapshot->current_chunk = current_chunk;
        return snapshot;
    }

    void PoolAllocator::rewind(Snapshot* _snapshot)
    {
        if (_snapshot == nullptr) return;

        PoolSnapshot* snapshot = nullptr;
        // try {
            snapshot = dynamic_cast<PoolSnapshot*>(_snapshot);
        // } catch(...) {}
        if (snapshot == nullptr) {
            ASSERT(snapshot != nullptr, "Failed to cast snapshot, when trying to rewind");
            return;
        }
        this->pool = snapshot->current_chunk;
    }

    void PoolAllocator::reset()
    {
        const usize total_size = chunk_count * chunk_size;
        ASSERT(chunk_count > 0, "Chunk count must be bigger than 0");
        ASSERT(chunk_size > 0, "Chunk size must be bigger than 0");

        pool = root;
        PoolChunk* current_block = pool;

        for (usize i = chunk_size; i < total_size; i += chunk_size)
        {
            current_block->next = (PoolChunk*)((char*)root + i);
            current_block = current_block->next;
        }
        current_block->next = NULL;
    }

    void PoolAllocator::cleanup()
    {
        SL_ALLOCATOR_FREE(root);
        root = nullptr;
        pool = nullptr;
    }

    void PoolAllocator::display_content()
    {
        // @TODO not really useful, must traverse all chunks
        log("|---------------------\n");
        log("|Pool allocator:\n");
        log("|---------------------\n");
        log("|  chunk_count: %zu\n", chunk_count);
        log("|  chunk_size: %zu\n", chunk_size);
        log("|  current_chunk: %p\n", pool);
        log("|  root_chunk: %p\n", root);
        log("|---------------------\n");
    }

    void* ArenaAllocator::allocate(usize size, u16 alignment)
    {
        const usize aligned_size = ALIGNMENT(size, alignment);

        ArenaRegion* region = NULL;
        bool found_region = false;
        for (usize i = this->_current_region_index; i < _regions.count(); ++i) {
            region = &_regions.get(i);
            if (region->cursor + aligned_size <= region->capacity || region->data == NULL) {
                found_region = true;
                this->_current_region_index = i;
                break;
            }
        }
        if (!found_region) {
            _regions.push(ArenaRegion{});
            region = &_regions.last();
            this->_current_region_index = _regions.count() - 1;
        }

        if (region->data == NULL) {
            const usize allocated_size = MAX(aligned_size, _region_size);
            region->cursor = 0;
            region->capacity = allocated_size;
            region->data = (char*)SL_ALLOCATOR_MALLOC(allocated_size);
            ASSERT_DEBUG(region->data != nullptr);
        }
        void* new_ptr = (void*)(region->data + region->cursor);
        region->cursor += aligned_size;
        return new_ptr;
    }

    void* ArenaAllocator::reallocate(void* ptr, usize old_size, usize new_size)
    {
        void* new_ptr = allocate(new_size);
        if (ptr == nullptr) return new_ptr;

        memory_copy(new_ptr, new_size, ptr, old_size);
        return new_ptr;
    }

    void ArenaAllocator::reset()
    {
        for (auto& region : _regions) {
            region.cursor = 0;
        }
        _current_region_index = 0;
    }

    void ArenaAllocator::cleanup()
    {
        for (auto& region : _regions) {
            SL_ALLOCATOR_FREE(region.data);
        }
        _regions.cleanup();
        _current_region_index = 0;
    }

    void ArenaAllocator::rewind(Snapshot* _snapshot)
    {
        if (_snapshot == nullptr) return;
        ArenaSnapshot* snapshot = nullptr;
        // try {
            snapshot = dynamic_cast<ArenaSnapshot*>(_snapshot);
        // } catch(...) {}
        if (snapshot == nullptr) {
            ASSERT(snapshot != nullptr, "Failed to cast snapshot, when trying to rewind");
            return;
        }

        if (snapshot->region_index < _regions.count()) {
            auto& region = _regions[snapshot->region_index];
            region.cursor = snapshot->index;
            _current_region_index = snapshot->region_index;
            for (usize i = snapshot->region_index + 1; i < _regions.count(); ++i)
                _regions[i].cursor = 0;
        }
    }

    Snapshot* ArenaAllocator::snapshot()
    {
        if (_regions.count() < 1) allocate(0, 0); // initilize regions if empty

        const auto current_index = _regions[_current_region_index].cursor;
        ArenaSnapshot* snapshot = (ArenaSnapshot*) temp_alloc(sizeof(ArenaSnapshot), alignof(ArenaSnapshot));
        ASSERT_NOT_NULL(snapshot);
        ::new (snapshot) ArenaSnapshot; // Initilize RTTI for virtual methods
        snapshot->region_index = _current_region_index;
        snapshot->index = current_index;
        return snapshot;
    }

    void ArenaAllocator::display_content()
    {
        u32 iter = 0;
        log("|---------------------\n");
        log("|Arena allocator:\n");
        log("|---------------------\n");
        for (auto& region : _regions) {
            log("|-region %d", iter++);
            if (iter - 1 == _current_region_index) log(" <--");
            log("\n");
            log("|  capacity: %zu\n", region.capacity);
            log("|  cursor: %zu\n", region.cursor);
            log("|---------------------\n");
        }
    }
} // namespace Sl

#endif // !SL_IMPLEMENTATION
