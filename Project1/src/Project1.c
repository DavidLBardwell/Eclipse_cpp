/*
 * David Bardwell
 * Computer Architecture Project 1 - MIPS Disassembler
 *
 * for Computer Organization and Design
 * Tyngsboro Campus - Wednesday evening 6-9pm class
 *
 * Due: October 1, 2008
 *
 * Project solution developed in C using
 * Microsoft Developer Studio 97
 *
 */
/* Project comments:
   Project 1 is a MIPS machine code instruction disassembler for
   some R-Type and I-Type instructions. The program below does the
   following:
   (1) Load the MIPS instructions into an unsigned integer array
   (2) Calls the disassembler function to store the disassembled instructions
   (3) prints out the disassembled instructions
 */

/* includes */
#include <stdio.h>
#include <string.h>
#include <malloc.h>

/*
MIPS instruction masks to determine components of instructions
 */
#define MASK_OPCODE     0xfc000000
#define MASK_REG1       0x03e00000
#define MASK_REG2       0x001f0000
#define MASK_REG3       0x0000f800
#define MASK_FUNC       0x0000003f
#define MASK_PC_OFFSET  0x0000ffff

/*
Right bitwise shift positions to get value of constituent components
of the instruction
 */
#define OPCODE_SHIFT  26
#define REG1_SHIFT    21
#define REG2_SHIFT    16
#define REG3_SHIFT    11

/* Function codes for R-TYPE instructions */
#define ADD_FUNCT  0x20
#define SUB_FUNCT  0x22
#define AND_FUNCT  0x24
#define OR_FUNCT   0x25
#define SLT_FUNCT  0x2a

/* Opcodes for I-TYPE instructions */
#define LW_OPCODE   0x23
#define SW_OPCODE   0x2b
#define BEQ_OPCODE  0x04
#define BNE_OPCODE  0x05

/* forward declarations */
int LoadMIPSInstructions();
void DisassembleInstructions(int number_of_instructions);

/* pointer that holds MIPS instructions to disassemble */
static unsigned int *mips_instructions;

/* pointer that hold the disassembled instructions for output */
static char *mips_dis[15];

/**
 *  Name:        main
 *  Description: main function to load, disassemble and print MIPS instructions
 *
 *  Returns: int; 0 on normal exit
 */
int main(int argc, char *argv[])
{
    int number_of_instructions;
    int i;

    /* Initialize the MIPS instruction array for disassembling */
    number_of_instructions = LoadMIPSInstructions();

    /* Disassemble the loaded MIPS instructions */
    DisassembleInstructions(number_of_instructions);

    /* print out the instructions */
    for (i = 0; i < number_of_instructions; i++) {
        printf("%s\n", mips_dis[i]);
    }

    /* free allocated memory */
    for (i = 0; i < number_of_instructions; i++) {
        free(mips_dis[i]);
    }
    free(mips_instructions);
    return 0;
}

/**
 * Name:        LoadMIPSInstructions
 * Description: Loads MIPS instructions for disassembling
 *
 * Returns:     int; number of instructions loaded for disassembling
 *
 **/
int LoadMIPSInstructions()
{
    mips_instructions = (unsigned int *) malloc(15 * sizeof(unsigned int));

    /* Initialize the MIPS instruction array for disassembling */
    mips_instructions[0]  = 0x022da822;
    mips_instructions[1]  = 0x12a70003;
    mips_instructions[2]  = 0x8d930018;
    mips_instructions[3]  = 0x02689820;
    mips_instructions[4]  = 0xad930018;
    mips_instructions[5]  = 0x02697824;
    mips_instructions[6]  = 0xad8ffff4;
    mips_instructions[7]  = 0x018c6020;
    mips_instructions[8]  = 0x018c6020;
    mips_instructions[9]  = 0x158ffff6;
    mips_instructions[10] = 0x02a4a825;
    //mips_instructions[11] = 0x02f68820; /* test case */
    //mips_instructions[12] = 0xad0dfff4; /* test case */
    return 11;
}

/**
 * Name:        DisassembleInstructions
 * Description: Disassembles the MIPS machine code instructions
 *              stored in mips_instructions and stores the resulting
 *              disassembled instruction in mips_dis[] array of
 *              character strings.
 *
 * Returns:     void
 */
