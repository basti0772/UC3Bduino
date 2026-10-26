/* SPDX-License-Identifier: MIT
 * Optional debugger smoke test; no GPIO assumptions or library calls.
 * These volatile objects intentionally exercise nonempty .data and .bss.
 */
static volatile unsigned char initialized_bytes[7] = { 3, 5, 7, 11, 13, 17, 19 };
static volatile unsigned char zero_bytes[7];

enum startup_result {
    STARTUP_NOT_CHECKED = 0,
    STARTUP_PASSED = 1,
    STARTUP_FAILED = 2
};
volatile enum startup_result startup_check_status;

int main(void)
{
    unsigned int index;
    int passed = (startup_check_status == STARTUP_NOT_CHECKED);

    passed = passed && initialized_bytes[0] == 3;
    passed = passed && initialized_bytes[1] == 5;
    passed = passed && initialized_bytes[2] == 7;
    passed = passed && initialized_bytes[3] == 11;
    passed = passed && initialized_bytes[4] == 13;
    passed = passed && initialized_bytes[5] == 17;
    passed = passed && initialized_bytes[6] == 19;
    for (index = 0; index < sizeof(zero_bytes); ++index) {
        passed = passed && zero_bytes[index] == 0;
    }
    startup_check_status = passed ? STARTUP_PASSED : STARTUP_FAILED;
    for (;;) {
        /* Inspect startup_check_status in a debugger: 1 = pass, 2 = fail. */
    }
}
