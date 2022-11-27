#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <math.h>
#include "qsim.h"

/* declare global variables */

int             W_verbose = 0;
pthread_mutex_t W_mutex   = PTHREAD_MUTEX_INITIALIZER;

struct mutexint threads_run;
pthread_mutex_t compiler_mutex = PTHREAD_MUTEX_INITIALIZER;
struct mutexint C_stepsahead;
pthread_mutex_t renderer_mutex = PTHREAD_MUTEX_INITIALIZER;
struct mutexint R_discardable;

#define main_ISCHAR  0x100

int main(int argc, char **argv)
{
	int argi, r = EXIT_FAILURE, pthreadr;
	const char *path = NULL;
	struct exp exp;
	pthread_t compiler_thread, renderer_thread;

	/* parse arguments passed to program for options */

	for (argi = 1; --argc; ++argi)
		if (argv[argi][0] == '-')
			switch (argv[argi][1] == '-' ?
			          arrin(&argv[argi][2], 3, "verbose", "file", "output")
			        : (int)argv[argi][1] | main_ISCHAR)
			{
			case -1:
			/* unknown option */
				warn(WL_warn, "qsim", "Unknown option \"%a\" provided, ignoring.\n", argv[argi]);

				break;

			case 0:
			case (int)'v' | main_ISCHAR:
			/* toggle verbose */
				W_verbose = 1;

				break;

			case 1:
			case (int)'f' | main_ISCHAR:
			/* experiment file */
				if (argc == 1)
					warn(WL_fail, "qsim", "No file provided after (-f|--file).\n");
				else
					path = argv[++argi], --argc;

				break;

			case 2:
			case (int)'o' | main_ISCHAR:
			/* output file */
				if (argc == 1)
					warn(WL_fail, "qsim", "No file provided after (-o|--output).\n");
				else {
					FILE *out;

					if ((out = fopen(argv[++argi], "w")) == NULL)
						warn(WL_fail, "qsim", "Could not open output file \"%a\" for writing.\n", argv[argi]);
					/*
					else
						fclose(stdout), stdout = out;
					*/
				}

				break;
			}
		else
			warn(WL_warn, "qsim", "Unknown argument \"%a\" provided, ignoring.\n", argv[argi]);

	/* define members of certain global variables */
	pthread_mutex_init(&threads_run.mutex, NULL);
	threads_run.value = 1;
	pthread_mutex_init(&C_stepsahead.mutex, NULL);
	C_stepsahead.value = 0;
	pthread_mutex_init(&R_discardable.mutex, NULL);
	R_discardable.value = 0;

	/* initialize the experiment structure's contents */
	exp.title[0] = '\0';
	exp.path[0] = '\0';
	exp.delta = (real)0;
	exp.limit = 0;
	exp.system = NULL;
	exp.nobjects = 0;
	exp.frame = NULL;

	if (path == NULL)
		warn(WL_warn, "qsim", "No path to experiment file provided; use (-f|--file) followed by the path to an experiment file.\n");
	else
	if (!readexp(path, &exp))
		warn(WL_fail, "qsim", "Could not load experiment file \"%a\" properly.\n", path);
	else
	if (!initexp(&exp))
		warn(WL_fail, "qsim", "Could not initialize experiment.\n");
	else
	if ((pthreadr = pthread_create(&compiler_thread, NULL, compiler, (void *)&exp)))
		warn(WL_fail, "qsim", "Could not create thread for the compiler, (pthread_create) returned %i.\n", pthreadr);
	else
	if ((pthreadr = pthread_create(&renderer_thread, NULL, renderer, (void *)&exp)))
	{
		warn(WL_fail, "qsim", "Could not create thread for the renderer, (pthread_create) returned %i.\n", pthreadr);

		setmutexint(&threads_run, 0);
		pthread_join(compiler_thread, NULL);
	} else {
		warn(WL_verbose, "qsim", "Beginning %a.\n", exp.title);

		struct frame *next;

		warn(WL_verbose, "discarder", "Initialized.\n");

		for (;;)
		{
			pthread_mutex_lock(&R_discardable.mutex);

			if (R_discardable.value == 0)
			/* wait until there are frames to discard */
			{
				pthread_mutex_unlock(&R_discardable.mutex);

				for (;;)
				{
					catchmutex(&renderer_mutex);
					pthread_mutex_lock(&R_discardable.mutex);

					if (R_discardable.value > 0)
						break;

					if (!readmutexint(&threads_run))
						break;

					pthread_mutex_unlock(&R_discardable.mutex);
				}
			}

			if (!readmutexint(&threads_run) && R_discardable.value == 0)
			{
				pthread_mutex_unlock(&R_discardable.mutex);

				break;
			}

			warn(WL_verbose, "discarder", "Discarding %i frame(s).\n", R_discardable.value);

			do {
				next = exp.frame->next;

				free(exp.frame->system);
				free(exp.frame);

				exp.frame = next;
			}
			while (--R_discardable.value);

			if (!readmutexint(&threads_run))
			{
				pthread_mutex_unlock(&R_discardable.mutex);

				break;
			}

			pthread_mutex_unlock(&R_discardable.mutex);
		}

		warn(WL_verbose, "discarder", "Terminated.\n");

		pthread_join(compiler_thread, NULL);
		pthread_join(renderer_thread, NULL);

		r = EXIT_SUCCESS;
	}

	freeexp(&exp);

	pthread_mutex_destroy(&W_mutex);
	pthread_mutex_destroy(&threads_run.mutex);
	pthread_mutex_destroy(&compiler_mutex);
	pthread_mutex_destroy(&C_stepsahead.mutex);
	pthread_mutex_destroy(&renderer_mutex);
	pthread_mutex_destroy(&R_discardable.mutex);

	warn(WL_verbose, "qsim", "Returned %i.\n", r);

	return r;
}
