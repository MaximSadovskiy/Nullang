#define SL_IMPLEMENTATION
#include "../../Sl.hpp"

using namespace Sl;

const char* to_bool(bool val) {
    return val == true ? "true" : "false";
}

int main()
{
    StrView str = "    Hello 123 World   ";

    // Trim spaces from both ends of string
    str.trim();

    // Found out if starts with certain word
    log("Starts with 'Hello': %s\n", to_bool(str.starts_with("Hello")));  // true

    // Found out if ends with certain word
    log("Ends with 'World': %s\n", to_bool(str.ends_with("World")));      // true
    log("Ends with 'abc12': %s\n", to_bool(str.ends_with("abc12")));      // false

    // Found out if contais certain word
    log("Contains '123': %s\n", to_bool(str.contains("123"))); // true

    // Found out index of first occurence of certain word
    log("Found '23' at index %zu\n", str.find_first_occurrence("23"));

    // Chop 'Hello' from left side
    str.chop_left(6);
    // Split by '123' and 'World' and chop the first one
    str.chop_left_by_delimeter(" ");

    // Print final StringView
    log("Final string: '");
      log(SV_FORMAT "'\n", SV_ARG(str));
}