#include "parse.h"
#include <string.h>
#include <assert.h>
#include <ctype.h>

#define MIN_TREESIZE 5

// TODO: for a better debug information display
// using a recursive-depth to show preceding tabs
#define PARSE_STACK(msg) {                     \
    if (flag & TRACE_PARSE)                    \
        fprintf(stdout, "    %s\n", msg);    \
}

typedef struct tree_t {
    node_t *root;
    node_t *stack;
    node_t *heap;
} tree_t;

static bool init_tree(tree_t *t, uint size)
{
    t->root = (node_t *) malloc(size * sizeof(node_t));
    if (t->root == NULL) return false;
    // make all nodes to NULL
    memset(t->root, 0, size * sizeof(node_t));
    t->stack = t->root;
    t->heap  = t->root + size;
    return true;
}

#define top(t)   ((t)->stack-1)
#define push(t)  ((t)->stack++)
#define pull(t)  (*--(t)->heap = *--(t)->stack, (t)->heap)
#define pop(t)   ((t)->stack--)

static void link_up(const node_t *base, tree_t *t)
{
    node_t *child1, *child2, *parent;
    // when t->stack is base itsetlf
    // if triggers we push a epsilon
    //if (t->stack == base) push(t)->op = NOP;
    if (top(t) < base) push(t)->op = NOP;
    while (top(t) > base) {
        child2 = pull(t);
        child1 = pull(t);
        parent = push(t);
        parent->op = CAT;
        parent->attr.child[0] = child1;
        parent->attr.child[1] = child2;
    }
    assert(top(t) == base);
}

static void add_to_set(uint mask, uint c)
{
    if (!(flag & IGNORE_CASE) || !isalpha(c)) {
        charset[c] |= mask;
        return ;
    }
    charset[c] |= mask;
    c ^= (1<<5);    // invert case
    charset[c] |= mask;
}

static bool parse_set(char **re, tree_t *t)
{
    struct node_t *node, *base = t->stack;
    static uint mask = 1;
    unsigned char *sp = (unsigned char*) *re;
    // only supports 32 character sets
    // once mask equal to zero
    // means all 32 sets are used up
    // have no room for more
    // using __int64 for 64 sets
    // if you want a unlimited sets
    // redesign the prog_t structure
    assert(mask != 0);
    // only one bit set e.g. 2^n
    assert((mask & (mask-1)) == 0);
    bool is_negtive = false;
    if (*sp == '^') {
        sp++;
        is_negtive = true;
    }
    uint c;
    if (*sp == ']') {
        // [] adds nothing  never match '\0'
        if (*(sp-1) == '[') {
            sp++;    // empty set eat ]
            return true;
        }
        // [?] adds single-char set
        c = *(sp++ - 1);
        goto DEFAULT;
    }
    
    while (1) {
        c = *sp++;
        switch (c) {
            case '\0':
                PARSE_STACK("case '\\0' in parse_set()");
                sp--;
                // no break
            
            case ']':
                PARSE_STACK("case ']' in parse_set()");
                goto EOW;
            
            default:
            DEFAULT:
                if (*sp != '-' || sp[1] == ']' || sp[1] == '\0') {
                    sprintf(median, "DEFAULT: Entering add_to_set([%c], %u) in parse_set()", c, mask);
                    PARSE_STACK(median);
                    add_to_set(mask, c);
                }
                else {
                    sprintf(median, "DEFAULT: Entering add_to_set([%c-%c], %u) in parse_set()", c, sp[1], mask);
                    PARSE_STACK(median);
                    while (c <= sp[1])
                        add_to_set(mask, c++);
                    sp += 2;        // [?-?]: skip ?]
                }
            break;
        }
    }
    EOW:
    if (is_negtive) {
        // never match c = '\0'
        for (c = 1; c < UCHAR_MAX; c++)
            charset[c] ^= mask;
    }
    node = push(t);
    node->op = SET;
    node->attr.mask = mask;
    mask <<= 1;        // now use next bit
    *re = (char *) sp;
    assert(top(t) == base);
    return true;
}

