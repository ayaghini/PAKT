// Sa818ResponseParser.cpp
#include "pakt/Sa818ResponseParser.h"
#include <cstring>

namespace pakt {

Sa818ResponseParser::Result
Sa818ResponseParser::parse_dmo_response(const char *resp, const char *prefix)
{
    if (!resp) return Result::Unknown;
    const size_t plen = strlen(prefix);
    if (strncmp(resp, prefix, plen) != 0) return Result::Unknown;
    // Character immediately after prefix is the status digit
    const char status = resp[plen];
    if (status == '0') return Result::Ok;
    if (status >= '1' && status <= '9') return Result::Error;
    return Result::Unknown;
}

Sa818ResponseParser::Result
Sa818ResponseParser::parse_connect(const char *resp)
{
    return parse_dmo_response(resp, "+DMOCONNECT:");
}

Sa818ResponseParser::Result
Sa818ResponseParser::parse_set_group(const char *resp)
{
    return parse_dmo_response(resp, "+DMOSETGROUP:");
}

} // namespace pakt
