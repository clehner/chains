#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "hash/hash.h"
#include "chains.h"

const char *word_sentinel = "";
const char *sentinel_sentence[] = {"", NULL};

struct gram {
	int value;
	hash_t *next; // map words to gram structs
};

struct markov_model {
	struct gram *forward;
	struct gram *backward;
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
	}
}

int
learn_ngram(struct gram *stats, const char **ngram, int direction) {
	int i;
	struct gram *substat;
	const char *word;

	// Increment total count of 1-grams
	stats->value++;

	// Add the 1-gram, then the 2-gram, etc, up to N
	for (i = 0; i < N; i++) {

		// Select the word depending on the direction
		word = ngram[direction > 0 ? i : N - i - 1];

		// Lookup by the next word
		substat = stats->next ? hash_get(stats->next, (char *)word) : NULL;

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

			hash_set(stats->next, (char *)word, substat);
		}

		// Increment the count for this i-gram
		substat->value++;

		// Proceed to the next word-level
		stats = substat;
	}

	return 1;
}

int
mm_learn_ngram(struct markov_model *model, const char **ngram) {
	return
		learn_ngram(model->forward, ngram, 1) &&
		learn_ngram(model->backward, ngram, -1);
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
		words[i] = (char *)word_sentinel;
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
		words[i++] = (char *)word_sentinel;
	}

	return i;
}

// Learn a sequence of n words
int
mm_learn_sequence(struct markov_model *model, const char **words,
		unsigned int len) {
	// Learn each ngram in the sequence
	for (unsigned int i = 0; i <= len - N; i++) {
		if (!mm_learn_ngram(model, &words[i])) return 0;
	}
	return 1;
}

// Learn a sentence.
int
mm_learn_sentence(struct markov_model *model, const char *line) {
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
	if (!mm_learn_sequence(model, (const char **)words, num_words)) {
		fprintf(stderr, "Failed to learn a sentence\n");
		return 0;
	}

	return 1;
}

void *
gram_pick(struct gram *stats) {
	unsigned int index_sum = 0;
	unsigned int index_pick = rand() % stats->value;

	hash_each(stats->next, {
		// Increment the running total
		index_sum += ((struct gram *)val)->value;
		// Check if we are at the desired index
		if (index_sum > index_pick) {
			// Found our word
			// printf("found word %s\n", word);
			return (char *)key;
		}
	});

	return NULL;
}

// Pick a word using the markov model of order n,
// starting from a (n-1)-gram,
// going in positive or negative direction.
// Write the word to the n-th place in ngram.
// Returns the number of words written (up to n).
int
mm_pick_ngram(struct markov_model *model, char **ngram, int n, int direction) {
	int i;
	int num_words = 0;
	const char *word;
	struct gram *stats, *substats;

	stats = direction > 0 ? model->forward : model->backward;

	// Walk the tree to get to the distribution for this prefix
	for (i = 0; i < n-1; i++) {
		word = ngram[i];
		if (!word) {
			// We don't have a word here, so we need to fill it up
			word = ngram[i] = gram_pick(stats);
			num_words++;
			// printf("Make it up %d: \"%s\"\n", i, word);
		}
		substats = hash_get(stats->next, (char *)word);
		if (!substats) {
			// The model does not include this prefix.
			// Abort picking
			// fprintf(stderr, "Model does not have this prefix\n");
			return 0;
		} else {
			stats = substats;
		}
	}

	// Walk the tree with weighted random, to finish the ngram
	for (; i < n; i++) {
		// printf("%d\n", i);

		// Pick an index within the probability distribution, for the desired word
		// picker_index_sum = 0;
		// printf("pick: %d/%d\n", picker_index_pick, stats->value);

		if (!(ngram[i] = (char *)gram_pick(stats))) {
			fprintf(stderr, "Unable to pick ngram\n");
			return 0;
		}
		num_words++;
	}

	return num_words;
}

