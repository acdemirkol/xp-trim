/*
 * Minimal unit test helpers. No external dependencies.
 *
 * Each test is a void function that uses CHECK*; run_tests() in test_main.c
 * calls them in order and the process exit code is the number of failures.
 */
#ifndef XPTRIM_TEST_H
#define XPTRIM_TEST_H

#include <stdio.h>
#include <string.h>

extern int test_failures;
extern int test_checks;

#define CHECK(cond)                                                          \
    do {                                                                     \
        test_checks++;                                                       \
        if (!(cond)) {                                                       \
            test_failures++;                                                 \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, \
                    #cond);                                                  \
        }                                                                    \
    } while (0)

#define CHECK_EQ_U64(actual, expected)                                       \
    do {                                                                     \
        unsigned long long a_ = (unsigned long long)(actual);                \
        unsigned long long e_ = (unsigned long long)(expected);              \
        test_checks++;                                                       \
        if (a_ != e_) {                                                      \
            test_failures++;                                                 \
            fprintf(stderr, "%s:%d: %s == %llu, expected %llu\n", __FILE__,  \
                    __LINE__, #actual, a_, e_);                              \
        }                                                                    \
    } while (0)

#define CHECK_RESULT(expr, expected)                                         \
    do {                                                                     \
        int a_ = (int)(expr);                                                \
        int e_ = (int)(expected);                                            \
        test_checks++;                                                       \
        if (a_ != e_) {                                                      \
            test_failures++;                                                 \
            fprintf(stderr, "%s:%d: %s returned %d, expected %d (%s)\n",     \
                    __FILE__, __LINE__, #expr, a_, e_, #expected);           \
        }                                                                    \
    } while (0)

/* Test suites, one per source file in tests/. */
void suite_geometry(void);
void suite_ranges(void);
void suite_dsm(void);
void suite_identify(void);

#endif /* XPTRIM_TEST_H */
