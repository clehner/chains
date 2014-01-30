#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "strmap.h"

#define MAX_LINE_LENGTH  512
#define N 3
#define HASHMAP_CAPACITY 100
#define DELIMETERS " \n\r\t"

char *todo_response = "TODO";
char *word_sentinel = "";

struct gram {
	char *word;
	int value;
	StrMap *next; // map following words to gram structs
};

struct gram *
gram_new() {
	struct gram *stats = malloc(sizeof (struct gram));
	if (!stats) {
		perror("malloc");
		return NULL;
	}
	memset(stats, 0, sizeof (struct gram));
	return stats;
}

void gram_print_iter(const char *key, void *value, const void *obj);

// Print the whole data structure
void
gram_print_prefixed(struct gram *stats, char *prefix) {
	char *subprefix;
	int prefix_len;

	// Concat the parent words prefix with the current word
	if (prefix) {
		prefix_len = strlen(prefix);
		subprefix = malloc((prefix_len + 2 + strlen(stats->word)) *
				sizeof(char));
		strncpy(subprefix, prefix, prefix_len);
		subprefix[prefix_len] = ' ';
		strcpy(subprefix + prefix_len + 1, stats->word);
	} else {
		subprefix = stats->word;
	}

	printf("\"%s\" (%d)\n", subprefix, stats->value);

	if (stats->next) {
		sm_enum(stats->next, gram_print_iter, subprefix);
	}
}

void
gram_print_iter(const char *key, void *value, const void *obj) {
	gram_print_prefixed((struct gram *) value, (char *) obj);
}

// Print the whole data structure
void
gram_print(struct gram *stats) {
	gram_print_prefixed(stats, NULL);
}

void
print_ngram(char **ngram) {
	int i;
	for (i = 0; i < N; i++) {
		printf("%s%s", ngram[i], i < N-1 ? " " : "\n");
	}
}

int
learn_ngram(struct gram *stats, char **ngram) {
	int i;
	struct gram *substat;

	// Increment total count of 1-grams
	stats->value++;

	// Add the 1-gram, then the 2-gram, etc, up to N
	for (i = 0; i < N; i++) {
		// Lookup by the next word
		substat = sm_get(stats->next, ngram[i]);

		// If the tree does not reach this word, add a new node
		if (!substat) {

			substat = gram_new();
			if (!substat) {
				fprintf(stderr, "Failed to create a gram\n");
				return 0;
			}
			substat->word = ngram[i];
			if (stats->next == NULL) {
				// Create a new hashmap here
				stats->next = sm_new(HASHMAP_CAPACITY);
				if (!stats->next) {
					return 0;
				}
			}

			if (!sm_put(stats->next, ngram[i], substat)) {
				fprintf(stderr, "Unable to put a word\n");
				return 0;
			}

		}

		// Increment the count for this i-gram
		substat->value++;

		// Proceed to the next word-level
		stats = substat;
	}

	return 1;
}

// Learn a sentence.
// Alters the line string argument
int
learn_sentence(struct gram *stats, char *line) {
	char *word;
	char *new_word;
	char *ngram[N];
	unsigned int i, word_len;

	if (!line) return 0;

	// Pad the ngram with empty words,
	// to signify the beginning of a sentence
	for (i = 0; i < N; i++) {
		ngram[i] = word_sentinel;
	}

	// Split the sentence into words
	for (word = strtok(line, DELIMETERS); word; word = strtok(NULL, DELIMETERS)) {
		// Organize the words into ngrams
		// Move back the previous words
		for (i = 1; i < N; i++) {
			ngram[i-1] = ngram[i];
		}

		// Allocate space for the new word
		word_len = strlen(word) + 1;
		new_word = malloc(word_len * sizeof(char));
		if (new_word == NULL) {
			fprintf(stderr, "Unable to copy a word\n");
		}
		strncpy(new_word, word, word_len);

		// Add the current word
		ngram[N-1] = new_word;

		// Learn the ngram
		if (!learn_ngram(stats, ngram)) {
			fprintf(stderr, "Failed to learn an n-gram\n");
		}
	}
	return 1;
}

// Learn a sentence, and generate a response to it.
int
respond_and_learn(struct gram *stats, char *line, char *response) {
	strcpy(response, todo_response);
	return 1;
}

int
main (int argc, char *argv[]) {
	int i;
	char c;

	struct gram *stats;
	char *corpus_path = NULL;
	FILE *corpus_file;
	char line[MAX_LINE_LENGTH];
	char response[MAX_LINE_LENGTH];
	int dump_table = 0;

	for(i = 1; i < argc; i++) {
		c = argv[i][1];
		if(argv[i][0] != '-' || argv[i][2]) {
			if (!corpus_path) {
				corpus_path = argv[i];
			} else {
				c = -1;
			}
		}
		switch(c) {
			case 'd':
				dump_table = 1;
				break;
			case 'v':
				fprintf(stderr, "chaino, © 2014 Charles Lehner\n");
				return 1;
		}
	}
	if (!corpus_path || c == -1) {
		fprintf(stderr, "Usage: chaino [-v] [-d] corpus_file\n");
		return 1;
	}

	// Read the corpus file
	corpus_file = fopen(corpus_path, "r");
	if (!corpus_file) {
		perror("fopen");
		return 1;
	}

	stats = gram_new();
	// Special top-level gram
	stats->word = "";
	stats->next = sm_new(HASHMAP_CAPACITY);

	// Each line of the corpus is a sentence.
	// Read each line.
	while (!feof(corpus_file)) {
		// Get the line
		if (!fgets(line, sizeof(line), corpus_file)) {
			break;
		}
		// Learn the ngram frequencies of the sentence
		if (!learn_sentence(stats, line)) {
			fprintf(stderr, "Failed to learn a sentence\n");
			// Attempt no recovery
			break;
		}
	}
	fclose(corpus_file);

	// Print the data structure
	if (dump_table) {
		gram_print_top(stats);
		return 0;
	}

	// Respond to queries
	while (!feof(stdin)) {
		// Receive a message
		if (!fgets(line, sizeof(line), stdin)) {
			break;
		}
		// Respond to the message
		if (!respond_and_learn(stats, line, response)) {
			fprintf(stderr, "Failed to respond\n");
		} else {
			printf("Response: %s\n", response);
		}
	}
}