// Generate a response sequence from the given markov model,
// starting from the ngram given in sequence
// Writes words to sequence, and the length of the sequence to seq_len.
int
mm_generate_sequence(struct markov_model *model, int n,
	char *sequence[], int direction) {

	int i;
	int sentinels_seen = 0;
	int num_words;

	for (i = 0; i < MAX_LINE_WORDS - n; i++) {

		// printf("Prefix: ");
		// print_ngram((const char **)&sequence[i], n-1);

		num_words = mm_pick_ngram(model, &sequence[i], n, direction);
		if (!num_words) {
			// fprintf(stderr, "Failed to pick ngram\n");
			break;
		};
		// printf("word %d (%d): %s\n", i, num_words, sequence[i+n-1]);

		// Keep track of how many consecutive sentinels we have seen
		if (sequence[i+num_words] == word_sentinel) {
			sentinels_seen++;
			if (sentinels_seen >= n-2) {
				// We've reached a sentence end
				// printf("end\n");
				i += n;
				break;
			}
		} else {
			sentinels_seen = 0;
		}
	}
	if (i >= MAX_LINE_WORDS) {
		i = MAX_LINE_WORDS;
		fprintf(stderr, "A sequence was trimmed.\n");
	} else {
		// printf("Sequence end.\n");
	}
	// printf("Sequence: ");
	// print_ngram((const char **)sequence, i);
	return i;
}

// Calculate the length of all the strings in an array, with spaces between
// them.
size_t
sequence_strlen(char *sequence[], int seq_len) {
	size_t len = 0;
	for (int i = 0; i < seq_len; i++) {
		if (sequence[i] != word_sentinel) {
			len += strlen(sequence[i]) + 1;
		}
	}
	return len;
}

void
sequence_print(char *sequence[], int len) {
	for (int i = 0; i < len; i++) {
		if (sequence[i] != word_sentinel) {
			printf("\"%s\" ", sequence[i]);
		}
	}
	printf("\n");
}

// Concatenate a sequence into a string dest, up to max_len chars.
// Words in the sequence are seperated by spaces, and terminated with \0
// Direction can be 1 for forward or -1 for reverse.
void
sequence_concat(char *dest, char *sequence[], int max_len,
		int seq_len, int direction) {
	int word_len;
	int sentence_i = 0;

	// Concatenate the words into a sentence
	for (int i = 0; i < seq_len; i++) {
		char *word = sequence[i * direction];
		// printf("Word %d: \"%s\"\n", i*direction, word);
		if (word == word_sentinel) {
			// Skip sentinels marking the beginning and end of the sequence
			continue;
		}
		word_len = strlen(word);
		if (sentence_i + word_len + 1 > max_len) {
			// Can't fit this word. End the sentence here.
			fprintf(stderr, "A sentence was trimmed.\n");
			dest[sentence_i] = '\0';
			return;
		}
		strncpy(&dest[sentence_i], word, word_len);
		sentence_i += word_len + 1;
		dest[sentence_i-1] = ' ';
	}
	dest[sentence_i ? sentence_i-1 : 0] = '\0';
}

// Generate a sentence using the markov model, given an initial seed ngram
// Writes to sentence
int
mm_generate_sentence(struct markov_model *model, const char **initial_ngram, char *sentence) {
	char *seq_forward[MAX_LINE_WORDS];
	char *seq_backward[MAX_LINE_WORDS];
	int seq_skip = 0;
	int seq_forward_len;
	int seq_backward_len;
	unsigned int seq_backward_strlen;
	char generate_forward = 1;
	char generate_backward = 1;
	int i;

	sentence[0] = '\0';

	// printf("Initial ngram: ");
	// print_ngram(initial_ngram, N-1);

	// If the initial ngram ends with a sentinel, only generate a back-sequence
	if (initial_ngram[N-2] == word_sentinel) {
		generate_forward = 0;
	}

	// If the intial ngram starts with a sentinel, only generate a forward sequence.
	if (initial_ngram[0] == word_sentinel) {
		generate_backward = 0;
	}

	if (generate_forward) {

		// Start with the (n-1)-gram prefix
		for (i = 0; i < N-1; i++) {
			seq_forward[i] = (char *)initial_ngram[i];
		}

		// Generate a sequence from the initial ngram to a sentence end
		seq_forward_len = mm_generate_sequence(model, N, seq_forward, 1);

		// printf("Forward sequence (%d): ", seq_forward_len);
		// sequence_print(seq_forward, seq_forward_len);
	}

	if (generate_backward) {

		// Start with the (n-1)-gram prefix, reversed
		// or start with the initial ngram in the forward sequence, because it
		// may have been extended
		for (i = 0; i < N-1; i++) {
			seq_backward[i] =
				(generate_forward && seq_forward_len ?
					seq_forward :
					(char **)initial_ngram
				)[N-i-2];
		}

		// Generate a reverse sequence from the initial ngram to a sentence start
		seq_backward_len = mm_generate_sequence(model, N, seq_backward, -1);
		seq_backward_strlen = sequence_strlen(seq_backward, seq_backward_len);

		// printf("Reverse sequence (%d, %d): ", seq_backward_len, seq_backward_strlen);
		// sequence_print(seq_backward, seq_backward_len);

		if (seq_backward_len) {
			sequence_concat(sentence, seq_backward + seq_backward_len - 1,
					MAX_LINE_LENGTH, seq_backward_len, -1);
		}
	} else {
		seq_backward_strlen = 0;
	}

	// printf("Sentence: %s\n", sentence);
	if (generate_forward && generate_backward && seq_backward_strlen) {
		// Skip the overlapping ngram
		seq_skip = N-1;
		seq_forward_len -= seq_skip;

		// Add space in between forward and backward parts of the sentence
		sentence[seq_backward_strlen - 1] = ' ';
	}

	if (generate_forward) {
		sequence_concat(sentence + seq_backward_strlen, seq_forward + seq_skip, MAX_LINE_LENGTH, seq_forward_len, 1);
	}

	if (!generate_forward && !generate_backward) {
		return 0;
	}

	// printf("Sentence: %s\n", sentence);
	return 1;
}

