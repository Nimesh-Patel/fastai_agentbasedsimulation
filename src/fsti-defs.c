#include <stdio.h>
#include <glib.h>

#include "utils/utils.h"
#include "fsti-error.h"
#include "fsti-defs.h"


#define SET_DEST(type) do {                      \
        type d = s;                              \
        if(FSTI_GET_TYPE(d) == dest_type) {      \
            memcpy(dest, &d, sizeof(d));          \
            return;                              \
        }                                        \
    } while(0)

#define SET_SRC(type) do {                                      \
        type s;                                                 \
        if (FSTI_GET_TYPE(s) == src_type) {                     \
            memcpy(&s, src, sizeof(s));                         \
            SET_DEST(_Bool);                                    \
            SET_DEST(char);                                     \
            SET_DEST(signed char);                              \
            SET_DEST(short);                                    \
            SET_DEST(unsigned short);                           \
            SET_DEST(int);                                      \
            SET_DEST(unsigned int);                             \
            SET_DEST(long);                                     \
            SET_DEST(unsigned long int);                        \
            SET_DEST(long long int);                            \
            SET_DEST(unsigned long long int);                   \
            SET_DEST(float);                                    \
            SET_DEST(double);                                   \
            SET_DEST(long double);                              \
        }                                                       \
    } while(0)

void fsti_cnv_vals(void *dest, const void *src,
                   enum fsti_type dest_type, enum fsti_type src_type)
{
    SET_SRC(_Bool);
    SET_SRC(char);
    SET_SRC(signed char);
    SET_SRC(short);
    SET_SRC(unsigned short);
    SET_SRC(int);
    SET_SRC(unsigned int);
    SET_SRC(long);
    SET_SRC(unsigned long int);
    SET_SRC(long long int);
    SET_SRC(unsigned long long int);
    SET_SRC(float);
    SET_SRC(double);
    SET_SRC(long double);

    FSTI_ASSERT(0, FSTI_ERR_WRONG_TYPE, NULL);

    return;
}

size_t fsti_hash(const char *str)
{
    unsigned long hash = 5381;
    int c;
    size_t result;

    while ( (c = *str++) )
        hash = ((hash << 5) + hash) + c;

    result = hash % FSTI_HASHSIZE;
    return result;
}

struct fsti_variant fsti_identify_token(char *token)
{
    double d;
    long i;
    struct fsti_variant result;
    char *err = NULL;

    errno = 0;
    g_strstrip(token);

    i = strtol(token, &err, 10);
    if (errno || *err != 0) {
	errno = 0;
	err = NULL;
	d = strtod(token, &err);
	if (errno || *err != 0) {
	    result.value.str = strdup(token);
            FSTI_ASSERT(result.value.str, FSTI_ERR_NOMEM, NULL);
	    result.type = STR;
	} else {
	    result.value.dbl = d;
	    result.type = DBL;
	}
    } else {
	result.value.longint = i;
	result.type = LONG;
    }

    return result;
}


struct fsti_variant fsti_identify_token_const(const char *token)
{
    char s[FSTI_TOKEN_LEN + 1];
    strncpy(s, token, FSTI_TOKEN_LEN);
    s[FSTI_TOKEN_LEN] = 0;
    FSTI_ASSERT(s, FSTI_ERR_NOMEM, NULL);

    return fsti_identify_token(s);
}

static _Thread_local char full_filename[FILENAME_MAX];

char *fsti_make_full_data_filename(const char *filename)
{
    const char *path;

    path = getenv("FSTI_DATA");

    if (path)
        snprintf(full_filename, FILENAME_MAX, "%s%s%s", path, G_DIR_SEPARATOR_S,
                 filename);
    else
        strncpy(full_filename, filename, FILENAME_MAX);

    return full_filename;
}

FILE *fsti_open_data_file(const char *filename, const char *mode)
{
    return fopen(fsti_make_full_data_filename(filename), mode);
}

void fsti_remove_data_file(const char *filename)
{
    remove(fsti_make_full_data_filename(filename));
}
