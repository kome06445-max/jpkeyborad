#ifndef BSDJP_USER_DICT_H
#define BSDJP_USER_DICT_H

/*
 * User dictionary: records conversion history for learning.
 * Entries are stored as (reading, candidate, frequency) tuples.
 * Persisted to ~/.bsdjp/user_dict.txt on cleanup.
 */

void user_dict_init(void);
void user_dict_cleanup(void);

/* Record that the user selected this candidate for the given reading */
void user_dict_record(const char *reading, const char *candidate);

/* Look up user dictionary entries for a reading, sorted by frequency desc */
int user_dict_lookup(const char *reading, char **candidates, int max);

#endif /* BSDJP_USER_DICT_H */
