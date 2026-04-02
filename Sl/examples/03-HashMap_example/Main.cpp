#define SL_IMPLEMENTATION
#include "../../Sl.hpp"

using namespace Sl;

// Calls hasher function of hash map with custom data
//u64 StrView::hash(usize seed, const StrView& key, Hasher_fn callback)
//{
//    ASSERT(callback, "Hasher function could not be null");
//    return callback(seed, key.data, key.size);
//}

int main()
{
    HashMap<int, const char*> map;
    // Deinitilize content of HashMap at the end of the function
    defer(map.cleanup());

    // Insert some values
    map.insert(1, "11");
    map.insert(2, "22");
    map.insert(3, "33");

    // Print this values (Order is not guaranteed).
    map.forEach([](auto& key, auto& value){
        log("Key: %d, Value: '%s'\n", key, value);
    });

    // Remove second key "22", cause why not
    map.remove(2);

    log("----------------\n");
    // HashMap also support hashing for custom types
    // This map would not just raw hash StrView struct,
    //  but will use data pointer instead, effectivly hashing string itself
    //
    // You can see implementation of StrView::hash at the top.
    HashMap<StrView, int, StrView::hash> map2;
    map2.insert("Test", 12345);

    log("Got key: %d\n", *map2["Test"]);
}