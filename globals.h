/*****************************
 * First_created: 160701
 * Last_modified: 160705
 * Regular expression header
 * Pure comprehension purpose
 * The code is self-explanatory
*****************************/
#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdio.h>
#include <stdlib.h>

#ifndef UCHAR_MAX
    #define UCHAR_MAX 255
#endif

#define MAX_CAPTURE 9    // $0 - $9

#ifndef bool
    typedef enum {false, true} bool;
    // GCC-only
    #define bool __typeof(bool)
#endif

typedef unsigned uint;

enum opcode_t {
    I_ALT, 
    I_CHR, 
    I_ANY, 
    I_SET, 
    I_MAT, 
    I_MAE, 
    I_JMP, 
    I_SPL, 
    I_SAV
};

typedef struct inst_t {
    enum opcode_t opcode;
    union {
        struct inst_t *child[2];
        uint ch[2];
        uint mask;
        uint savepoint;
    } attr;
} inst_t;

enum op_t {
    CHR, 
    ANY, 
    SET, 
    MAE, 
    NOP, 
    CAT, 
    SEL, 
    OPT, 
    WOP, 
    AST, 
    WAS, 
    PLS, 
    WPL, 
    CAP
};

typedef struct node_t {
    enum op_t op;
    union {
        struct node_t *child[2];
        uint c;
        uint mask;
    } attr;
} node_t;

typedef struct prog_t {
    inst_t *code;
    uint size;
} prog_t;

uint charset[UCHAR_MAX];

char median[UCHAR_MAX];

extern uint flag;

#define IGNORE_CASE   1
#define TRACE_PARSE   2
#define TRACE_COMPILE 4
#define TRACE_VM      8
#define TRACE_ALL    14
#define ALL_FLAGS    15

#endif
