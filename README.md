# chains

Word-level Markov chains in C.

Train a trigram Markov model and use it to generate text.

## Installation

with [clib](https://github.com/clibs/clib/):

	clib install clehner/chains

via git:

	git clone https://github.com/clehner/chains.git
	cd chains
	make install

## Usage

	chains [options]

### Options:

- `-v` Output program usage.
- `-h` Output help information.
- `-f corpus_file` Read the given file and learn each line.
- `-d` Output the markov model data structure.
- `-s seed_num` Seed the random number generator.
- `-l` Learn each query before responding to it.

`chains` will read the corpus file(s) given with the `-f` options, and then
interactively respond to queries on standard I/O. Responses are generated that
include an n-gram from the query, or include a smaller sequence if the model
does not include any n-grams from the query.

### Example:

The user's queries and the program's responses alternate line by
line, until the user ends the execution by pressing `^D`.

    $ chains -f LICENSE
	> GNU
	of the GNU Lesser 
	> You may 
	You may convey a covered work under sections 3 and 4 of this License.
	> 
	If the Library that is part of the GNU GPL.

Upon receiving an empty line as a query, `chains` will pick an n-gram from the
model to start the sequence.

The license isn't the best example since it produces incomplete sentences, as
the file is line-wrapped. For a more interesting example, try `chaino` with an IRC
channel log.

## API

`chains` can be used as a library. See the header file.

## Todo

- Allow changing N, the order of the model.
- Improve space efficiency.
- Use Katz's back-off, Kneser-Ney smoothing, or something.
- Allow looking up the probability of a sequence based on the model.
- Document the API.
