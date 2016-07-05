#include "vm.h"
#include <string.h>
#include <assert.h>

typedef struct thread_t {
    inst_t *pc;
    char *saved[2 * (MAX_CAPTURE+1)];
    uint thread_id;
} thread_t;

typedef struct threadlist_t {
    thread_t *t;
    inst_t *base;
    uint n;
    uint size;
    uint list_id;
} threadlist_t;

static thread_t mk_thread(inst_t *pc, char *saved[])
{
    thread_t t;
    t.pc = pc;
    t.thread_id = 0;
    memcpy(t.saved, saved, sizeof(t.saved));
    return t;
}

static bool init_list(threadlist_t *list, prog_t *prog)
{
    list->n = 0;
    list->list_id = 1;
    list->size = prog->size;
    list->base = prog->code;
    list->t = (thread_t *) malloc(list->size * sizeof(thread_t));
    return list->t != NULL;
}

static void swap_thread(threadlist_t *lhs, threadlist_t *rhs)
{
    threadlist_t tmp = *lhs;
    *lhs = *rhs;
    *rhs = tmp;
}

static void clean_up(threadlist_t *list)
{
    list->n = 0;
    // list_id is unsigned int  maybe upper-bound overflowed
    if (++list->list_id == 0) {
        memset(list->t, 0, list->size * sizeof(thread_t));
        list->list_id = 1;
    }
}

static void add_thread(char *sp, threadlist_t *list, thread_t t)
{
    thread_t *p;
    int i;
    while (1) {
        i = t.pc - list->base;
        assert(i >= 0 && i < list->size);
        if (list->t[i].thread_id == list->list_id)
            break;
        list->t[i].thread_id = list->list_id;
        switch (t.pc->opcode) {
            case I_JMP:
                t.pc = t.pc->attr.child[0];
            break;
            
            case I_SPL:
                add_thread(sp, list, mk_thread(t.pc->attr.child[0], t.saved));
                t.pc = t.pc->attr.child[1];
            break;
            
            case I_SAV:
                t.saved[t.pc->attr.savepoint] = sp;
                t.pc++;
            break;
            
            default:
                // instruction handled by vm()
                p = &list->t[list->n++];
                t.thread_id = p->thread_id;
                *p = t;
            return ;
        }
    }
}

bool vm(char *re, prog_t *prog, char *saved[])
{
    threadlist_t clist = {0}, nlist = {0};
    thread_t *t;
    inst_t *pc;
    if (!init_list(&clist, prog))
        return false;
    if (!init_list(&nlist, prog)) {
        free(clist.t);    // remember free it
        return false;
    }
    
    // safe for sizeof even t not initialized
    memset(saved, 0, sizeof(t->saved));
    
    add_thread(re, &clist, mk_thread(prog->code, saved));
    uint i, j;
    bool is_matched = false;
    do {
        for (i = 0; i < clist.n; i++) {
            t = &clist.t[i];
            pc = t->pc;
            switch (pc->opcode) {
                case I_ALT:
                    if (*re == pc->attr.ch[1])
                        goto OKAY;
                // no break
                
                case I_CHR:
                    if (*re == pc->attr.ch[0])
                        goto OKAY;
                break;
                
                case I_SET:
                    if (!(charset[(uint) *re] & pc->attr.mask))
                        break;
                // no break
                
                case I_ANY:
                OKAY:
                    add_thread(re+1, &nlist, mk_thread(t->pc+1, t->saved));
                break;
                
                case I_MAE:
                    if (*re != '\0') break;
                
                case I_MAT:
                    is_matched = true;
                    memcpy(saved, t->saved, sizeof(t->saved));
                    assert(t->saved[0] != NULL);
                    j = clist.n - 1;
                    while (clist.t[j].saved[0] == NULL ||
                             clist.t[j].saved[0] > t->saved[0])
                         j--;
                    clist.n = j + 1;
                break;
                
                default:
                    fprintf(stderr, "ERROR: Unswitched case %u in vm()\n", pc->opcode);
                    exit(-1);
                break;
            }
        }
        swap_thread(&clist, &nlist);
        clean_up(&nlist);
    } while (*re++ && clist.n > 0);
    
    return is_matched;
}
