#include "test-framework.h"

/* Entry point shared by all test binaries. Individual test files register
 * their tests via TEST(); this translation unit provides main(). */
int main()
{
	return test_framework::run_all();
}
