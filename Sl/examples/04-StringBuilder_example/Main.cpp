#define SL_IMPLEMENTATION
#include "../../Sl.hpp"

using namespace Sl;

void print_and_clear(StrBuilder& builder) {
    // Append null at the end, so that we can print it as regular null-terminated C-string
    builder.append_null();

    // Print content
    log("%s", builder.data());
    // Or if data is not null terminated:
    // log(SB_FORMAT "\n", SB_ARG(builder));

    // Clear inner buffer, so that we can reuse StringBuilder later
    builder.clear();
}

int main()
{
    StrBuilder builder;
    // Deinitilize content of StringBuilder at the end of the function
    defer(builder.cleanup());

    // You can 'chain' append values into builder
    builder << "Number:" << ' ' << 123.123 << '\n';
    print_and_clear(builder);

    // Or you can use append functions
    builder.append("Test append:").appendf(" %d", 321).append('\n');
    print_and_clear(builder);
}