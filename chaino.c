#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "strmap.h"

#define MAX_LINE_LENGTH  512
#define MAX_LINE_WORDS 256
#define N 3
#define HASHMAP_CAPACITY 100
#define DELIMETERS " \n\r\t"

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

int gram_print_iter(const char *key, void *value, void *obj);

// Print the whole data structure
int
gram_print_prefixed(char *word, struct gram *stats, char *prefix) {
	char *subprefix;
	int subprefix_len;

	// Concat the parent words prefix with the current word
	if (prefix) {
		subprefix_len = strlen(prefix) + 6 + strlen(word);
		subprefix = malloc(subprefix_len * sizeof(char));
		snprintf(subprefix, subprefix_len, "    %s %s", prefix, word);
	} else {
		subprefix = word;
	}

	printf("%s (%d)\n", subprefix, stats->value);

	if (stats->next) {
		sm_enum(stats->next, (sm_enum_func)gram_print_prefixed, subprefix);
	}
	return 1;
}

// Print the whole data structure
void
gram_print(struct gram *stats) {
	gram_print_prefixed("", stats, NULL);
}

void
print_ngram(char **ngram) {
	int i;
	for (i = 0; i < N; i++) {
		printf("\"%s\"%s", ngram[i], i < N-1 ? " " : "\n");
		//printf("%s%s", ngram[i], i < N-1 ? " " : "\n");
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

// Learn an ngram. Passed from the tokenizer
int
mm_learn_ngram_iter (char **ngram, void *obj) {
	if (!mm_learn_ngram((struct markov_model *) obj, ngram)) {
		fprintf(stderr, "Failed to learn an n-gram\n");
		return 0;
	}
	return 1;
}

typedef int(*ngram_enum_func)(char **ngram, void *obj);

// Tokenize a sentence and execute the callback with each ngram.
// Alters the line string argument.
// obj is client-specified, and passed to the callback
int
tokenize_sentence(char *line, ngram_enum_func ngram_callback, void *obj) {
	char *word;
	char *new_word;
	char *ngram[N];
	unsigned int i, word_len = 0;

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

		// Emit the ngram
		if (!ngram_callback(ngram, obj)) {
			return 1;
		}
	}

	// Pad the end of the sentence with empty words,
	// to signify the end of a sentence.

	// Unless the sentence was empty: then the end padding
	// collapses with the start padding.
	if (word_len == 0) {
		// Emit one blank ngram
		if (!ngram_callback(ngram, obj)) {
			return 1;
		}

	} else for (i = N-1; i > 0; i--) {
		int j;

		// Move back the previous words
		for (j = 1; j < N; j++) {
			ngram[j-1] = ngram[j];
		}

		// Add the sentinel
		ngram[N-1] = word_sentinel;

		// Emit the ngram
		if (!ngram_callback(ngram, obj)) {
			return 1;
		}
	}

	return 1;
}

// Learn a sentence.
// Alters the line string argument
int
mm_learn_sentence(struct markov_model *model, char *line) {
	if (!tokenize_sentence(line, mm_learn_ngram_iter, model)) {
		fprintf(stderr, "Failed to learn a sentence\n");
		return 0;
	}
}

struct word_pick {
	unsigned int index_pick;
	unsigned int index_sum;
	char *word;
};

int
gram_pick_iter(const char *word, struct gram *stats, struct word_pick *picker) {
	// Increment the running total
	picker->index_sum += stats->value;
	// Check if we are at the desired index
	if (picker->index_sum > picker->index_pick) {
		// Found our word
		picker->word = stats->word;
		return 0;
	}
	return 1;
}

// Pick an ngram using the markov model, starting with a (n-1)-gram,
// going in positive or negative direction
int
mm_pick_ngram(struct markov_model *model, char **ngram, int direction) {
	int i;
	char *word;
	struct gram *stats, *substats;
	struct word_pick picker = {0};

	stats = direction > 0 ? model->forward : model->backward;

	// Walk the tree to get to the distribution for this prefix
	for (i = 0; i < N-1; i++) {
		word = ngram[i];
		if (!word) {
			// We don't have a word here, so we need to fill it up
			break;
		}
		substats = sm_get(stats->next, word);
		if (!substats) {
			// The model does not include this prefix.
			// Stop here and pick the remaining words using weighted random
			break;
		} else {
			ngram[i] = word;
			stats = substats;
		}
	}

	// Pick an index within the probability distribution, for the desired word
	picker.index_pick = rand() % stats->value;
	// printf("pick: %d/%d\n", picker.index_pick, stats->value);

	// Walk the tree with weighted random, to finish the ngram
	for (; i < N; i++) {
		sm_enum(stats->next, (sm_enum_func)gram_pick_iter, &picker);
		if (!picker.word) {
			return 0;
		}
		ngram[i] = picker.word;
		// printf("Got %s\n", picker.word);
	}

	// print_ngram(ngram);

	return 1;
}

struct sentence_pick {
	char *best_response;
	int best_response_score;
	struct markov_model *model;
};

// Generate a sentence using the markov model, given an initial seed ngram
// Writes to sentence
int
mm_generate_sentence(struct markov_model *model, char **initial_ngram, char *sentence) {
	int i;
	int ngram_len;
	char *sentence_word = sentence;
	char *ngram[N];
	//char *response_words[MAX_LINE_WORDS];

	// Generate the sentence after the seed
	// Generate the sentence before the seed
	// Return the combined sentence
	// printf("ngram: ");
	// print_ngram(initial_ngram);
	//strcpy(sentence, "hi");

	// Start the ngram
	for (i = 1; i < N; i++) {
		ngram[i-1] = initial_ngram[i];
	}
	ngram[N-1] = NULL;
	
	printf("initial ngram: ");
	print_ngram(ngram);

	while (ngram[0] && ngram[0][0]) {

		// Pick the next ngram
		if (!mm_pick_ngram(model, ngram, 1)) {
			return 0;
		}

		// printf("picked %s\n", ngram[N-1]);
		ngram_len = strlen(ngram[N-1]);

		//response_words[j++] = ngram[N-1];
		strncpy(sentence_word, ngram[N-1], ngram_len);
		sentence_word += ngram_len + 1;
		sentence_word[-1] = ' ';

		// Move back the previous words
		for (i = 1; i < N; i++) {
			ngram[i-1] = ngram[i];
		}

	}

	sentence_word[-1] = '\0';
	printf("sentence: %s\n", sentence);

	return 1;
}

int
score_response(char *response) {
	return strlen(response);
}

// Read an ngram from an input sentence, and generate a sentence using it
int
mm_respond_ngram_iter(char **ngram, void *obj) {
	struct sentence_pick *picker = obj;
	char response[MAX_LINE_LENGTH];
	int score;

	// Generate a candidate response
	if (!mm_generate_sentence(picker->model, ngram, response)) {
		return 0;
	}

	score = score_response(response);
	if (score > picker->best_response_score) {
		picker->best_response_score = score;
		strncpy(picker->best_response, response, MAX_LINE_LENGTH);
	}

	return 1;
}

// Learn a sentence, and generate a response to it.
int
respond_and_learn(struct markov_model *model, char *line, char *response) {
	int line_len = strlen(line);
	char *line_copy;
	char *word;
	struct sentence_pick picker = {
		.model = model,
		.best_response = response,
		.best_response_score = ~1
	};

	line_copy = malloc(line_len * sizeof(char));
	if (!line_copy) return 0;
	strncpy(line_copy, line, line_len);

	// Tokenize the sentence and generate candidate responses
	if (!tokenize_sentence(line, mm_respond_ngram_iter, &picker)) {
		fprintf(stderr, "Failed to pick ngrams");
		return 0;
	}

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
