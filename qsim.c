#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "qsim.h"

int arrin(const char *arr, int narr, ...)
{
	va_list ap;
	int i;
	char *cmp;

	va_start(ap, narr);

	for (i = 0; i < narr; ++i)
	{
		cmp = va_arg(ap, char *);

		if (strcmp(arr, cmp) == 0)
		{
			va_end(ap);

			return i;
		}
	}

	va_end(ap);

	return -1;
}

int cin(char c, const char *arr)
{
	int i;

	for (i = 0; arr[i] != '\0' && arr[i] != c; ++i)
		;

	return arr[i] == '\0' ? -1 : i;
}

void warn(int level, const char *name, const char *format, ...)
{
	static const char *levels[] = { "verbose", "info", "warn", "fail", "crash" };

	if (level == WL_verbose && !W_verbose)
		return;

	va_list ap;
	char *arr;
	long double ld;
	int c, x;

	pthread_mutex_lock(&W_mutex);
	va_start(ap, format);

	if (level != -1)
		fprintf(stderr, "(%s) ", levels[level]);

	fprintf(stderr, "%s: ", name);

	while ((c = *format++) != '\0')
		if (c == '%')
			switch ((c = *format++))
			{
			case 'e':
			/* (long double) */
				ld = va_arg(ap, long double);
				fprintf(stderr, "%Le", ld);

				break;

			case 'i':
			/* (int) */
				x = va_arg(ap, int);
				fprintf(stderr, "%d", x);

				break;

			case 'a':
			/* (char *) */
				arr = va_arg(ap, char *);

				while ((c = *arr++) != '\0')
					putc(c, stderr);

				break;

			default:
			/* (char) */
				putc(c, stderr);

				break;
			}
		else
			putc(c, stderr);

	va_end(ap);
	pthread_mutex_unlock(&W_mutex);
}

int readarr(FILE *f, const char *term, char *arr, int arrsize)
{
	int i, c, r;

	/* skip space (tabs, spaces, new-lines, etc.) */
	while (isspace(c = getc(f)))
		;

	for (i = 0; c != EOF && (r = cin(c, term)) == -1; c = getc(f))
		if (i == arrsize - 1)
		{
			while ((c = getc(f)) != EOF && (r = cin(c, term)) == -1)
				;

			break;
		} else
			arr[i++] = c;

	arr[i] = '\0';

	return c == EOF ? -1 : r;
}

vector V_add(vector a, vector b)
{
	vector c;

	c.x = a.x + b.x;
	c.y = a.y + b.y;

	return c;
}

vector V_mul(vector a, real mul)
{
	vector b;

	b.x = a.x * mul;
	b.y = a.y * mul;

	return b;
}

vector V_sub(vector a, vector b)
{
	vector c;

	c.x = a.x - b.x;
	c.y = a.y - b.y;

	return c;
}

vector V_div(vector a, real quo)
{
	vector b;

	b.x = a.x / quo;
	b.y = a.y / quo;

	return b;
}

int readmutexint(struct mutexint *MI)
{
	int value;

	pthread_mutex_lock(&MI->mutex);
	value = MI->value;
	pthread_mutex_unlock(&MI->mutex);

	return value;
}

int setmutexint(struct mutexint *MI, int value)
{
	pthread_mutex_lock(&MI->mutex);
	MI->value = value;
	pthread_mutex_unlock(&MI->mutex);

	return value;
}

int incmutexint(struct mutexint *MI)
{
	int value;

	pthread_mutex_lock(&MI->mutex);
	value = ++MI->value;
	pthread_mutex_unlock(&MI->mutex);

	return value;
}

int decmutexint(struct mutexint *MI)
{
	int value;

	pthread_mutex_lock(&MI->mutex);
	value = --MI->value;
	pthread_mutex_unlock(&MI->mutex);

	return value;
}

#define RD_ARRSIZE  32

