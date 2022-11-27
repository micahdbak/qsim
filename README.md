# qsim: A Charged Particle Simulator

## Overview

This project simulates the many forces imposed on charged, massed objects within a specified system using Coulomb's Law of electromagnetic attraction, Newton's law of universal gravitation, and Newtonian kinematics.

### Terminology

* 'Object' refers to something with location, velocity, charge, and mass.
* 'System' refers to a group of objects.
* 'Experiment' refers to the information about how a system is to be simulated and information about the system itself.
  - 'Delta'-time refers to the length in time that a system is allowed to move, per-say, before forces are recalculated.
* 'Routine' refers to the way that forces are calculated, and then applied to objects.

## Installation

### Dependencies

This program is writtin in C, so a C compiler is needed. It then depends on the C math library, which is linked by `-lm`, and the POSIX multithreading library, linked by `-lpthreads`. If you are compiling this on a UNIX machine, such as MacOS, Linux, or *BSD, these should be provided for. Otherwise, seek documentation on installing these libraries. `make` is used for compiling the program. To install `make`:

MacOS / Brew: `brew install make`

Debian-based distributions: `apt install make`

And so on and so forth.

### Download and Compile

* ```git clone https://github.com/micahdbak/qsim```
* ```cd qsim```
* ```make qsim```

## Running The Program

Arguments are passed into `qsim` with a dash and a letter corresponding to a specific option. For example, to enable verbosity (informational output about what the program is doing), you pass `-v` into `qsim`: `qsim -v`. As a baseline, you must provide an experiment file into `qsim`. This is done by specifying the file after the option `-f`. I.e., `qsim -f <experiment-file>`.

### Options

- `-v`: Enable verbose output.
- `-f <experiment-file>`: Specify the experiment file.

### Output

As frames are simulated, they will be output to `stdout` by the following syntax:

```
frame [n]:
	object [n]:
		felec: (x, y)
		fgrav: (x, y)
		acc: (x, y)
		vel: (x, y)
		loc: (x, y)
	object [n + 1]:
		felec: (x, y)
		fgrav: (x, y)
		acc: (x, y)
		vel: (x, y)
		loc: (x, y)
```

The different components of this are as follows:
- `felec`: Electric force vector (Newtons) imposed on the object.
- `fgrav`: Gravitational force vector (Newtons) imposed on the object.
- `acc`: Acceleration vector.
- `vel`: Velocity vector.
- `loc`: Location vector, relative to an arbitrary (0,0).

Verbose, errors, and warnings are all output to `stderr`. So to save the output of an experiment to a file for later reference, simply redirect the program's `stdout` to the file of your choosing: `qsim [options] > output`

## Experiment Files

This program requires an experiment file. Below is an example experiment file, that contains comments describing each part.

[example.exp](example.exp)
```
# By beginning a line with a '#',  you create a comment.
# Comments are ignored by the program, and are for the user's convenience.

# All data about the experiment file follows this syntax: data-key: data-value;
# Where 'data-key' names what data is being defined, and 'data-value' refers
#     to the content of this data.
# For data-values that are numbers, there is a lot of freedom.
# A number can be described using the following syntax, where parenthesis
#     surround mandatory components, curly braces surround characters that
#     separate components, and square braces surround optional components.
#   [sign](whole){.}[decimal]{e or E}[power] [prefix](unit)
# The prefix component is a single character directly preceding a unit that
#     multiplies the data-value following the metric prefix multipliers:
#   - P, "peta": 10^15
#   - T, "tera": 10^12
#   - G, "giga": 10^9
#   - M, "mega": 10^6
#   - k, "kilo": 10^3
#   - h, "hecto": 10^2
#   - d, "deci": 10^-1
#   - c, "centi": 10^-2
#   - m, "milli": 10^-3
#   - u, "micro": 10^-6
#   - n, "nano": 10^-9
#   - p, "pico": 10^-12
#   - f, "femto": 10^-15
# Examples:
#   -1.01e7 ms
#   +1.02e-5Ps
#   1e100 fs
#   ...
# Units differ depending on the data-key that you are entering a value for.

# title: sets the experiment's title. No unit, this is a text value.
title: Example Experiment;

# delta: sets the delta-time for this experiment. Unit: 's', seconds.
delta: 1ms;

# limit: the number of frames that will be simulated. Unit: 'fr.', frames.
# A frame is a moment in time that contains information about objects'
#     locations, velocities, accelerations, etc..
# Every frame differs in time with respect to the delta-time.
limit: 10fr.;

# system: lists the objects in this experiment and their starting values.
# Every line corresponds to an object.
# Each member of a line, separated by a comma, refers to a specific piece
#     of information about that object.
# The system is terminated by the last line ending with a semicolon.
# The members are ordered as follows:
#   location-x, location-y, velocity-x, velocity-y,   charge,     mass
#   'm'         'm'         'm/s'       'm/s'         'e', or 'C' 'g'
# The units are metre, metres per second, either the elementary charge or
#     Coulombs, and grams, respectively.
# For charge and mass, constants are available for specific applications.
# For charge:
#   - e, "elementary charge": EC in qsim.h.
#   - na, "not applicable": 0.
# For mass:
#   - pm, "proton mass": PM in qsim.h.
#   - nm, "neutron mass": NM in qsim.h.
#   - em, "electron mass": EM in qsim.h.
#   - na, "not applicable": 0.
# After the first colon, a new line should be entered.
system:
-1m, 0m, 0m/s, 0m/s, +e, pm  # Proton
1m,  0m, 0m/s, 0m/s, -e, em; # Electron
```

## Routine Design

This program simulates frames by getting the net force on an object relative to its position, then allowing it to accelerate towards a new position for a certain period of time. This period of time is the previously discussed delta-time. The program then repeats this process until the frame limit is reached. Because this design is very simple, it does lend to inaccuracies. No matter the delta-time at which an object is allowed to move before its net force is recalculated, the delta-time cannot reach an infinitesimal value which is ideal. However, as long as delta remains very small, the margin of error should not be too great.

### Steps

The first step is that of acquiring an object's net force. The way that this is done is by a vector sum of its electromagnetic force and its gravitational force, which are calculated using Coulomb's law of electromagnetic attraction and Newton's law of universal gravitation, respectively, against the entire system of objects.

For the second step, this net force is used to get the acceleration of an object, then the velocity, then the location. If we let $F$ be the net force on an object, $a$ be its acceleration, $v$ be its velocity, $l$ be its location, $n$ be the current frame, and $\delta$ be the delta-time, then acceleration is given with respect to the equation $F = ma$, such that $a = {F \over m}$. Velocity, then, is given by $v_{n + 1} = v_{n} + a_{n} \delta$. Lastly, location is given by $l_{n + 1} = l_{n} + v_{n}\delta$.
