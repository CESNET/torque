#include "license_pbs.h"
#include "lib_ifl.h"
#include "test_enc_Track.h"
#include <stdlib.h>
#include <stdio.h>
#include <check.h>

#include "pbs_error.h"
#include "scaffolding.h"
START_TEST(test_one)
  {
  int rc = 0;
  return rc;
  }
END_TEST

START_TEST(test_two)
  {
  int rc = 0;
  return rc;
  }
END_TEST

Suite *enc_Track_suite(void)
  {
  Suite *s = suite_create("enc_Track_suite methods");
  TCase *tc_core = tcase_create("test_one");
  tcase_add_test(tc_core, test_one);
  suite_add_tcase(s, tc_core);

  tc_core = tcase_create("test_two");
  tcase_add_test(tc_core, test_two);
  suite_add_tcase(s, tc_core);

  return s;
  }

void rundebug()
  {
  }

int main(void)
  {
  int number_failed = 0;
  rundebug();
  SRunner *sr = srunner_create(enc_Track_suite());
  srunner_set_log(sr, "enc_Track_suite.log");
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return number_failed;
  }