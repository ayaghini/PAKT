#pragma once
// Sa818ResponseParser – AT response classifier for the SA818-V module
//
// Pure C++, no ESP-IDF dependencies, host-testable.
//
// SA818 response format:
//   +DMOCONNECT:0\r\n   → Ok (0 = success)
//   +DMOCONNECT:1\r\n   → Error (non-zero status code)
//   +DMOSETGROUP:0\r\n  → Ok
//   +DMOSETGROUP:1\r\n  → Error
//   anything else       → Unknown (unexpected / truncated / timeout)

namespace pakt {

class Sa818ResponseParser
{
public:
    enum class Result { Ok, Error, Unknown };

    // "+DMOCONNECT:0[...]" → Ok
    // "+DMOCONNECT:<non-zero>[...]" → Error
    // other → Unknown
    static Result parse_connect(const char *resp);

    // "+DMOSETGROUP:0[...]" → Ok
    // "+DMOSETGROUP:<non-zero>[...]" → Error
    // other → Unknown
    static Result parse_set_group(const char *resp);

private:
    static Result parse_dmo_response(const char *resp, const char *prefix);
};

} // namespace pakt
