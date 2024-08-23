#include "unity.h"
#include <stdbool.h>
#include <stdlib.h>
#include "../../examples/autotest-validate/autotest-validate.h"
#include "../../assignment-autotest/test/assignment1/username-from-conf-file.h"

/**
<<<<<<< HEAD
 * This function should:
 *   1) Call the my_username() function in autotest-validate.c to get your hard-coded username.
 *   2) Obtain the value returned from function malloc_username_from_conf_file() in username-from-conf-file.h within
 *       the assignment autotest submodule at assignment-autotest/test/assignment1/
 *   3) Use unity assertion TEST_ASSERT_EQUAL_STRING_MESSAGE to verify the two strings are equal.  See
 *       the [unity assertion reference](https://github.com/ThrowTheSwitch/Unity/blob/master/docs/UnityAssertionsReference.md)
 */
=======
* This function should:
*   1) Call the my_username() function in Test_assignment_validate.c to get your hard coded username.
*   2) Obtain the value returned from function malloc_username_from_conf_file() in username-from-conf-file.h within
*       the assignment autotest submodule at assignment-autotest/test/assignment1/
*   3) Use unity assertion TEST_ASSERT_EQUAL_STRING_MESSAGE the two strings are equal.  See
*       the [unity assertion reference](https://github.com/ThrowTheSwitch/Unity/blob/master/docs/UnityAssertionsReference.md)
*/
>>>>>>> assignments-base/assignment2
void test_validate_my_username()
{
    // Call the my_username() function to get the hardcoded username
    const char* hardcoded_username = my_username();

    // Obtain the value returned from malloc_username_from_conf_file() function
    char* config_username = malloc_username_from_conf_file();

    // Use unity assertion to verify that the two strings are equal
    TEST_ASSERT_EQUAL_STRING_MESSAGE(hardcoded_username, config_username, "Usernames do not match!");

    // Free the memory allocated by malloc_username_from_conf_file
    free(config_username);
}

