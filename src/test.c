#include <stdio.h>
#include "reader.test.h"
#include "printer.test.h"
#include "lizp.test.h"
#include "value.test.h"
#include "eval.test.h"

void Test(void)
{
    printf("  ValueTest\n");
    ValueTest();
    printf("  ReaderTest\n");
    ReaderTest();
    printf("  PrinterTest\n");
    PrinterTest();
    printf("  EvalTest\n");
    EvalTest();
    printf("  LizpTest\n");
    LizpTest();
}

int main(void)
{
    printf("Testing...\n");
    Test();
    printf("Testing is successful.\n");
    return 0;
}

