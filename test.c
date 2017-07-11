#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <check.h>

#include "mdbx.h"

void sprint_hex(char *s, void *data, int len);
struct decoded_val *decode_num(char *s);

struct hex_data;

#define decode_val_int(d) *((uint32_t *)d->val.mv_data)

START_TEST(test_print_hex)
{

	static char *test_input = "\x0a\xf1";
	static char *expected = "0af1";
	char result[strlen(test_input) * 2 + 1];

	sprint_hex(result, test_input, 2);
	ck_assert_str_eq(expected, result);

	test_input = "";
	expected = "";
	char result2[1];
	sprint_hex(result2, test_input, 0);
	ck_assert_str_eq(expected, result2);
}
END_TEST

START_TEST(test_decode_hex)
{
}
END_TEST

START_TEST(test_decode_num)
{
	struct decoded_val *val;

	/* Leading whitespace, trailing alpha chars, and trailing whitespace
	 * should be ignored */
	val = decode_num(" 1a ");
	ck_assert(val != NULL);
	ck_assert_int_eq(decode_val_int(val), htonl(1));
	ck_assert_int_eq(val->val.mv_size, sizeof(uint32_t));

	/* non-integers should be 0 */
	val = decode_num("foo");
	ck_assert(val != NULL);
	ck_assert_int_eq(decode_val_int(val), 0);

	/* floats should drop the decimal point */
	val = decode_num("123.1");
	ck_assert(val != NULL);
	ck_assert_int_eq(decode_val_int(val), htonl(123));

	/* negative integers should be 0 */
	val = decode_num("-1");
	ck_assert(val == NULL);

	val = decode_num("123");
	ck_assert(val != NULL);
	ck_assert_int_eq(decode_val_int(val), htonl(123));

	/* UINT32_MAX should be UINT32_MAX */
	val = decode_num("4294967295");
	ck_assert(val != NULL);
	ck_assert_int_eq(decode_val_int(val), htonl(4294967295));

	/* greater than UINT32_MAX should be 0 */
	val = decode_num("4294967296");
	ck_assert(val == NULL);

	/* Leading integers should be ignored */
	val = decode_num("0123");
	ck_assert(val != NULL);
	ck_assert_int_eq(decode_val_int(val), htonl(123));
}
END_TEST

int main(void)
{
	SRunner *sr;
	Suite *s;
	TCase *tc_core;
	int num_failed;

	s = suite_create("mdbx");

	tc_core = tcase_create("all");
	tcase_add_test(tc_core, test_print_hex);
	tcase_add_test(tc_core, test_decode_hex);
	tcase_add_test(tc_core, test_decode_num);
	suite_add_tcase(s, tc_core);

	sr = srunner_create(s);
	srunner_run_all(sr, CK_NORMAL);
	num_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (num_failed == 0) ? 0 : 1;
}
