#include "compile.h"
#include <string.h>
#include <assert.h>
#include <ctype.h>

#define MIN_CODESIZE 6

static bool nocase   = false;
static bool matchend = false;
static uint nextsave = 0;

static inst_t* do_compile(const node_t *t, inst_t *pc)
{
    inst_t *next;
    uint c, savepoint;
    while (1) {
        switch (t->op) {
            case CHR:
                c = t->attr.c;
                if (!nocase || !isalpha(c)) {
                    pc->opcode = I_CHR;
                    pc->attr.ch[0] = c;
                } else {
                    pc->opcode = I_ALT;
                    pc->attr.ch[0] = c;
                    c ^= (1<<5);
                    pc->attr.ch[1] = c;
                }
                pc++;
            goto DONE;
            
            case ANY:
                pc->opcode = I_ANY;
                pc++;
            goto DONE;
            
            case SET:
                pc->opcode = I_SET;
                pc->attr.mask = t->attr.mask;
                pc++;
            goto DONE;
            
            case MAE:
                matchend = 1;
            goto DONE;
            
            case NOP:
                // do nothing here (no operation for epsilon)
            goto DONE;
            
            case CAT:
                // [!] may stackoverflow
                pc = do_compile(t->attr.child[0], pc);
                t = t->attr.child[1];
            break;    // it's a break for child[1]
            
            case SEL:
                pc->opcode = I_SPL;
                pc->attr.child[0] =   pc + 1;
                next = do_compile(t->attr.child[0], pc+1);
                pc->attr.child[1] = next + 1;
                next->opcode = I_JMP;
                pc = next->attr.child[0] = do_compile(t->attr.child[1], next+1);
            goto DONE;
            
            case OPT:
                pc->opcode = I_SPL;
                pc->attr.child[0] = pc+1;
                pc = pc->attr.child[1] = do_compile(t->attr.child[0], pc+1);
            goto DONE;
            
            case WOP:
                pc->opcode = I_SPL;
                pc->attr.child[1] = pc+1;
                pc = pc->attr.child[0] = do_compile(t->attr.child[0], pc+1);
            goto DONE;
            
            case AST:
                pc->opcode = I_SPL;
                pc->attr.child[0] =   pc + 1;
                next = do_compile(t->attr.child[0], pc+1);
                pc->attr.child[1] = next + 1;
                next->opcode = I_JMP;
                next->attr.child[0] = pc;
                pc = next + 1;
            goto DONE;
            
            case WAS:
                pc->opcode = I_SPL;
                pc->attr.child[1] =   pc + 1;
                next = do_compile(t->attr.child[0], pc+1);
                pc->attr.child[0] = next + 1;
                next->opcode = I_JMP;
                next->attr.child[0] = pc;
                pc = next + 1;
            goto DONE;
            
            case PLS:
                next = do_compile(t->attr.child[0], pc);
                next->opcode = I_SPL;
                next->attr.child[0] = pc;
                next->attr.child[1] = next + 1;
                pc = next + 1;
            goto DONE;
            
            case WPL:
                next = do_compile(t->attr.child[0], pc);
                next->opcode = I_SPL;
                next->attr.child[1] = pc;
                next->attr.child[0] = next + 1;
                pc = next + 1;
            goto DONE;
            
            case CAP:
                if (nextsave > MAX_CAPTURE) {
                    t = t->attr.child[0];
                    break;
                }
                savepoint = 2 * nextsave++;
                pc->opcode = I_SAV;
                pc->attr.savepoint = savepoint;
                next = do_compile(t->attr.child[0], pc+1);
                next->opcode = I_SAV;
                next->attr.savepoint = savepoint + 1;
                pc = next + 1;
            goto DONE;
            
            default:
                fprintf(stderr, "ERROR: Unswitched case %u in do_compile()\n", t->op);
            exit(-1);        // which panic :-(
        }
    }
    DONE:
    return pc;
}

