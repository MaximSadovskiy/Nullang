#ifndef SL_RELATIVEPTR_H
#define SL_RELATIVEPTR_H

#include "Sl_Defines.hpp"

// Example:
//    auto relPtr  = REL_PTR(int, arr, 123);
//    auto origPtr = relPtr[arr]; // returns 123
#define REL_PTR(RelativeT, base, ptr) Sl::RelPtr<decltype(base), decltype(ptr), RelativeT>((base), (ptr));

namespace Sl
{
    // RelativeT must be signed numeric type, since you want to know offset from left and right
	template <typename BasePtrT, typename PtrT, typename RelativeT = int32_t>
	struct RelPtr
	{
		RelativeT _relative_index = 0;
		RelPtr(const BasePtrT base_ptr, const PtrT ptr)
		{
    		const ptrdiff_t offset = ((ptrdiff_t)ptr) - ((ptrdiff_t)base_ptr);

            const auto bits = sizeof(RelativeT) * 8 - 1;
           	const s64 max_offset = ((1ll << (bits - 1)) - 1) * 2;
    		const s64 min_offset = ((1LL << bits));
    		ASSERT(offset <= max_offset && offset >= min_offset,
    		    "Offset exceded max size of relative_index type, change RelativeT to a bigger size");

            _relative_index = offset;
		}

		PtrT to_normal(const BasePtrT base_ptr) noexcept
		{
			return (PtrT) ((ptrdiff_t) base_ptr + (ptrdiff_t)_relative_index);
		}

		PtrT operator[](const BasePtrT base_ptr) noexcept
		{
    		return to_normal(base_ptr);
		}
	};
} // namespace Sl

#endif // SL_RELATIVEPTR_H
