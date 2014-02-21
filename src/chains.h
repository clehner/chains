#ifndef CHAINS_H
#define CHAINS_H

#define MAX_LINE_LENGTH  512
#define MAX_LINE_WORDS 256
#define N 3
#define DELIMETERS " \n\r\t"

struct markov_model;

// creation

struct markov_model*
mm_new();

// inspection

void
mm_print(struct markov_model *model);

// learning

int
mm_learn_ngram(struct markov_model *model, const char **ngram);

int
mm_learn_sequence(struct markov_model *model, const char **words,
		unsigned int len);

int
mm_learn_sentence(struct markov_model *model, const char *line);

int
mm_learn_file(struct markov_model *model, const char *corpus_path);

// generating

int
mm_pick_ngram(struct markov_model *model, char **ngram, int n, int direction);

int
mm_generate_sequence(struct markov_model *model, int n,
	char *sequence[], int direction);

int
mm_generate_sentence(struct markov_model *model,
	const char **initial_ngram, char *sentence);

// both

int
mm_respond_and_learn(struct markov_model *model, const char line[],
	char *best_response, char learn);

#endif /* CHAINS_H */
