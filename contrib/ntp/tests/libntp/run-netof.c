/* AUTOGENERATED FILE. DO NOT EDIT. */

//=======Test Runner Used To Run Each Test Below=====
#define RUN_TEST(TestFunc, TestLineNum) \
{ \
  Unity.CurrentTestName = #TestFunc; \
  Unity.CurrentTestLineNumber = TestLineNum; \
  Unity.NumberOfTests++; \
  if (TEST_PROTECT()) \
  { \
      setUp(); \
      TestFunc(); \
  } \
  if (TEST_PROTECT() && !TEST_IS_IGNORED) \
  { \
    tearDown(); \
  } \
  UnityConcludeTest(); \
}

//=======Automagically Detected Files To Include=====
#include "unity.h"
#include <setjmp.h>
#include <stdio.h>

//=======External Functions This Runner Calls=====
extern void setUp(void);
extern void tearDown(void);
void resetTest(void);
extern void test_ClassBAddress(void);
extern void test_ClassCAddress(void);
extern void test_ClassAAddress(void);
extern void test_IPv6Address(void);


//=======Test Reset Option=====
void resetTest()
{
  tearDown();
  setUp();
}

char *progname;


//=======MAIN=====
int main(int argc, char *argv[])
{
  progname = argv[0];
  Unity.TestFile = "netof.c";
  UnityBegin("netof.c");
  RUN_TEST(test_ClassBAddress, 10);
  RUN_TEST(test_ClassCAddress, 20);
  RUN_TEST(test_ClassAAddress, 30);
  RUN_TEST(test_IPv6Address, 43);

  return (UnityEnd());
}
