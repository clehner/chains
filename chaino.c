#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "hash/hash.h"

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
	int value;
	hash_t *next; // map words to gram structs
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

// Print the whole data structure
int
gram_print_prefixed(char *word, struct gram *stats, char *prefix) {
	char *subprefix;
	int subprefix_len;

	// Concat the parent words prefix with the current word
	if (prefix) {
		subprefix_len = strlen(prefix) + 6 + strlen(word);
		subprefix = malloc(subprefix_len * sizeof(char));
		if (!subprefix) {
			perror("malloc");
			return 0;
		}
		snprintf(subprefix, subprefix_len, "    %s %s", prefix, word);
	} else {
		subprefix = word;
	}

	printf("%s (%d)\n", subprefix, stats->value);

	if (stats->next) {
		hash_each(stats->next, {
			gram_print_prefixed((char *)key, val, subprefix);
		});
	}

	if (prefix) {
		free(subprefix);
	}
	return 1;
}

// Print the whole data structure
void
gram_print(struct gram *stats) {
	gram_print_prefixed("", stats, NULL);
}

void
print_ngram(const char *ngram[], int n) {
	int i;
	for (i = 0; i < n; i++) {
		printf("\"%s\"%s", ngram[i], i < n-1 ? " " : "\n");
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
		substat = stats->next ? hash_get(stats->next, word) : NULL;

		// If the tree does not reach this word, add a new node
		if (!substat) {

			substat = gram_new();
			if (!substat) {
				fprintf(stderr, "Failed to create a gram\n");
				return 0;
			}
			if (stats->next == NULL) {
				// Create a new hashmap here
				stats->next = hash_new();
				if (!stats->next) {
					return 0;
				}
			}

			hash_set(stats->next, word, substat);
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

// Tokenize a sentence and write the words of it to the given array.
// Alters the line string argument.
// Return the number of words written to the array
unsigned int
tokenize_sentence(char *line, char **words) {
	char *word;
	unsigned int i;

	if (!line) return 0;

	// Start with N-1 empty words,
	// to signify the beginning of a sentence
	for (i = 0; i < N-1; i++) {
		words[i] = word_sentinel;
	}

	// Split the sentence into words
	for (word = strtok(line, DELIMETERS); word; word = strtok(NULL, DELIMETERS)) {
		// Add the current word
		words[i++] = word;
	}

	// Pad the end of the sentence with empty words,
	// to signify the end of a sentence.
	for (int j = 0; j < N-1; j++) {
		// Add the sentinel
		words[i++] = word_sentinel;
	}

	return i;
}

// typedef int(*ngram_enum_func)(char **ngram, void *obj);

// Learn a sequence of n words
int
mm_learn_sequence(struct markov_model *model, char *words[], unsigned int len) {
	// Learn each ngram in the sequence
	for (int i = 0; i <= len - N; i++) {
		if (!mm_learn_ngram(model, &words[i])) return 0;
	}
	return 1;
}

// Learn a sentence.
int
mm_learn_sentence(struct markov_model *model, char *line) {
	char *words[MAX_LINE_WORDS];
	unsigned int num_words;

	// Copy the line so we can safely store its words in the model
	unsigned int line_len = strlen(line) + 1;
	char *line_copy = malloc(line_len * sizeof(char));
	if (!line_copy) {
		perror("malloc");
		return 0;
	}
	strncpy(line_copy, line, line_len);

	// Split the sentence into words
	num_words = tokenize_sentence(line_copy, words);

	// Learn the sequence of words
	if (!mm_learn_sequence(model, words, num_words)) {
		fprintf(stderr, "Failed to learn a sentence\n");
		return 0;
	}
}

// Pick a word using the markov model of order n,
// starting from a (n-1)-gram,
// going in positive or negative direction.
// Write the word to the n-th place in ngram
// Return 1 if a word found, 0 if not.
int
mm_pick_ngram(struct markov_model *model, char **ngram, unsigned int n, int direction) {
	int i;
	const char *word;
	struct gram *stats, *substats;

	unsigned int picker_index_pick;
	unsigned int picker_index_sum;

	stats = direction > 0 ? model->forward : model->backward;

	// Walk the tree to get to the distribution for this prefix
	for (i = 0; i < n-1; i++) {
		word = ngram[i];
		if (!word) {
			// We don't have a word here, so we need to fill it up
			return 0;
			break;
		}
		substats = hash_get(stats->next, (char *)word);
		if (!substats) {
			// The model does not include this prefix.
			// Abort picking
			return 0;
		} else {
			stats = substats;
		}
	}

	// Walk the tree with weighted random, to finish the ngram
	for (; i < n; i++) {
		// printf("%d\n", i);

		// Pick an index within the probability distribution, for the desired word
		picker_index_pick = rand() % stats->value;
		picker_index_sum = 0;
		// printf("pick: %d/%d\n", picker_index_pick, stats->value);

		hash_each(stats->next, {
			// Increment the running total
			picker_index_sum += ((struct gram *)val)->value;
			// Check if we are at the desired index
			if (picker_index_sum > picker_index_pick) {
				// Found our word
				// printf("found word %s\n", word);
				//return (char *)key;
				ngram[i] = (char *)key;
				return 1;
			}
		});

		// todo: all returning multiple words
		printf("No word\n");
		return 0;

		// printf("word: %s\n", picker.word);
		// if (!picker_word) {
			// printf("no word\n");
			// return 0;
		// }
		// printf("Got %s\n", picker.word);
	}

	return 0;
}

// Generate a response sequence from the given markov model,
// starting from an ngram of order n.
// Writes words to sequence, and length of the sequence to seq_len
void
mm_generate_sequence(struct markov_model *model,
	const char *ngram[], int n,
	char *sequence[], unsigned int *seq_len) {

	int i;
	char *word;
	int sentinels_seen = 0;

	// Start with the (n-1)-gram prefix
	for (i = 0; i < n; i++) {
		sequence[i] = (char *)ngram[i];
	}

	// printf("word0 %d: %s\n", i, sequence[0], "");
	for (i = 0; i < MAX_LINE_WORDS; i++) {

		// printf("Prefix: ");
		// print_ngram((const char **)&sequence[i], n-1);

		if (!mm_pick_ngram(model, &sequence[i], n, 1)) break;
		// printf("word %d: %s\n", i, sequence[i], "");

		// Keep track of how many consecutive sentinels we have seen
		if (sequence[i+1] == word_sentinel) {
			sentinels_seen++;
			if (sentinels_seen >= n-1) {
				// We've reached a sentence end
				break;
			}
		} else {
			sentinels_seen = 0;
		}
	}
	if (i >= MAX_LINE_WORDS) {
		fprintf(stderr, "A sequence was trimmed.\n");
	} else {
		// printf("Sequence end.\n");
	}
	*seq_len = i;
}

// Generate a sentence using the markov model, given an initial seed ngram
// Writes to sentence
int
mm_generate_sentence(struct markov_model *model, const char **initial_ngram, char *sentence) {
	int word_len;
	char *ngram[N];
	unsigned int seq_len;
	char *sequence[MAX_LINE_WORDS];
	int sentence_i = 0;
	//char *response_words[MAX_LINE_WORDS];

	// printf("Initial ngram: ");
	// print_ngram(initial_ngram, N-1);

	// Generate the sequence
	mm_generate_sequence(model, initial_ngram, N, sequence, &seq_len);
	// printf("len: %d\n", seq_len);

	// Concatenate the words into a sentence
	for (int i = 0; i < seq_len; i++) {
		if (sequence[i] == word_sentinel) {
			// Skip sentinels marking the beginning and end of the sequence
			continue;
		}
		word_len = strlen(sequence[i]);
		if (sentence_i + word_len + 1 > MAX_LINE_LENGTH) {
			// Can't fit this word. End the sentence here.
			fprintf(stderr, "A sentence was trimmed.\n");
			sentence[sentence_i] = '\0';
			return 1;
		}
		strncpy(&sentence[sentence_i], sequence[i], word_len);
		sentence_i += word_len + 1;
		sentence[sentence_i-1] = ' ';
	}
	sentence[sentence_i ? sentence_i-1 : 0] = '\0';

	// Generate the sentence after the seed
	// Generate the sentence before the seed

	// printf("Sentence: %s\n", sentence);

	return 1;
}

int
score_response(char *response) {
	return response ? strlen(response) : 0;
}

// Learn a sentence, and generate a response to it.
int
respond_and_learn(struct markov_model *model, const char line[], char *best_response) {
	unsigned int line_len = strlen(line) + 1;
	unsigned int num_words;
	char *line_copy;
	char *word;
	char *words[MAX_LINE_WORDS];
	char response[MAX_LINE_LENGTH];
	int score, best_score = ~1;
	int i;
	// char *sentinels[] = {word_sentinel, word_sentinel, word_sentinel};

	// Copy the line because it is going to be tokenized and stored in the model
	line_copy = malloc(line_len * sizeof(char));
	if (!line_copy) {
		perror("malloc");
		return 0;
	}
	strncpy(line_copy, line, line_len);

	// Tokenize the sentence
	num_words = tokenize_sentence(line_copy, words);

	// Learn the input sequence
	if (!mm_learn_sequence(model, words, num_words)) {
		fprintf(stderr, "Failed to learn a sequence\n");
		return 0;
	}

	// Generate a candidate response for each (n-1)-gram in the input

	// printf("Num words: %d\n", num_words);

	for (int i = 1; i <= num_words-N; i++) {
		// printf("Got gram: ");
		// print_ngram(ngram);

		// Generate a candidate response from this ngram
		if (mm_generate_sentence(model, (const char **)&words[i], response)) {
			score = score_response(response);
			if (score > best_score) {
				best_score = score;
				strncpy(best_response, response, MAX_LINE_LENGTH);
			}
		}
	}

	if (0) {
		fprintf(stderr, "Failed to pick ngrams");
		return 0;
	}

	// If we didn't get a decent sentence,
	// generate one from start sentinels
	if (best_score <= 0) {
		fprintf(stderr, "[next case]\n");
		//mm_generate_sentence(model, sentinels, response);
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
	model->forward->next = hash_new();

	// Create the ngram model for backward lookups
	model->backward = gram_new();
	if (!model->backward) {
		free(model);
		return NULL;
	}
	model->backward->next = hash_new();

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
	unsigned int seed = 0;

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
			case 's':
				if(++i < argc) seed = strtoul(argv[i], NULL, 10);
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

	if (!seed) {
		// Default to seeding with current time
		srand(time(NULL));
	} else if (seed != 1) {
		// Special case: don't seed
		srand(seed);
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
		// printf("Line: \"%s\"\n", line);
		// Respond to the message
		response[0] = '\0';
		if (!respond_and_learn(model, line, response)) {
			fprintf(stderr, "Failed to respond\n");
		} else {
			printf("%s\n", response);
		}
	}
}
