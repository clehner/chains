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

struct markov_model {
	struct gram *forward;
	struct gram *backward;
};

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
		subprefix = malloc((prefix_len + 6 + strlen(stats->word)) *
				sizeof(char));
		strncpy(subprefix, "    ", 4);
		strncpy(subprefix+4, prefix, prefix_len);
		subprefix[prefix_len+4] = ' ';
		strcpy(subprefix + prefix_len + 5, stats->word);
	} else {
		subprefix = stats->word;
	}

	printf("%s (%d)\n", subprefix, stats->value);

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
learn_ngram(struct gram *stats, char **ngram, int direction) {
	int i;
	struct gram *substat;
	char *word;

	// Increment total count of 1-grams
	stats->value++;

	// Add the 1-gram, then the 2-gram, etc, up to N
	for (i = 0; i < N; i++) {

		// Select the word depending on the direction
		word = ngram[direction > 0 ? i : N - i - 1];

		// Lookup by the next word
		substat = sm_get(stats->next, word);

		// If the tree does not reach this word, add a new node
		if (!substat) {

			substat = gram_new();
			if (!substat) {
				fprintf(stderr, "Failed to create a gram\n");
				return 0;
			}
			substat->word = word;
			if (stats->next == NULL) {
				// Create a new hashmap here
				stats->next = sm_new(HASHMAP_CAPACITY);
				if (!stats->next) {
					return 0;
				}
			}

			if (!sm_put(stats->next, word, substat)) {
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

int
mm_learn_ngram(struct markov_model *model, char **ngram) {
	return
		learn_ngram(model->forward, ngram, 1) &&
		learn_ngram(model->backward, ngram, -1);
}

// Learn a sentence.
// Alters the line string argument
int
mm_learn_sentence(struct markov_model *model, char *line) {
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
			return 0;
		}
		strncpy(new_word, word, word_len);

		// Add the current word
		ngram[N-1] = new_word;

		// Learn the ngram
		if (!mm_learn_ngram(model, ngram)) {
			fprintf(stderr, "Failed to learn an n-gram\n");
		}
	}

	// Pad the end of the sentence with empty words,
	// to signify the end of a sentence.

	// Unless the sentence was empty: then the end padding
	// collapses with the start padding.
	if (!new_word) {
		// Learn one blank ngram
		if (!mm_learn_ngram(model, ngram)) {
			fprintf(stderr, "Failed to learn an n-gram\n");
		}

	} else for (i = N-1; i > 0; i--) {
		int j;

		// Move back the previous words
		for (j = 1; j < N; j++) {
			ngram[j-1] = ngram[j];
		}

		// Add the sentinel
		ngram[N-1] = word_sentinel;

		// Learn the ngram
		if (!mm_learn_ngram(model, ngram)) {
			fprintf(stderr, "Failed to learn an n-gram\n");
		}
	}

	return 1;
}

// Learn a sentence, and generate a response to it.
int
respond_and_learn(struct markov_model *model, char *line, char *response) {
	strcpy(response, todo_response);
	return 1;
}

struct markov_model*
mm_new() {
	struct markov_model *model = malloc(sizeof (struct markov_model));
	if (!model) return NULL;

	// Create the ngram model for forward lookups
	model->forward = gram_new();
	if (!model->forward) {
		free(model);
		return NULL;
	}
	model->forward->word = "";
	model->forward->next = sm_new(HASHMAP_CAPACITY);

	// Create the ngram model for backward lookups
	model->backward = gram_new();
	if (!model->backward) {
		free(model);
		return NULL;
	}
	model->backward->word = "";
	model->backward->next = sm_new(HASHMAP_CAPACITY);

	return model;
}

void
mm_print(struct markov_model *model) {
	printf("FORWARD\n");
	gram_print(model->forward);
	printf("BACKWARD\n");
	gram_print(model->backward);
}

int
main (int argc, char *argv[]) {
	int i;
	char c;

	struct markov_model *model;
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

	model = mm_new();
	if (!model) {
		fprintf(stderr, "Unable to create markov model\n");
		return 1;
	}

	// Each line of the corpus is a sentence.
	// Read each line.
	while (!feof(corpus_file)) {
		// Get the line
		if (!fgets(line, sizeof(line), corpus_file)) {
			break;
		}
		// Learn the ngram frequencies of the sentence
		if (!mm_learn_sentence(model, line)) {
			fprintf(stderr, "Failed to learn a sentence\n");
			// Attempt no recovery
			break;
		}
	}
	fclose(corpus_file);

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
		if (!respond_and_learn(model, line, response)) {
			fprintf(stderr, "Failed to respond\n");
		} else {
			printf("Response: %s\n", response);
		}
	}
}
