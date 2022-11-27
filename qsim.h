/* ------------------------
 * qsim:   A Charged Particle Simulator
 *         By Micah Baker
 * ------------------------
 * Completed: 2022/02/04
 * Uploaded:  2022/11/27
 * ------------------------
 */

/*
 * constants, types and structures
 */

/* from a Texus-Instruments-36X Pro calculator */
#define K    (8987551787)   /* Coulomb's Constant */
#define G    (6.67428e-11)  /* Universal Gravitation Constant */
/* from <https://physlink.com/reference/physicalconstants.cfm> */
#define EC   (1.60217646e-19)  /* Elementary Charge (C) */
#define AMU  (1.66053873e-27)  /* Atomic Mass Unit (kg) */
#define PM   (1.67262158e-27)  /* Proton Mass (kg) */
#define NM   (1.67492716e-27)  /* Neutron Mass (kg) */
#define EM   (9.10938188e-31)  /* Electron Mass (kg) */

/* real type */
typedef long double real;

/* vector type */
typedef struct vector
{
	real x, y;
} vector;

/* mutual-exclusion-integer type */
struct mutexint
{
	pthread_mutex_t mutex;
	int value;
};

/* datum structure */
struct datum
{
	real value;
	const char **units;
	int nunits, unit;
};

/* experiment structure */

#define exp_TITLESIZE  256
#define exp_PATHSIZE   1024

struct exp
{
	char title[exp_TITLESIZE], path[exp_PATHSIZE];
	real delta;
	int limit;

	/* system: linked-list of object structures */
	struct object
	{
		/* data ordered as is read from file */
		vector loc, vel;
		real charge, mass;
		struct object *next;
	} *system;
	int nobjects;

	/* frame-stream: linked-list of frame structures */
	struct frame
	{
		/* system: array of snapshot structure with size (nobjects) */
		struct snapshot
		{
			vector felec, fgrav, acc, vel, loc;
		} *system;
		struct frame *next;
	} *frame;
};


/*
 * variables
 */

/* for use in (warn) */
extern int             W_verbose;
extern pthread_mutex_t W_mutex;

/* for use in all threads */
extern struct mutexint threads_run;
extern pthread_mutex_t compiler_mutex;
extern struct mutexint C_stepsahead;
extern pthread_mutex_t renderer_mutex;
#define                R_toofar  8
extern struct mutexint R_discardable;

/*
 * functions
 */

/* arrin: Return the index that (arr) is found in the sequence of arrays following (narr), -1 if not found.
 * cin: Return the index that (c) is found in the character array (arr), -1 if not found.
 */
int arrin(const char *arr, int narr, ...);
int cin(char c, const char *arr);

/* warn: Print a warning prefixed with the warning level specified by (level), also prefixed with the name of
 * where the warning is coming from, followed by the formatted text in (format) that may include variables /
 * useful information regarding the warning.
 *
 * Inside (format), the following sequences of characters will be
 * substituted by their corresponding argument of equal type.
 *
 * %e - scientific notation of a (long double); uses fprintf
 * %i - integer output of an (int); uses fprintf
 * %a - character array of type (char *)
 */
/* warning levels */
#define WL_none     -1  /* name: format            */
#define WL_verbose  0   /* (verbose) name: format  */
#define WL_info     1   /* (info) name: format     */
#define WL_warn     2   /* (warn) name: format     */
#define WL_fail     3   /* (fail) name: format     */
#define WL_crash    4   /* (crash) name: format    */

void warn(int level, const char *name, const char *format, ...);

/* readarr: Read into (arr) from (f) until a character from (term) is found, or until (arrsize) is reached.
 * catchmutex: Hold if (_mutex) is locked, then unlock it once it has been locked.
 */
int readarr(FILE *f, const char *term, char *arr, int arrsize);
#define catchmutex(_mutex)  pthread_mutex_lock(_mutex), pthread_mutex_unlock(_mutex)

/* V_add: Return the vector sum of vectors (a) and (b).
 * V_mul: Return the vector resulting from the vector (a) times the multiplicand (mul).
 * V_sub: Return the vector difference of vectors (a) and (b).
 * V_div: Return the vector resulting from the vector (a) divided by the quotient (quo).
 * V_get: Return the magnitude of the vector (_a).
 * V_set: Return a vector with the same direction as (_a), but with the magnitude of (_mag).
 * V_make: Return a vector with the x-component (_x), and the y-component (_y).
 */
vector V_add(vector a, vector b);
vector V_mul(vector a, real mul);
vector V_sub(vector a, vector b);
vector V_div(vector a, real quo);
#define V_get(_a)        (real)sqrt(powl((_a).x, 2) + powl((_a).y, 2))
#define V_set(_a, _mag)  V_mul((_a), (real)(_mag) / V_get((_a)))
#define V_make(_x, _y)   (vector){(real)(_x), (real)(_y)}

/* readmutexint: Return the integer value of (MI).
 * setmutexint: Return the integer value of (MI) after it has been set to (value).
 * incmutexint: Return the integer value of (MI) after it has been incremented once.
 * decmutexint: Return the integer value of (MI) after it has been decremented once.
 */
