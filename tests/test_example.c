#include <cmocka.h>
// #include "your_code.h"  // The header of your C code to be tested

// Test function
static void test_addition(void **state) {
    (void) state;  // unused variable

    // int result = add(2, 3);  // Assume you have a function `add`
    assert_int_equal(5, 5);  // Check that the result is 5
}

// Setup function
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_addition),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
