#ifndef DEFER_H
#define DEFER_H

#include <functional>

class Defer__ {
public:
    using FuncPtr = std::function<void()>;

    Defer__(FuncPtr func) : func_(func) {}
    ~Defer__() noexcept { func_(); }
    Defer__& operator=(const Defer__&) = delete;
    Defer__(const Defer__&) = delete;
private:
    FuncPtr func_;
};

#ifndef DEFER_CONCAT
#  define DEFER_CONCAT(a, b) a ## b
#endif // !DEFER_CONCAT
#ifndef DEFER_CONCAT_EXPAND
#  define DEFER_CONCAT_EXPAND(a, b) DEFER_CONCAT(a,b)
#endif // !DEFER_CONCAT_EXPAND
#define defer(code) Defer__ DEFER_CONCAT_EXPAND(defer,__COUNTER__)([&](){code ;});

#endif // DEFER_H
