#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "chains.h"

int
main (int argc, char *argv[]) {
	int i;
	char c;

	struct markov_model *model;
	char line[MAX_LINE_LENGTH];
	char response[MAX_LINE_LENGTH];
	char dump_table = 0;
	char opt_learn = 0;
	char *corpus_path = NULL;
	unsigned int seed = 0;

	for(i = 1; i < argc; i++) {
		c = argv[i][1];
		if(argv[i][0] != '-' || argv[i][2]) {
			c = -1;
		}
		switch(c) {
			case 'd':
				dump_table = 1;
				break;
			case 's':
				if(++i < argc) seed = strtoul(argv[i], NULL, 10);
				break;
			case 'l':
				// learn from queries
				opt_learn = 1;
				break;
			case 'f':
				if(++i < argc) corpus_path = argv[i];
				break;
			case 'v':
				fprintf(stderr, "chains, © 2014 Charles Lehner\n");
				return 1;
			case 'h':
			default:
				fprintf(stderr, "Usage: chains [-v] [-d] [-n] [-f corpus_file]\n");
				return 1;
		}
	}

	if (!seed) {
		// Default to seeding with current time
		srand(time(NULL));
	} else if (seed != 1) {
		// Special case: don't seed
		srand(seed);
	}

	model = mm_new();
	if (!model) {
		fprintf(stderr, "Unable to create markov model\n");
		return 1;
	}

	if (corpus_path) {
		if (!mm_learn_file(model, corpus_path)) {
			fprintf(stderr, "Unable to learn corpus\n");
		}
	}

	// Print the data structure
	if (dump_table) {
		mm_print(model);
		return 0;
	}

	// Respond to queries
	while (!feof(stdin)) {
		// Receive a message
		if (!fgets(line, sizeof(line), stdin)) {
			break;
		}

		// Respond to the message
		response[0] = '\0';
		if (!mm_respond_and_learn(model, line, response, opt_learn)) {
			fprintf(stderr, "Failed to respond\n");
		} else {
			printf("%s\n", response);
			fflush(stdout);
		}
	}
}