void DisassembleInstructions(int number_of_instructions)
{
    unsigned int opcode;         /* opcode         6-bits, bit pos 31-26 in word */
    unsigned int src_register1;  /* src_register1  5-bits, bit pos 25-21 in word */
    unsigned int src_register2;  /* src_register2  5-bits, bit pos 20-16 in word */
    unsigned int dest_register3; /* dest_register3 5-bits, bit pos 15-11 in word */
    unsigned int function_code;  /* func_code      6-bits, bit pos 5-0   in word */
    char function_code_str[10];
    short pc_offset;             /* pc_offset 16-bits bit pos 15-0  in word */
    unsigned int start_address;
    unsigned int next_address;
    int instr_count;

    /* Initialize starting address for the MIPS instructions at 0x7a060 */
    start_address = 0x7a060;
    next_address = start_address; /* next address will start at the start_address */

    /* loop over our MIPS instructions and disassemble them */
    for (instr_count = 0; instr_count < number_of_instructions; instr_count++) {

        /* allocate 81 characters from the heap for the next instruction */
        mips_dis[instr_count] = (char *) malloc(81);

	    /* Get Opcode */
	    opcode = (mips_instructions[instr_count] & MASK_OPCODE) >> OPCODE_SHIFT ;

        /* Get Register 1 */
        src_register1 = (mips_instructions[instr_count] & MASK_REG1) >> REG1_SHIFT ;

        /* Get Register 2 */
        src_register2 = (mips_instructions[instr_count] & MASK_REG2) >> REG2_SHIFT;

        /* Handle the R-Type instructions here i.e. opcode = 0 */
        if (opcode == 0) {
            /* Get Register 3 */
            dest_register3 = (mips_instructions[instr_count] & MASK_REG3) >> REG3_SHIFT;

            /* Get Function code */
            function_code = mips_instructions[instr_count] & MASK_FUNC;

            if (function_code == ADD_FUNCT)
                strcpy(function_code_str, "add");
            else if (function_code == SUB_FUNCT)
                strcpy(function_code_str, "sub");
            else if (function_code == AND_FUNCT)
                strcpy(function_code_str, "and");
            else if (function_code == OR_FUNCT)
                strcpy(function_code_str, "or");
            else if (function_code == SLT_FUNCT)
                strcpy(function_code_str, "slt");

            /* prepare output of R-TYPE instruction
               careful to make sure dest_register3 which is the destination
               register is first in the disassembled instruction.
             */
            sprintf(mips_dis[instr_count], "%x  %s $%d, $%d, $%d",
		                      next_address, function_code_str,
							  dest_register3, src_register1, src_register2);
       }
       else {
           /* Handle I-Type instructions: load, store, beq, bne */
           int current_address, branch_address;
           pc_offset = mips_instructions[instr_count] & MASK_PC_OFFSET;

           switch(opcode) {
           case LW_OPCODE:
               strcpy(function_code_str, "lw");
               sprintf(mips_dis[instr_count], "%x  %s $%d, %d($%d)",
	                  next_address, function_code_str,
					  src_register2, pc_offset, src_register1);
               break;

           case SW_OPCODE:
               strcpy(function_code_str, "sw");
               sprintf(mips_dis[instr_count], "%x  %s $%d, %d($%d)",
	                  next_address, function_code_str,
					  src_register2, pc_offset, src_register1);
               break;

           case BEQ_OPCODE:
               /* calculate the branch address from the next address + 4 and
                * keep in mind that the pc_offset is in words so we need to
                * left shift 2 positions to get amount in bytes */
               current_address = next_address + 4;
               branch_address = current_address + (pc_offset << 2);
               strcpy(function_code_str, "beq");
               sprintf(mips_dis[instr_count], "%x  %s $%d, $%d, address %x",
	                  next_address, function_code_str,
					  src_register1, src_register2, branch_address);
               break;

           case BNE_OPCODE:
               current_address = next_address + 4;
               branch_address = current_address + (pc_offset << 2);
               strcpy(function_code_str, "bne");
               sprintf(mips_dis[instr_count], "%x  %s $%d, $%d, address %x",
	                  next_address, function_code_str,
					  src_register1, src_register2, branch_address);
               break;

           default:
               break;
           }
       }

       /* increment our next address for next instruction by 4 bytes or 1 word */
       next_address += 4;
    }
}
