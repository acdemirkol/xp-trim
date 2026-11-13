#include "core/result.h"

const char *xt_result_str(xt_result r)
{
    switch (r) {
    case XT_OK:             return "ok";
    case XT_E_INVALID_ARG:  return "invalid argument";
    case XT_E_OUT_OF_RANGE: return "range outside partition or 48-bit LBA limit";
    case XT_E_OVERFLOW:     return "arithmetic overflow";
    case XT_E_OVERLAP:      return "overlapping ranges";
    case XT_E_BAD_DATA:     return "malformed device data";
    }
    return "unknown error";
}
