#define SL_IMPLEMENTATION
#include "../../Sl.hpp"

using namespace Sl;

// There is two types of arrays: Array and LocalArray.
//  The only difference is that LocalArray preallocates some size into local storage,
//   which means he doesn't need to allocate it from Heap/Allocator until the threshold
int main()
{
    // You can swap this two, without affecting final result
        // LocalArray<int> array;
        Array<int> array;

    // Deinitilize content of array at the end of the function
    defer(array.cleanup());

    // Append numbers from 1 to 9 into array
    for (int i = 1; i < 10; ++i)
        array.push(i);

    // Or append using '<<' operator
    array << 10;

    // Print this numbers
    array.forEach([](auto& val){
        log("%d\n", val);
    });
}