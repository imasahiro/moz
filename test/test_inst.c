#include <stdio.h>
#define MOZVM_OPCODE_SIZE 1
#include "instruction.h"
#include "mozvm.h"
#include "vm_inst.h"

int main(int argc, char const* argv[])
{
#define PRINT(OP) fprintf(stderr, "%-10s size=%d\n",  #OP, opcode_size(OP));
    OP_EACH(PRINT);
    return 0;
}
