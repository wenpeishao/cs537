#include "types.h"
#include "stat.h"
#include "user.h"

int main(void)
{
    char buf[512];
    getlastcat(buf);
    printf(1, "XV6_TEST_OUTPUT Last catted filename: %s\n", buf);
    exit();
}