int
score_response(char *response) {
	return response ? strlen(response) : 0;
}

// Generate a response to a sentence. Optionally learn the input.
int
mm_respond_and_learn(struct markov_model *model, const char line[], char *best_response, char learn) {
	unsigned int line_len = strlen(line) + 1;
	unsigned int num_tokens;
	int num_words;
	char *line_copy;
	char *words[MAX_LINE_WORDS];
	char *initial_ngram[N];
	char response[MAX_LINE_LENGTH];
	int score, best_score = ~1;

	// Copy the line because it is going to be tokenized and stored in the model
	line_copy = malloc(line_len * sizeof(char));
	if (!line_copy) {
		perror("malloc");
		return 0;
	}
	strncpy(line_copy, line, line_len);

	// Tokenize the sentence
	num_tokens = tokenize_sentence(line_copy, words);

	// Learn the input sequence
	if (learn && !mm_learn_sequence(model, (const char **)words, num_tokens)) {
		fprintf(stderr, "Failed to learn a sequence\n");
		return 0;
	}

	// Ignore the sentinel padding in the sequence
	num_words = num_tokens - 2*(N-1);
	// words = &words[2];
	// printf("Num words: %d\n", num_words);

	// Try to generate a response based on N-grams from the input. If that does
	// not yield anything useful, try (N-1)-grams, and so on, until we hit 0,
	// and then just pick a random sentence not based on the input.
	for (int m = (num_words < N ? num_words : N); m > 0; m--) {
		// printf("m=%d\n", m);

		// Generate a candidate response for each m-gram in the input
		for (int i = 0; i <= num_words-m; i++) {
			// printf("i: %d, m: %d, num_words-m: %d\n", i, m, num_words-m);

			// Copy the m-gram, with null termination
			for (int j = 0; j < m; j++) {
				initial_ngram[j] = words[i+j+N-1];
				// printf("Ngram %d: %s\n", j+N-1, initial_ngram[j]);
			}
			initial_ngram[m] = NULL;

			// Generate a candidate response from this ngram
			if (mm_generate_sentence(model, (const char **)initial_ngram, response)) {
				score = score_response(response);
				// printf("Score %d: %s\n", score, response);
				if (score > best_score) {
					best_score = score;
					strncpy(best_response, response, MAX_LINE_LENGTH);
				}
				// printf("\n");
			}
		}

		// If we get a good response, we're done.
		if (best_score > 0) {
			return 1;
		}
	}

	// Last resort: generate a sentence from sentinels
	return mm_generate_sentence(model, sentinel_sentence, best_response);
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
mm_learn_file(struct markov_model *model, const char *corpus_path) {
	FILE *corpus_file;
	char line[MAX_LINE_LENGTH];

	// Read the corpus file
	corpus_file = fopen(corpus_path, "r");
	if (!corpus_file) {
		perror("fopen");
		return 0;
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
	return 1;
}