static bool parse_scope(char **re, tree_t *t, uint scope)
{
    node_t *child1, *child2, *node;
    node_t *base = t->stack;
    char c, *sp = *re;
    while (1) {
        c = *sp++;
        switch (c) {
            case '\\':
                PARSE_STACK("case '\\\\' in parse_scope()");
                if (*sp) c = *sp++;
                // no break
            
            default:
            DEFAULT:
                sprintf(median, "DEFAULT char '%c' in parse_scope()", c);
                PARSE_STACK(median);
                node = push(t);
                node->op = CHR;
                node->attr.c = c;
            break;
            
            case '.':
                PARSE_STACK("case '.' in parse_scope()");
                push(t)->op = ANY;
            break;
            
            case '[':
                sprintf(median, "case '[': Entering parse_set() in parse_scope(%u)", scope);
                PARSE_STACK(median);
                if (!parse_set(&sp, t))
                    return false;
            break;
            
            case '?':
                PARSE_STACK("case '?' in parse_scope()");
                if (t->stack == base) goto DEFAULT;
                child1 = pull(t);
                node   = push(t);
                node->op = (*sp == '?') ? (sp++, WOP) : OPT;
                node->attr.child[0] = child1;
            break;
            
            case '*':
                PARSE_STACK("case '*' in parse_scope()");
                if (t->stack == base) goto DEFAULT;
                child1 = pull(t);
                node   = push(t);
                node->op = (*sp == '?') ? (sp++, WAS) : AST;
                node->attr.child[0] = child1;
            break;
            
            case '+':
                PARSE_STACK("case '+' in parse_scope()");
                if (t->stack == base) goto DEFAULT;
                child1 = pull(t);
                node  = push(t);
                node->op = (*sp == '?') ? (sp++, WPL) : PLS;
                node->attr.child[0] = child1;
            break;
            
            case '|':
                PARSE_STACK("case '|' in parse_scope()");
                sprintf(median, "Entering link_up() in parse_scope(%u)", scope);
                PARSE_STACK(median);
                link_up(base, t);
                
                child1 = top(t);
                sprintf(median, "Entering parse_scope(%d) in parse_scope(%u)", scope, scope);
                PARSE_STACK(median);
                if (!parse_scope(&sp, t, scope))
                    return false;
                child2 = top(t);
                
                if (child1->op == NOP && child2->op == NOP) {
                    // you just can pop one node e.g. the child2
                    // :. child1 may be used by other nodes
                    // so you can't just simply pop child1 out
                    pop(t);
                } else if (child1->op == NOP || child2->op == NOP) {
                    if (child1->op == NOP) {
                        child1 = pull(t);
                        pop(t);
                    } else {
                        pop(t);
                        child1 = pull(t);
                    }
                    node = push(t);
                    node->op = SEL;
                    node->attr.child[0] = child1;
                } else {
                    child2 = pull(t);
                    child1 = pull(t);
                    node   = push(t);
                    node->op = SEL;
                    node->attr.child[0] = child1;
                    node->attr.child[1] = child2;
                }
            break;
            
            case '(':
                sprintf(median, "case '(': Entering parse_scope(%u) in parse_scope(%u)", scope+1, scope);
                PARSE_STACK(median);
                if (!parse_scope(&sp, t, scope+1))
                    return false;
                child1 = pull(t);
                node   = push(t);
                node->op = CAP;
                node->attr.child[0] = child1;
            break;
            
            case ')':
                PARSE_STACK("case ')' in parse_scope()");
                if (scope == 0) goto DEFAULT;
                goto EOW;
            break;
            
            case '$':
                PARSE_STACK("case '$' in parse_scope()");
                if (*sp != '\0') goto DEFAULT;
                // no break
            
            case '\0':
                PARSE_STACK("case '\\0' in parse_scope()");
                sp--;
                goto EOW;
            break;        // unreachable
        }
    }
    EOW:
    *re = sp;        // updates re
    sprintf(median, "Entering link_up() in parse_scope(%u)", scope);
    PARSE_STACK(median);
    link_up(base, t);
    return true;
}

static bool parse_regex(char *re, tree_t *t)
{
    node_t *child, *node;
    node_t *base = t->stack;
    if (*re != '^') {
        push(t)->op = ANY;
        child = pull(t);
        node  = push(t);
        node->op = WAS;
        node->attr.child[0] = child;
    } else
        re++;    // just skip '^'
    
    PARSE_STACK("Entering parse_scope(0) in parse_regex()");
    if (!parse_scope(&re, t, 0)) {
        fprintf(stderr, "ERROR %#x: Fatal error in parse_scope()", 7);
        return false;
    }
    
    child = pull(t);
    node  = push(t);
    node->op = CAP;
    node->attr.child[0] = child;
    if (*re == '$') {
        re++;
        push(t)->op = MAE;
    }
    
    PARSE_STACK("Entering link_up() in parse_regex()");
    link_up(base, t);
    
    return true;
}

bool parse(char *re, node_t **tree)
{
    tree_t t;
    uint max_size = 2*strlen(re) + MIN_TREESIZE;
    PARSE_STACK("Entering init_tree() in parse()");
    if (!init_tree(&t, max_size)) {
        fprintf(stderr, "ERROR %#x: Memory exhausted in init_tree()", 5);
        return false;
    }
    
    PARSE_STACK("Entering parse_regex() in parse()");
    if (!parse_regex(re, &t)) {
        fprintf(stderr, "ERROR %#x: Fatal error in parse_regex()", 6);
        free(t.root);
        return false;
    }
    
    assert(t.stack <= t.heap);
    *tree = t.root;
    
    return true;
}