int readdatum(FILE *f, const char *term, struct datum *datum)
{
	int c, negative = 0, i, r = -1;
	char arr[RD_ARRSIZE];

	datum->value = (real)0;
	datum->unit = 0;

	while (isspace(c = getc(f)) && (r = cin(c, term)) == -1)
		;

	if (r != -1)
		return r;

	if (c == '-')
		negative = 1, c = getc(f);
	else
	if (c == '+')
		c = getc(f);

	if (isalpha(c))
	/* constant */
	{
		i = 0;

		do
			if ((r = cin(c, term)) != -1)
				break;
			else
			if (i == RD_ARRSIZE - 1)
			{
				while (isalpha(c = getc(f)) && (r = cin(c, term)) == -1)
					;

				break;
			} else
				arr[i++] = tolower(c);
		while (isalpha(c = getc(f)));

		arr[i] = '\0';

		switch (arrin(arr, 5, "e", "pm", "nm", "em", "na"))
		{
		case -1:
			warn(WL_info, "readdatum", "Unknown constant \"%a\" used, assuming value of 0.\n", arr);

			break;

		case 0:  datum->value = (real)EC;              break;
		/* mass related constants; must be defined in (g) */
		case 1:  datum->value = (real)PM * (real)1e3;  break;
		case 2:  datum->value = (real)NM * (real)1e3;  break;
		case 3:  datum->value = (real)EM * (real)1e3;  break;
		case 4:  datum->value = 0;                     break;
		}

		if (negative)
			datum->value *= (real)-1;
	} else {
		int x = 0, unit, j, k;

		if (isdigit(c))
		/* whole component */
			do
				x = (x * 10) + (c - '0');
			while (isdigit(c = getc(f)));

		datum->value = (real)x;

		if (c == '.')
		/* decimal component */
			for (x = 1; isdigit(c = getc(f)); ++x)
				datum->value += (real)(c - '0') * powl((real)0.1, (real)x);

		if (negative)
			datum->value *= (real)-1;

		if (c == 'e')
		/* scientific notation */
		{
			negative = 0;

			if ((c = getc(f)) == '-')
				negative = 1, c = getc(f);
			else
			if (c == '+')
				c = getc(f);
			else
			if (!isdigit(c))
			/* mistook a unit for scientific notation */
			{
				ungetc(c, f);
				c = 'e';

				goto readdatum_unitspec;
			}

			x = 0;

			do
				x = (x * 10) + (c - '0');
			while (isdigit(c = getc(f)));

			if (negative)
				datum->value *= (real)1 / powl((real)10, (real)x);
			else
				datum->value *= powl((real)10, (real)x);
		}

	readdatum_unitspec:
		/* unit specification */

		while (isspace(c))
			c = getc(f);

		for (i = 0; c != EOF && !isspace(c) && (r = cin(c, term)) == -1; c = getc(f))
			if (i < RD_ARRSIZE - 1)
				arr[i++] = c;
			else {
				while (isalpha(c = getc(f)))
					;

				break;
			}

		arr[i] = '\0';

		if (i == 0)
			goto readdatum_end;

		for (unit = 0; unit < datum->nunits; ++unit)
		{
			j = i;
			k = strlen(datum->units[unit]);

			if (k > j)
				continue;

			while (arr[--j] == datum->units[unit][--k] && k != 0)
				;

			if (arr[j] == datum->units[unit][k])
			/* unit found */
			{
				datum->unit = unit;

				if (j == 1)
					switch (arr[0])
					{
					case 'P':  datum->value *= (real)1e+15;  break;
					case 'T':  datum->value *= (real)1e+12;  break;
					case 'G':  datum->value *= (real)1e+9;   break;
					case 'M':  datum->value *= (real)1e+6;   break;
					case 'k':  datum->value *= (real)1e+3;   break;
					case 'h':  datum->value *= (real)1e+2;   break;
					case 'd':  datum->value *= (real)1e-1;   break;
					case 'c':  datum->value *= (real)1e-2;   break;
					case 'm':  datum->value *= (real)1e-3;   break;
					case 'u':  datum->value *= (real)1e-6;   break;
					case 'n':  datum->value *= (real)1e-9;   break;
					case 'p':  datum->value *= (real)1e-12;  break;
					case 'f':  datum->value *= (real)1e-15;  break;
					default:   goto readdatum_invalidmul;    break;
					}
				else
				if (j != 0)
					goto readdatum_invalidmul;

				break;

			readdatum_invalidmul:
				warn(WL_info, "readdatum", "Improper metric multiplier in \"%a\", ignoring but maintaining unit.\n", arr);

				break;
			}
		}

		if (unit == datum->nunits)
		/* unit not found */
			warn(WL_info, "readdatum", "Improper unit used in \"%a\", using default unit of \"%a\" instead.\n", arr, datum->units[0]);
	}
readdatum_end:

	if (r != -1)
		return r;

	while (c != EOF && (r = cin(c, term)) == -1)
		c = getc(f);

	return c == EOF ? -1 : r;
}

