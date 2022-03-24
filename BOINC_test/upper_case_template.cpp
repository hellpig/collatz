// A wrapper for sticking my code into the BOINC server's example_app
// Modified from apps/upper_case.cpp
// No Graphics
// No wasting CPU time
// No checkpoints
//
// command line options
// -cpu_time N

#ifdef _WIN32
#include "boinc_win.h"
#else
#include "config.h"
#include <cstdio>
#include <cctype>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#endif

#include "str_util.h"
#include "util.h"
#include "filesys.h"
#include "boinc_api.h"
#include "mfile.h"
#include "graphics2.h"

using std::string;

#define INPUT_FILENAME "in"
#define OUTPUT_FILENAME "out"





/*put your global code here*/

















double cpu_time = 20;

int main(int argc, char **argv) {
    int i;
    int c, nchars = 0, retval;
    double fsize;
    char input_path[512], output_path[512], buf[256];
    MFILE out;
    FILE* infile;

    for (i=0; i<argc; i++) {
        if (!strcmp(argv[i], "-cpu_time")) {
            cpu_time = atof(argv[++i]);
        }
    }

    retval = boinc_init();
    if (retval) {
        fprintf(stderr, "%s boinc_init returned %d\n",
            boinc_msg_prefix(buf, sizeof(buf)), retval
        );
        exit(retval);
    }

    // open the input file (resolve logical name first)
    //
    boinc_resolve_filename(INPUT_FILENAME, input_path, sizeof(input_path));
    infile = boinc_fopen(input_path, "r");
    if (!infile) {
        fprintf(stderr,
            "%s Couldn't find input file, resolved name %s.\n",
            boinc_msg_prefix(buf, sizeof(buf)), input_path
        );
        exit(-1);
    }

    // get size of input file (used to compute fraction done)
    //
    file_size(input_path, fsize);

    boinc_resolve_filename(OUTPUT_FILENAME, output_path, sizeof(output_path));

    // open output file
    retval = out.open(output_path, "wb");
    if (retval) {
        fprintf(stderr, "%s APP: upper_case output open failed:\n",
            boinc_msg_prefix(buf, sizeof(buf))
        );
        fprintf(stderr, "%s resolved name %s, retval %d\n",
            boinc_msg_prefix(buf, sizeof(buf)), output_path, retval
        );
        perror("open");
        exit(1);
    }

    double start = dtime();

    // main loop - read characters, write
    //
    for (i=0; ; i++) {
        c = fgetc(infile);

        if (c == EOF) break;
        out._putchar(c);
        nchars++;
    }
    boinc_fraction_done(0);
    out._putchar('\n');

    retval = out.flush();
    if (retval) {
        fprintf(stderr, "%s APP: upper_case flush failed %d\n",
            boinc_msg_prefix(buf, sizeof(buf)), retval
        );
        exit(1);
    }

    double eTime = dtime()-start;




    /*put your main() code here*/


















    boinc_fraction_done(1);
    boinc_finish(0);
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR Args, int WinMode) {
    LPSTR command_line;
    char* argv[100];
    int argc;

    command_line = GetCommandLine();
    argc = parse_command_line( command_line, argv );
    return main(argc, argv);
}
#endif