bool compile(const char *re, node_t *tree, prog_t *prog)
{
    uint max_size = 2*strlen(re) + MIN_CODESIZE;
    prog->code = (inst_t *) malloc(max_size * sizeof(inst_t));
    if (prog->code == NULL) {
        fprintf(stderr, 
            "Memory exhausted in malloc() of compile() while allocating %u bytes\n", 
            max_size * sizeof(inst_t)
        );
        return false;
    }
    
    // bool-type in globals.h is a pseudo-bool type
    // so we need convert (flag & IGNORE_CASE) into 0/1
    // which indeed is unnecessary
    // i just showing a alternative
    nocase = (flag & IGNORE_CASE) || false;
    
    inst_t *pc = do_compile(tree, prog->code);
    pc->opcode = matchend ? I_MAE : I_MAT;
    prog->size = ++pc - prog->code;
    
    assert(prog->size <= max_size);
    
    // shrink memory prog->code got into prog->size (for saving space)
    // since prog->code is linear structure  you can do it safely
    pc = (inst_t *) realloc(prog->code, prog->size * sizeof(inst_t));
    if (pc == NULL) {
        fprintf(stderr, 
            "Memory exhausted in realloc() of compile() while reallocating %u bytes\n", 
            prog->size * sizeof(inst_t)
        );
        // failed to shrinking prog->code  it's a try not fatal
        // so just leave the leftover memories alone
        // alternative to free memories and report a error message:
        //free(prog->code);
        //return false;
    } else
        prog->code = pc;    // successful shrinked its size now update it
    
    // we already constructed pseudo-program
    // free intermediate representive to save memory
    free(tree);
    
    if (flag & TRACE_COMPILE) print_prog(prog);
    
    return true;
}

static char* calc_range(char *sp, uint first, uint last)
{
    // three situations: just a, ab, a-z
    *sp++ = first;
    if (last > first) {
        // if more than 2 elements
        if (last > first+1) *sp++ = '-';
        *sp++ = last;
    }
    return sp;
}

static const char* make_charset(inst_t *pc)
{
    uint mask   = pc->attr.mask;
    uint marked = mask;
    char *sp = median;
    
    // actually '\1' is a control character
    // but we used to here to check it's a ^set
    // mostly :. you cannot type '\1' in keyboard
    if (charset[1] & mask) {
        // since ^ invert the set we need to invert
        // marked to trace [^...] e.g. the contrary way
        marked = 0;
        *sp++ = '^';
    }
    
    if ((charset[']'] & mask) == marked)
        *sp++ = ']';
    
    uint c, first = 0, last = 0;
    for (c = 1; c < UCHAR_MAX; c++) {
        if ((charset[c] & mask) != marked || c == ']')
            continue;
        if (last && (last < c-1)) {
            sp = calc_range(sp, first, last);
            // reached a new range segment
            // consider about a b c d _ ... _ x y z
            first = last = c;
        } else {
            last = c;
            if (first == 0) first = c;
        }
    }
    
    // whatever the last set segment wont get calculated
    // it's need a new set segment to trigger calc_range()
    // so here we calculate it manully
    if (last) sp = calc_range(sp, first, last);
    *sp = '\0';        // don't forget it :-)
    
    return median;
}

void print_prog(const prog_t *prog)
{
    const inst_t *base = prog->code;
    inst_t *pc;
    uint i = 0, size = prog->size;
    while (i < size) {
        pc = &prog->code[i++];
        fprintf(stdout, "    %03d: ", pc-base);
        switch (pc->opcode) {
            case I_ALT:
                fprintf(stdout, "ALT %c %c\n", pc->attr.ch[0], pc->attr.ch[1]);
            break;
            
            case I_CHR:
                fprintf(stdout, "CHR %c\n", pc->attr.ch[0]);
            break;
            
            case I_ANY:
                fprintf(stdout, "ANY\n");
            break;
            
            case I_SET:
                fprintf(stdout, "SET [%s]\n", make_charset(pc));
            break;
            
            case I_MAT:
            case I_MAE:
                fprintf(stdout, pc->opcode == I_MAT ? "MAT\n" : "MAE\n");
            break;
            
            case I_JMP:
                fprintf(stdout, "JMP %03d\n", pc->attr.child[0] - base);
            break;
            
            case I_SPL:
                fprintf(stdout, "SEP %03d %03d\n", 
                        pc->attr.child[0]-base, 
                        pc->attr.child[1]-base
                );
            break;
            
            case I_SAV:
                fprintf(stdout, "SAV %d\n", pc->attr.savepoint);
            break;
            
            default:
                fprintf(stderr, "ERROR: Unswitched case %u in print_prog()\n", pc->opcode);
            exit(-2);
        }
    }
}