int readdata(FILE *f, const char *term, int ndatum, ...)
{
	va_list ap;
	struct datum *datum;
	int i;

	va_start(ap, ndatum);

	while (ndatum--)
	{
		datum = va_arg(ap, struct datum *);

		if ((i = readdatum(f, term, datum)) != 0)
		{
			va_end(ap);

			if (ndatum != 0)
			/* data incomplete */
			{
				warn(WL_warn, "readdata", "Data incomplete; setting remaining data with values of 0.\n");

				while (ndatum--)
				{
					datum = va_arg(ap, struct datum *);
					datum->value = (real)0;
					datum->unit = 1;
				}
			}

			return i;
		}
	}

	va_end(ap);

	return 0;
}

void mkdatum(struct datum *datum, int nunits, ...)
{
	va_list ap;
	const char **units;
	const char *arr;

	units = calloc(nunits, sizeof(const char *));

	if (units == NULL)
		warn(WL_crash, "mkdatum", "calloc returned NULL.\n"),
		exit(EXIT_FAILURE);

	datum->nunits = nunits;
	datum->units = units;

	va_start(ap, nunits);

	while (nunits--)
	{
		arr = va_arg(ap, const char *);
		*units++ = arr;
	}

	va_end(ap);

	return;
}

void freedata(int ndatum, ...)
{
	va_list ap;
	struct datum *datum;

	va_start(ap, ndatum);

	while (ndatum--)
	{
		datum = va_arg(ap, struct datum *);
		free(datum->units);
	}

	va_end(ap);
}

static struct object *mkobject(void)
{
	struct object *object = malloc(sizeof(struct object));

	if (object == NULL)
		warn(WL_crash, "mkobject", "malloc returned NULL.\n"),
		exit(EXIT_FAILURE);

	object->loc = V_make(0,0);
	object->vel = V_make(0,0);
	object->charge = (real)0;
	object->mass = (real)0;

	return object;
}

#define RE_KEYSIZE  32
#define RE_WARNEOF  "Unexpected end of file.\n"

