/*
 * Copyright (c) 2017 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The basic principle of operation is:
 *   No asynchronous behavior, no indeterminism.
 *   If you run the same thing 20 times, you get exactly the same result 20
 *   times.
 *   It does not matter if you are running from console, or in a debugger
 *   and you go for lunch in the middle of the debug session.
 *
 * This is achieved as follows:
 * The HW models run in their own simulated time. We do really not attempt
 * to link ourselves to the actual real time / wall time of the machine as this
 * would make execution indeterministic and debugging or instrumentation not
 * really possible. Although we may slow the run to real time.
 */

#include <soc.h>
#include "hw_models_top.h"
#include <stdlib.h>
#include "misc/util.h"
#include "argtable3.h"
#include <sys/types.h>
#include <unistd.h>


void cmd_line_free(void);

void main_clean_up(int exit_code)
{
	static int max_exit_code;

	max_exit_code = max(exit_code, max_exit_code);
	/*
	 * posix_soc_clean_up may not return if this is called from a SW thread,
	 * but instead it would get main_clean_up() recalled again
	 * ASAP from the HW thread
	 */
	posix_soc_clean_up();
	hwm_cleanup();
	cmd_line_free();
	exit(exit_code);
}


void store_pid(const char *file_name)
{
	FILE *fptr;
	pid_t pid = getpid();

	fptr = fopen(file_name, "w");

	if (fptr==NULL) {
		posix_print_error_and_exit("Could not open file %s for"
					" writing \n",
					file_name);

	}
	fprintf(fptr,"%ld", (long) pid);
	fclose(fptr);
}

static void *argtableg[10];
static char **tests_argv;
static int tests_argc;

void get_test_args(int *argc, char ***argv){
	*argv = tests_argv;
	*argc = tests_argc;
}

void parse_cmd_line(int argc, char *argv[])
{
	struct arg_lit  *help;
	struct arg_file *pid_file;
	struct arg_dbl  *stop_at;
	struct arg_str  *testargs;
	struct arg_end  *end = arg_end(20);

	help = arg_litn(NULL, "help", 0, 1, "display this help and exit");
	pid_file = arg_filen(NULL, "pid-file", "filename", 0, 1,
			"Save the process id in this file");

        stop_at  = arg_dbln(NULL, "stop-at", "<time>", 0, 1,
                        "In simulated seconds, when to stop automatically");

	testargs = arg_strn(NULL, NULL, "arg", 0, 100,
			"Extra arguments for tests");

	argtableg[0] = help;
	argtableg[1] = stop_at;
	argtableg[2] = pid_file;
	argtableg[3] = testargs;
	argtableg[4] = end;

	int nerrors = arg_parse(argc,argv,argtableg);


	if (help->count > 0)
	{
		printf("Usage: ");
		arg_print_syntax(stdout, argtableg, "\n");
		printf("\n");
		arg_print_glossary(stdout, argtableg, "  %-25s %s\n");
		main_clean_up(0);
	}


	if (nerrors > 0)
	{
		/* Display the error details contained in the arg_end struct.*/
		arg_print_errors(stdout, end, "");
		printf("Try '--help' for more information.\n");
		main_clean_up(1);
	}

	if (stop_at->count) {
		if ((*stop_at->dval)<0) {
			posix_print_error_and_exit("stop-at must be positive\n");
		}
		hwm_set_end_of_time((*stop_at->dval)*1e6);
	}

	if (pid_file->count){
		store_pid(pid_file->filename[0]);
	}

	tests_argc = testargs->count;
	tests_argv = (char**)testargs->sval;
}

void cmd_line_free(void){
	arg_freetable(argtableg, sizeof(argtableg) / sizeof(argtableg[0]));
}

/**
 * This is the actual main for the Linux process,
 * the Zephyr application main is renamed something else thru a define.
 *
 * Note that normally one wants to use this POSIX arch to be part of a
 * simulation engine, with some proper HW models and what not
 *
 * This is just a very simple demo which is able to run some of the sample
 * apps (hello world, synchronization, philosophers) and run the sanity-check
 * regression
 */
int main(int argc, char *argv[])
{
	parse_cmd_line(argc, argv);

	hwm_init();

	//Demostration of get_test_args()
	int argc2;
	char **argv2;
	get_test_args(&argc2, &argv2);
	for (int i = 0 ; i < argc2; i++){
		printf("%i: '%s'\n",i,argv2[i]);
	}
	//End of demo

	posix_boot_cpu();

	hwm_main_loop();

	return 0;

}
