/*
 * Result codes shared by the pure-logic core.
 */
#ifndef XPTRIM_CORE_RESULT_H
#define XPTRIM_CORE_RESULT_H

typedef enum {
    XT_OK = 0,
    XT_E_INVALID_ARG,  /* bad or inconsistent input */
    XT_E_OUT_OF_RANGE, /* range outside the partition or beyond 48-bit LBA */
    XT_E_OVERFLOW,     /* 64-bit arithmetic would overflow */
    XT_E_OVERLAP,      /* two ranges overlap */
    XT_E_BAD_DATA      /* malformed device data, e.g. IDENTIFY checksum */
} xt_result;

/* Short English description; never NULL. */
const char *xt_result_str(xt_result r);

#endif /* XPTRIM_CORE_RESULT_H */