int readexp(const char *path, struct exp *exp)
{
	struct stat statbuf;
	FILE *f;
	int c, x, i, r;
	char key[RE_KEYSIZE];
	struct object *node;
	struct datum time, limit, locx, locy, velx, vely, charge, mass;

	stat(path, &statbuf);

	if (!S_ISREG(statbuf.st_mode)
	 || (f = fopen(path, "r")) == NULL)
	/* check if file is regular, and if it has been opened properly */
		return 0;

	mkdatum(&locx, 1, "m");
	mkdatum(&locy, 1, "m");
	mkdatum(&velx, 1, "m/s");
	mkdatum(&vely, 1, "m/s");
	mkdatum(&charge, 2, "C", "e");
	mkdatum(&mass, 2, "g", "u");
	mkdatum(&time, 1, "s");
	mkdatum(&limit, 1, "fr.");

	strcpy(exp->path, path);

	for (;;)
	{
		while (isspace(c = getc(f)))
			;

		if (c == EOF)
			break;

		if (c == '#')
		{
			while ((c = getc(f)) != '\n' && c != EOF)
				;

			if (c == EOF)
				break;

			continue;
		}

		ungetc(c, f);

		if (readarr(f, ":", key, RE_KEYSIZE) == -1)
		{
			warn(WL_info, "readexp", RE_WARNEOF);

			goto readexp_end;
		}

		switch (arrin(key, 4, "title", "delta", "limit", "system"))
		{
		case -1:
			warn(WL_warn, "readexp", "Key \"%a\" is not known, skipping.\n", key);

			goto readexp_skip;

		case 0:
		/* title */
			if (readarr(f, ";", exp->title, exp_TITLESIZE) == -1)
			{
				warn(WL_info, "readexp", RE_WARNEOF);

				goto readexp_end;
			}

			break;

		case 1:
		/* delta */
			if (readdatum(f, ";", &time) == -1)
			{
				warn(WL_info, "readexp", RE_WARNEOF);

				goto readexp_end;
			}

			if (time.value > 0)
				exp->delta = time.value;
			else
				warn(WL_warn, "readexp", "Delta value is less than zero, discarding.\n");

			break;

		case 2:
		/* limit */
			if (readdatum(f, ";", &limit) == -1)
			{
				warn(WL_warn, "readexp", RE_WARNEOF);

				goto readexp_end;
			}

			x = ceil(limit.value);

			if (x > 0)
				exp->limit = x;
			else
				warn(WL_warn, "readexp", "Tau is not a natural number, or it is zero, discarding.\n");

			break;

		case 3:
		/* system */
			while (isspace(c = getc(f)))
				;

			if (c == EOF)
			{
				warn(WL_info, "readexp", RE_WARNEOF);

				goto readexp_end;
			}

			ungetc(c, f);

			exp->system = mkobject();
			node = exp->system;

			for (exp->nobjects = 1;; ++exp->nobjects)
			{
				x = readdata(f, ",#\n;", 6, &locx, &locy, &velx, &vely, &charge, &mass);

				node->loc.x = locx.value;
				node->loc.y = locy.value;

				node->vel.x = velx.value;
				node->vel.y = vely.value;

				if (charge.unit == 1)
				/* e -> C */
					charge.value *= EC;

				node->charge = charge.value;

				if (mass.unit == 1)
				/* u -> kg */
					mass.value *= AMU;
				else
				/* g -> kg */
					mass.value *= 1e-3;

				node->mass = mass.value;

				if (x == 0)
				{
					warn(WL_info, "readexp", "Too much data provided, skipping excess data.\n");

					while ((c = getc(f)) != EOF && (x = cin(c, "#\n;")) == -1)
						;

					if (c == EOF)
						x = -1;
					else
						++x;
				}

				if (x == 3)
				/* `;' reached */
					break;

				switch (x)
				{
				case -1:
				/* EOF reached */
					warn(WL_info, "readexp", RE_WARNEOF);
					node->next = NULL;

					goto readexp_end;

				case 1:
				/* `#' reached */
					while ((c = getc(f)) != '\n' && c != EOF)
						;

					if (c == EOF)
					{
						warn(WL_info, "readexp", RE_WARNEOF);
						node->next = NULL;

						goto readexp_end;
					}

					node->next = mkobject();
					node = node->next;

					break;

				case 2:
				/* `\n' reached */
					node->next = mkobject();
					node = node->next;

					break;
				}
			}

			node->next = NULL;

			break;
		}

		continue;

	readexp_skip:

		while ((c = getc(f)) != ';' && c != EOF)
			if (c == '#')
				while ((c = getc(f)) != '\n' && c != EOF)
					;

		if (c == EOF)
			break;

		continue;
	}
readexp_end:

	r = 1;

	if (exp->system == NULL)
		warn(WL_fail, "readexp", "System is empty.\n"), r = 0;

	if (exp->title[0] == '\0')
		warn(WL_fail, "readexp", "Delta is unset.\n"), r = 0;

	if (exp->delta == (real)0)
		warn(WL_fail, "readexp", "Delta is unset.\n"), r = 0;

	if (exp->limit == 0)
		warn(WL_fail, "readexp", "Limit is unset.\n"), r = 0;

	return r;
}

