#ifndef BSDJP_PREDICT_H
#define BSDJP_PREDICT_H

/*
 * Prediction engine: suggests completions based on
 * user dictionary history and dictionary prefix matches.
 */

void predict_init(void);
void predict_cleanup(void);

/* Get prediction candidates for a partial reading.
 * Returns allocated strings in candidates array. */
int predict_candidates(const char *partial_reading,
                       char **candidates, int max);

#endif /* BSDJP_PREDICT_H */
