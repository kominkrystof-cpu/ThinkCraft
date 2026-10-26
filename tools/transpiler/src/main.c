#include "classfile.h"
#include "emit_nasm.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *program) {
    fprintf(stderr,
            "usage: %s --class <file.class> --out <file.asm> [--prefix <symbol>] [--verbose]\n",
            program);
}

int main(int argc, char **argv) {
    const char *class_path = NULL;
    const char *out_path = NULL;
    const char *prefix = "j";
    int verbose = 0;
    int option;
    ClassFile *cf;
    int result;

    static struct option long_options[] = {
        { "class", required_argument, NULL, 'c' },
        { "out", required_argument, NULL, 'o' },
        { "prefix", required_argument, NULL, 'p' },
        { "verbose", no_argument, NULL, 'v' },
        { "help", no_argument, NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    while ((option = getopt_long(argc, argv, "c:o:p:vh", long_options, NULL)) != -1) {
        switch (option) {
        case 'c':
            class_path = optarg;
            break;
        case 'o':
            out_path = optarg;
            break;
        case 'p':
            prefix = optarg;
            break;
        case 'v':
            verbose = 1;
            break;
        case 'h':
            usage(argv[0]);
            return 0;
        default:
            usage(argv[0]);
            return 2;
        }
    }

    if (class_path == NULL || out_path == NULL) {
        usage(argv[0]);
        return 2;
    }

    cf = classfile_load(class_path);
    if (cf == NULL) {
        return 1;
    }

    result = emit_nasm(cf, out_path, prefix, verbose);
    classfile_free(cf);
    return result ? 0 : 1;
}