int readmutexint(struct mutexint *MI);
int setmutexint(struct mutexint *MI, int value);
int incmutexint(struct mutexint *MI);
int decmutexint(struct mutexint *MI);

/* readdatum: Read a real number from file (f) into datum structure (datum). The number read will have its
 * unit converted using the standard metric multipliers, aswell as the index of its unit, in an array of
 * permitted units stored in the datum structure (datum), stored in the datum structure (datum); if the unit
 * read from the file (f) is not contained in the units declared in (datum), then the unit will be assumed
 * as 0; the default unit of which the caller was expecting. It is assumed that the beginning of the datum
 * has already been reached, and the datum will be read until a character contained in (term) is reached.
 * Syntax for a datum is as follows,
 *
 * (sign)(whole component).(decimal component)e(scientific notation)(unit)
 * eg.,
 * 1, -1, +1, 1.0, -1.000e0, +1m, 1 m, -1e000cm, +1e000 cm, 1.000mm, -1.000 mm, etc.
 *
 * No component of the datum is necessary. If the whole component is excluded, then the value of 0 is assumed;
 * that is, preceeding any other provided components. If no component is provided, and it begins with alpha-
 * betical characters, then the datum will be assumed as a constant. The following constants are available
 * for use:
 *
 * note: All constants are case-insensitive.
 * `e': The elementary charge, or electron charge.
 * `pm': The mass of a proton.
 * `nm': The mass of a neutron.
 * `em': The mass of an electron. 
 *
 * (readdatum) will return the terminating character found from (f) in term, or -1 if EOF is reached.
 */
int readdatum(FILE *f, const char *term, struct datum *datum);

/* readdata: Read a sequence of data; see above for the syntax for an individual datum. The first character in
 * (term) will be used as the separation character, and the rest will be used as terminating characters. The
 * index of the terminating character found from (f) in (term) will be returned, or -1 if EOF is reached.
 */
int readdata(FILE *f, const char *term, int ndatum, ...);

/* mkdatum: Make a datum structure with the units following (nunits) stored as permissable units.
 * freedata: Free (ndatum) data following (ndatum).
 */
void mkdatum(struct datum *datum, int nunits, ...);
void freedata(int ndatum, ...);

/* readexp: Read an experiment file of which the path to is found in (path). An experiment file follows a `key
 * value' syntax, where a key is associated with a variable within the experiment structure; the value of which
 * the key leads to will be equal to the value set in the `key value' pair. The keys `title', `delta' and
 * `limit' follow this syntax:
 *
 * title: Example Experiment;
 * delta: 1ms;    # unit is `s'; seconds
 * limit: 30 fr.; # unit is `fr.'; frames
 *
 * `delta', and `limit' are data which values follow the syntax from the function (readdatum). A key begins at
 * an alphabetical character, and ends at the first instance of a `:'; after any space (single spaces, tabs,
 * or new lines) following the `:', a value will be read until the first instance of a `;' character, which
 * terminates a given `key value' pair. Comments may be placed before and after `key value' pairs to provide
 * information and context to the reader. ie.,
 *
 * # this comment is before a `key value' pair.
 * key: value; # this comment is after a `key value' pair.
 * # this comment is also after a `key value' pair.
 *
 * A comment begins at a `#' character, and ends at a `new line' character. The `system' key is used to specify
 * all objects that an experiment contains, and must follow a strict syntax of 6 data per line; each datum
 * corresponding to an object's location's x-component & y-component, an object's velocity's x-component & y-
 * component, an object's charge, and an object's mass. The first object may begin directly after the `:'
 * character following `system', or on the next line. Every line is an object, and the final object's line must
 * be terminated by a `;', whereas all objects' lines prior must end with a `new line'. Also, there may be
 * comments at the end of each object, separating individual objects; this will not cause errors. ie.,
 *
 * # each line must contain this data:
 * # loc-x, loc-y,  vel-x, vel-y,  charge,  mass
 * # the units for each datum are:
 * # m,     m,      m/s,   m/s,    C or e,  g or u
 * # if a unit is not specified, the default is assumed; the default unit for charge is the Coulomb (C),
 * # and the default unit for mass is the gram (g).
 *
 * system:
 * 0m, 0m,  0m/s, 0m/s,  +1e,  MP  # a proton, located at (0,0) with speed (0,0).
 * 1m, 1m,  0m/s, 0m/s,  -1e,  ME; # an electron, located at (1,1) with speed (0,0).
 *
 * Each datum is separated by a `,', and each object is separated by a new-line. The final object ends with
 * a `;' which terminates the system.
 * (readexp) will return 0 on failure, and 1 on success.
 */
int readexp(const char *path, struct exp *exp);

/* freeexp: Free an experiment structure.
 * initexp: Initialize an experiment structure, preparing it for use in the compiler and renderer.
 */
void freeexp(struct exp *exp);
int initexp(struct exp *exp);

/* compiler: Compile frames for an experiment structure.
 * renderer: Render frames compiled by the compiler, then mark them as discardable.
 */
void *compiler(void *);
void *renderer(void *);
