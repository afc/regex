#include <unistd.h>
#include <string.h>
#include "globals.h"
#include "parse.h"
#include "compile.h"
#include "vm.h"

uint flag = 0;

static bool readline(FILE *fin)
{
    if (fgets(median, UCHAR_MAX-1, fin) == NULL)
        return false;    // reached EOF or unknown error
    uint len = strlen(median);
    if (median[len-1] == '\n')
        median[len-1] = '\0';
    return true;
}

static void grep(char *re, prog_t *prog, const char *fname)
{
    static char *captures[2 * (MAX_CAPTURE+1)];
    //memset(captures, 0, 20 * sizeof(char *));
    
    FILE *fin = NULL;
    if (fname && (fin = fopen(fname, "r")) == NULL) {
        fprintf(stderr, "ERROR: failed to open `%s'\n", fname);
        return ;
    }
    
    if (flag & TRACE_VM)
        fprintf(stdout, "Matching result: \n");
    
    while (readline(fin)) {
        if (vm(median, prog, captures)) {
            if (flag & TRACE_VM) fprintf(stdout, "    ");
            printf("%s: %s\n", fname, median);
        }
        // TODO: print out captures here
    }
    
    if (fclose(fin) == EOF)
        fprintf(stderr, "ERROR: failed to close `%s'\n", fname);
}

int main(int argc, char *argv[])
{
#ifndef DEBUG
    uint i, opt;
    while ((opt = getopt(argc, argv, "ipcvda:")) != -1) {
        switch (opt) {
            case 'i': flag |= IGNORE_CASE;   break;
            
            case 'p': flag |= TRACE_PARSE;   break;
            
            case 'c': flag |= TRACE_COMPILE; break;
            
            case 'v': flag |= TRACE_VM;      break;
            
            case 'd': flag |= TRACE_ALL;     break;
            
            case 'a': flag |= ALL_FLAGS;     break;
            
            default:break;
        }
    }
    
    if ((i = optind) >= argc) {
        fprintf(stdout, "Usage: regex [-ipcvda] regex files...\n");
        return 0;
    }
#else
    flag |= TRACE_ALL;
    char *re = "";
    char *fname = "a.txt";
#endif
    
    prog_t *prog = (prog_t *) malloc(sizeof(prog_t));
    if (prog == NULL) {
        fprintf(stderr, "ERROR %#x: Memory exhausted\n", 2);
        return 2;
    }
    
#ifdef DEBUG
    fprintf(stderr, "You're in DEBUG mode\n");
#endif
    
    node_t *tree;
    if (flag & TRACE_PARSE) {
        fprintf(stdout, "Tracing parsing now...\n");
        fprintf(stdout, "    Entering parse() in main()\n");
    }
    
#ifndef DEBUG
    char *regex = argv[i];        // point to regex string
#else
    char *regex = re;
#endif
    
    memset(charset, 0, UCHAR_MAX);    // make charset[] all 0s
    
    if (!parse(regex, &tree)) {
        fprintf(stderr, "ERROR %#x: Fatal error during parsing\n", 3);
        free(prog);        // tree already reclaimed
        return 3;
    }
    
    if (flag & TRACE_COMPILE) {
        fprintf(stdout, "Tracing compiling now...\n");
        fprintf(stdout, "    Entering compile() in main()\n");
    }
    
    if (!compile(regex, tree, prog)) {
        fprintf(stderr, "ERROR %#x: Fatal error during compiling\n", 4);
        free(prog);
        return 4;
    }
    
    if (flag & TRACE_VM) {
        fprintf(stdout, "Tracing simulating now...\n");
        fprintf(stderr, "    Regular expression: %s\n", regex);
        fprintf(stdout, "    Entering grep() loop cycles in main()\n");
    }
    
#ifndef DEBUG
    while (++i < argc)
        grep(regex, prog, argv[i]);
#else
    grep(regex, prog, fname);
#endif
    
    free(prog->code);    // memory reclaim routine
    free(prog);
    
    return 0;
}