void freeexp(struct exp *exp)
{
	struct object *node, *nextn;
	struct frame *frame, *nextf;

	for (node = exp->system; node != NULL; node = nextn)
		nextn = node->next, free(node);

	for (frame = exp->frame; frame != NULL; frame = nextf)
	{
		nextf = frame->next;
		free(frame->system);
		free(frame);
	}
}

int initexp(struct exp *exp)
{
	struct frame *frame;

	if ((frame = malloc(sizeof(struct frame))) == NULL
	 || (frame->system = calloc(exp->nobjects, sizeof(struct snapshot))) == NULL)
	{
		warn(WL_crash, "initexp", "(malloc|calloc) returned NULL after attempting to allocate memory for a frame.\n");

		return 0;
	}

	int i;
	struct object *o;

	for (i = 0, o = exp->system; i < exp->nobjects; ++i, o = o->next)
		frame->system[i].loc = o->loc, frame->system[i].vel = o->vel;

	exp->frame = frame;

	return 1;
}

static int mkframe(struct exp *exp, struct frame *frame)
{
	struct frame *next;
	int i, j;
	struct object *o, *p;
	vector radius;
	real rsquared;

	if ((next = malloc(sizeof(struct frame))) == NULL
	 || (next->system = calloc(exp->nobjects, sizeof(struct snapshot))) == NULL)
	{
		warn(WL_crash, "mkframe", "(malloc|calloc) returned NULL when attempting allocation of frame.\n");

		return 0;
	}

	frame->next = next;

	/* routine */
	for (i = 0, o = exp->system; i < exp->nobjects; ++i, o = o->next)
	{
		/* initialize felec and fgrav vectors */
		frame->system[i].felec = V_make(0,0);
		frame->system[i].fgrav = V_make(0,0);

		for (j = 0, p = exp->system; j < exp->nobjects; ++j, p = p->next)
		{
			if (j == i)
			/* do not calculate force on an object from itself */
				continue;

			/* The radius vector is a line connecting object (o) to object
			 * (p), with the direction from object (o) towards (p). */
			radius = V_sub(frame->system[j].loc, frame->system[i].loc);
			rsquared = powl(V_get(radius), 2);

			/* Electrostatic force is a repulsive force (on like-charges), thus (V_sub). */
			frame->system[i].felec = V_sub(frame->system[i].felec, V_set(radius, p->charge / rsquared));
			/* Gravitational force is an attractive force, thus (V_add). */
			frame->system[i].fgrav = V_add(frame->system[i].fgrav, V_set(radius, p->mass / rsquared));
		}

		frame->system[i].felec = V_mul(frame->system[i].felec, K * o->charge);
		frame->system[i].fgrav = V_mul(frame->system[i].fgrav, G * o->mass);
		frame->system[i].acc = V_div(V_add(frame->system[i].felec, frame->system[i].fgrav), o->mass);

		next->system[i].vel = V_add(frame->system[i].vel, V_mul(frame->system[i].acc, exp->delta));
		next->system[i].loc = V_add(frame->system[i].loc, V_mul(next->system[i].vel, exp->delta));
	}

	return 1;
}

