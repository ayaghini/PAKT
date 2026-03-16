// PttController.cpp – FW-016: settable PTT safe-off hook

#include "pakt/PttController.h"

namespace pakt {

static std::function<void()> s_safe_off_fn;

void ptt_register_safe_off(std::function<void()> fn)
{
    s_safe_off_fn = std::move(fn);
}

void ptt_safe_off()
{
    if (s_safe_off_fn) s_safe_off_fn();
    // If not registered: hardware PTT is off by default (SA818 PTT pin is
    // active-low; released = off).  The watchdog's own log message documents
    // that the trigger fired.  No additional action is required here.
}

bool ptt_is_registered()
{
    return !!s_safe_off_fn;
}

} // namespace pakt
