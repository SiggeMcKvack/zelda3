#ifndef CONFIG_READER_INTERNAL_H
#define CONFIG_READER_INTERNAL_H

/*
 * Internal functions for config_reader.c
 * Exposed for unit testing when TEST_BUILD is defined.
 *
 * These functions use their original names (not prefixed) to avoid
 * changing the implementation. The header just declares them non-static.
 */

#ifdef TEST_BUILD

/* Trim leading and trailing whitespace from string (in-place) */
char* trim_whitespace(char *str);

/* Parse boolean value ("1", "true", "True" -> 1, else 0) */
int parse_bool(const char *value);

/* Parse integer value (wraps atoi) */
int parse_int(const char *value);

/* Parse string value (returns strdup'd copy or NULL) */
char* parse_string(const char *value);

/* Update string variable (free old, set new) */
void update_string(char **dest, const char *value);

/* Parse aspect ratio string, returns enum value and sets custom w/h if needed */
int parse_aspect_ratio(const char *value, int *custom_w, int *custom_h);

/* Parse output method string to enum value */
int parse_output_method(const char *value);

#endif /* TEST_BUILD */

#endif /* CONFIG_READER_INTERNAL_H */