void *compiler(void *arg)
{
	struct exp *exp = (struct exp *)arg;
	struct frame *frame;

	warn(WL_verbose, "compiler", "Initialized.\n");

	for (frame = exp->frame; readmutexint(&threads_run); frame = frame->next)
	{
		pthread_mutex_lock(&compiler_mutex);

		if (!mkframe(exp, frame))
		{
			warn(WL_verbose, "compiler", "Got error in (mkframe); sending signals to stop.\n");
			setmutexint(&threads_run, 0);
			pthread_mutex_unlock(&compiler_mutex);

			break;
		}

		incmutexint(&C_stepsahead);
		pthread_mutex_unlock(&compiler_mutex);
	}

	warn(WL_verbose, "compiler", "Terminated.\n");

	return NULL;
}

void *renderer(void *arg)
{
	struct exp *exp = (struct exp *)arg;
	struct frame *frame, *next;
	int catchup = 0, i = 0, j;

	warn(WL_verbose, "renderer", "Initialized.\n");

	for (frame = exp->frame; readmutexint(&threads_run); frame = next)
	{
		pthread_mutex_lock(&renderer_mutex);
		pthread_mutex_lock(&C_stepsahead.mutex);

		if (catchup)
		{
			if (C_stepsahead.value < 3)
			/* caught up */
			{
				warn(WL_verbose, "renderer", "Caught up to compiler, unlocking it.\n");
				catchup = 0;
				pthread_mutex_unlock(&compiler_mutex);
			}
		} else
		if (C_stepsahead.value == R_toofar)
		/* start catching up */
		{
			warn(WL_verbose, "renderer", "Too far behind compiler, locking it.\n");
			catchup = 1;
			pthread_mutex_lock(&compiler_mutex);
		} else
		if (C_stepsahead.value < 2)
		/* wait for compiler */
		{
			warn(WL_verbose, "renderer", "No frame prepared yet, waiting for compiler.\n");
			pthread_mutex_unlock(&C_stepsahead.mutex);

			for (;;)
			{
				/* catch end of a compile loop */
				catchmutex(&compiler_mutex);
				pthread_mutex_lock(&C_stepsahead.mutex);

				if (C_stepsahead.value >= 2)
					break;

				pthread_mutex_unlock(&C_stepsahead.mutex);
			}

			warn(WL_verbose, "renderer", "Frame prepared; rendering.\n");
		}

		pthread_mutex_unlock(&C_stepsahead.mutex);

		if (++i > exp->limit)
		{
			warn(WL_verbose, "renderer", "Limit of %i reached; breaking from loop and sending signals to stop.\n", exp->limit);
			setmutexint(&threads_run, 0);
			pthread_mutex_unlock(&renderer_mutex);

			break;
		}

		printf("frame %d:\n", i);

		for (j = 0; j < exp->nobjects; ++j)
			printf("\t" "object %d:\n"
			       "\t\t" "felec: (%Le, %Le)\n"
			       "\t\t" "fgrav: (%Le, %Le)\n"
			       "\t\t" "acc: (%Le, %Le)\n"
			       "\t\t" "vel: (%Le, %Le)\n"
			       "\t\t" "loc: (%Le, %Le)\n",
			    j, frame->system[j].felec.x, frame->system[j].felec.y,
			       frame->system[j].fgrav.x, frame->system[j].fgrav.y,
			       frame->system[j].acc.x, frame->system[j].acc.y,
			       frame->system[j].vel.x, frame->system[j].vel.y,
			       frame->system[j].loc.x, frame->system[j].loc.y);

		decmutexint(&C_stepsahead);
		incmutexint(&R_discardable);

		/* assume the discarding of this frame is instant */
		next = frame->next;

		pthread_mutex_unlock(&renderer_mutex);
	}
rend_main_end:

	if (catchup)
	{
		warn(WL_verbose, "renderer", "Was attempting to catch up to compiler before termination, unlocking it now so it may exit naturally.\n");
		catchup = 0;
		pthread_mutex_unlock(&compiler_mutex);
	}

	warn(WL_verbose, "renderer", "Terminated.\n");

	return NULL;
}
