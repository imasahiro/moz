#include <stdio.h>
#define MOZVM_MOZVM1_OPCODE_SIZE 1
#define MOZVM_MOZVM2_OPCODE_SIZE 1
#include "vm1/instruction.h"
#include "mozvm.h"
#include "vm_inst.h"
#include "compiler/ir.h"
#include "vm2_inst.h"

int main(int argc, char const* argv[])
{
    fprintf(stderr, "sizeof(*Node) %ld\n", sizeof(Node));
    fprintf(stderr, "sizeof(AstLog) %ld\n", sizeof(AstLog));
#define PRINT(OP) fprintf(stderr, "%-10s size=%d\n",  #OP, mozvm1_opcode_size(OP));
#define PRINT2(OP) fprintf(stderr, "%-10s size=%d\n",  #OP, mozvm2_opcode_size(OP));
    OP_EACH(PRINT);
    FOR_EACH_IR(PRINT2);
    return 0;
}
