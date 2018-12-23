#ifndef FSTI_AGENT
#define FSTI_AGENT

#include <stdio.h>
#include <gsl/gsl_rng.h>
#include <stdint.h>

#include "utils/utils.h"
#include "fsti-defs.h"
#include "fsti-defaults.h"

#define FSTI_FOR(agent_ind, agent, code) do {                           \
        size_t *_it;                                                    \
        for (_it = fsti_agent_ind_begin(&(agent_ind));                  \
             _it != fsti_agent_ind_end(&(agent_ind)); _it++) {          \
            agent = fsti_agent_ind_arrp(&(agent_ind), _it);             \
            { code }                                                    \
        }                                                               \
    } while(0)

#define FSTI_FOR_IT(agent_ind, it, code) do {                           \
        for (it = fsti_agent_ind_begin(&(agent_ind));                   \
             it != fsti_agent_ind_end(&(agent_ind)); it++) {            \
            { code }                                                    \
        }                                                               \
    } while(0)

struct fsti_agent {
    uint32_t id;
    uint8_t sex;
    union {
        uint8_t sex_preferred;
        uint8_t orientation;
    };
    union {
        union {
            int32_t age;
            int32_t birth_date;
        };
        struct {
            uint16_t birthday;
            uint16_t age_group;
        };
    };
    union {
        bool infected;
        float infected_date;
    };
    struct fsti_date cured; // Date last cured of last infection
    union {
        bool dead; // 0 if still alive
        struct fsti_date date_death;
    };
    uint8_t coinfected; // For users to use as they see fit
    uint8_t cause_of_death;
    uint8_t num_partners;
    uint32_t partners[FSTI_MAX_PARTNERS];
    uint32_t relchange[FSTI_MAX_PARTNERS];
    FSTI_AGENT_FIELDS
};

struct fsti_agent_elem {
    char name[32];
    size_t offset;
    enum fsti_type type;
    fsti_transform_func transformer;
};

struct fsti_ind_list {
    struct fsti_agent_ind *ind;
    struct fsti_ind_list *next;
};

struct fsti_agent_arr {
    struct fsti_agent *agents;
    size_t len;
    size_t capacity;
    struct fsti_ind_list *ind_list;
};

struct fsti_agent_ind {
    size_t *indices;
    size_t len;
    size_t capacity;
    struct fsti_agent_arr *agent_arr;
};

extern struct fsti_agent_arr fsti_saved_agent_arr;
extern const size_t fsti_agent_elem_n;

struct fsti_agent_elem *fsti_agent_elem_get();
_Bool fsti_agent_has_partner(const struct fsti_agent *agent);
void fsti_agent_make_half_partner(struct fsti_agent *a, struct fsti_agent *b);
void fsti_agent_make_partners(struct fsti_agent *a, struct fsti_agent *b);
void fsti_agent_break_half_partner(struct fsti_agent *a, struct fsti_agent *b);
void fsti_agent_break_partners(struct fsti_agent *a, struct fsti_agent *b);
float fsti_agent_default_distance(const struct fsti_agent *a, const struct fsti_agent *b);


struct fsti_agent_elem *fsti_agent_elem_by_strname(const char *name);
long fsti_agent_elem_val_l(struct fsti_agent_elem *elem,
                           struct fsti_agent *agent);
long fsti_agent_elem_val_by_strname_l(const char *name, struct fsti_agent *agent);

void fsti_agent_arr_add_dependency(struct fsti_agent_arr *agent_arr, struct fsti_agent_ind *agent_ind);
void fsti_agent_arr_fill_n(struct fsti_agent_arr *agent_arr,  size_t n);
void fsti_agent_arr_init(struct fsti_agent_arr *agent_arr);
void fsti_agent_arr_init_n(struct fsti_agent_arr *agent_arr, size_t n);
struct fsti_agent *fsti_agent_arr_begin(struct fsti_agent_arr *agent_arr);
struct fsti_agent *fsti_agent_arr_end(struct fsti_agent_arr *agent_arr);
struct fsti_agent *fsti_agent_arr_at(struct fsti_agent_arr *agent_arr, size_t index);
struct fsti_agent *fsti_agent_arr_front(struct fsti_agent_arr *agent_arr);
struct fsti_agent *fsti_agent_arr_back(struct fsti_agent_arr *agent_arr);
struct fsti_agent *fsti_agent_partner_get(struct fsti_agent_arr *agent_arr, struct fsti_agent *agent, size_t index);
struct fsti_agent *fsti_agent_partner_get0(struct fsti_agent_arr *agent_arr, struct fsti_agent *agent);
void fsti_agent_arr_push(struct fsti_agent_arr *agent_arr, const struct fsti_agent *agent);
struct fsti_agent *fsti_agent_arr_pop(struct fsti_agent_arr *agent_arr);
void fsti_agent_ind_fill_n(struct fsti_agent_ind *agent_ind,  size_t n);
void fsti_agent_ind_init_n(struct fsti_agent_ind *agent_ind,
                           struct fsti_agent_arr *agent_arr,
                           size_t n);
void fsti_agent_ind_init(struct fsti_agent_ind *agent_ind,
                         struct fsti_agent_arr *agent_arr);
size_t *fsti_agent_ind_begin(struct fsti_agent_ind *agent_ind);
size_t *fsti_agent_ind_end(struct fsti_agent_ind *agent_ind);
size_t *fsti_agent_ind_at(struct fsti_agent_ind *agent_ind, size_t index);
void fsti_agent_ind_push(struct fsti_agent_ind *agent_ind, size_t index);
size_t fsti_agent_ind_pop(struct fsti_agent_ind *agent_ind);
void fsti_agent_ind_remove(struct fsti_agent_ind *agent_ind, size_t *it);
struct fsti_agent * fsti_agent_ind_arr(struct fsti_agent_ind *agent_ind,
                                       size_t index);
struct fsti_agent * fsti_agent_ind_arrp(struct fsti_agent_ind *agent_ind,
                                        size_t *it);
struct fsti_agent * fsti_agent_ind_arr_front(struct fsti_agent_ind *agent_ind);
struct fsti_agent * fsti_agent_ind_arr_back(struct fsti_agent_ind *agent_ind);
void fsti_agent_arr_copy(struct fsti_agent_arr *dest,
                         struct fsti_agent_arr *src,
                         bool reset_members);
void fsti_agent_ind_shuffle(struct fsti_agent_ind *agent_ind, gsl_rng *rng);
void fsti_agent_ind_copy(struct fsti_agent_ind *dest, struct fsti_agent_ind *src);
void fsti_agent_ind_clear(struct fsti_agent_ind *agent_ind);
void fsti_agent_ind_free(struct fsti_agent_ind *agent_ind);
void fsti_agent_arr_free(struct fsti_agent_arr *agent_arr);
void fsti_agent_test(struct test_group *tg);

#endif
