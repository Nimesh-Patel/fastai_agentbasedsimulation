#include <glib.h>
#include <stddef.h>
#include "fsti-defs.h"
#include "fsti-events.h"
#include "fsti-report.h"
#include "fsti-dataset.h"
#include "fsti-generate.h"

struct fsti_agent fsti_global_agent;

static void
process_cell(struct fsti_agent *agent, char *cell, void *to,
             fsti_transform_func transformer)
{
    struct fsti_variant variant;

    variant = fsti_identify_token(cell);
    transformer(to, &variant, agent);
}

static void make_partnerships_mutual(struct fsti_agent_ind *agent_ind)
{
    struct fsti_agent *a, *b;
    size_t i;
    FSTI_FOR(*agent_ind, a, {
            for (i = 0; i < a->num_partners; i++) {
                b = fsti_agent_partner_get(agent_ind->agent_arr, a, i);
                FSTI_ASSERT(b->num_partners < FSTI_MAX_PARTNERS,
                            FSTI_ERR_OUT_OF_BOUNDS,
                            fsti_sprintf("Cannot make partnership "
                                         "for agents with ids: %zu %zu",
                                         a->id, b->id));
                fsti_agent_make_half_partner(b, a);
            }
        });
}

static void read_agents(struct fsti_simulation *simulation)
{
    size_t i, j;
    bool process_partners;
    FILE *f;
    struct csv cs;
    const char *filename;

    filename = fsti_config_at0_str(&simulation->config, "AGENTS_INPUT_FILE");
    process_partners = fsti_config_at0_long(&simulation->config,
                                            "MUTUAL_CSV_PARTNERS");
    f = fopen(filename, "r");
    FSTI_ASSERT(f, FSTI_ERR_AGENT_FILE, filename);

    cs = csv_read(f, simulation->agent_csv_header, simulation->csv_delimiter);
    FSTI_ASSERT(cs.header.len, FSTI_ERR_AGENT_FILE,
                FSTI_MSG("No header in agent csv file", filename));

    struct fsti_agent_elem *elems[cs.header.len];

    for (i = 0; i < cs.header.len; i++)
        elems[i] = fsti_agent_elem_by_strname(cs.header.cells[i]);

    for (i = 0; i < cs.len; i++) {
        memset(&simulation->csv_agent, 0, sizeof(struct fsti_agent));
        for (j = 0; j < cs.rows[i].len; ++j) {
            process_cell(&simulation->csv_agent,
                         cs.rows[i].cells[j],
                         (char *) &simulation->csv_agent + elems[j]->offset,
                         elems[j]->transformer);
            FSTI_ASSERT(fsti_error == 0,
                        FSTI_ERR_INVALID_CSV_FILE,
                        fsti_sprintf("File: %s at line %zu", filename, i+2));
        }
        fsti_agent_arr_push(&simulation->agent_arr,
                            &simulation->csv_agent);
    }
    fsti_agent_ind_fill_n(&simulation->living, cs.len);
    if (process_partners)
        make_partnerships_mutual(&simulation->living);

    csv_free(&cs);
    fclose(f);
}

void fsti_event_read_agents(struct fsti_simulation *simulation)
{
    static GMutex mutex;
    static bool initialized_agents = false;

    g_mutex_lock (&mutex);
    if (initialized_agents == false)  {
        read_agents(simulation);
        fsti_agent_arr_copy(&fsti_saved_agent_arr,
                            &simulation->agent_arr, false);
        initialized_agents = true;
        g_mutex_unlock (&mutex);
    } else {
        g_mutex_unlock (&mutex);
        fsti_agent_arr_copy(&simulation->agent_arr,
                            &fsti_saved_agent_arr, false);
        fsti_agent_ind_fill_n(&simulation->living, simulation->agent_arr.len);
    }
}


static void create_agent(struct fsti_simulation *simulation,
                         struct fsti_agent *agent,
                         const struct fsti_generator generators[])
{
    const struct fsti_generator *it;

    memset(agent, 0, sizeof(struct fsti_agent));
    agent->id = simulation->agent_arr.len;
    for (it = generators; it < generators + fsti_agent_elem_n; it++)
        if (it->gen_func) {
            it->gen_func(simulation, agent, &it->parms);
        }
}

static int cmp(const void *a, const void *b)
{
    const struct fsti_generator_pair *x = a, *y = b;
    return strcmp(x->name, y->name);
}

static struct fsti_generator_pair *get_gen_func(const char *key)
{
    struct fsti_generator_pair *pair;
    pair = bsearch(key, fsti_generator_map, fsti_generator_map_n,
                   sizeof(struct fsti_generator_pair), cmp);
    FSTI_ASSERT(pair, FSTI_ERR_KEY_NOT_FOUND, key);
    return pair;
}

static void setup_generators(const struct fsti_simulation *simulation,
                             struct fsti_generator generators[])
{
    size_t i, j;
    char key[FSTI_KEY_LEN];
    struct fsti_config_entry *entry;
    const struct fsti_agent_elem *elems = fsti_agent_elem_get();

    for (i = 0; i < fsti_agent_elem_n; i++) {
        strcpy(key, "A.");
        strncat(key, elems[i].name, FSTI_KEY_LEN - sizeof("A."));
        entry = fsti_config_find(&simulation->config, key);
        if (entry) {
            FSTI_ASSERT(entry->variants[0].type == STR, FSTI_ERR_WRONG_TYPE, key);
            generators[i].gen_func =
                get_gen_func(entry->variants[0].value.str)->func;
            generators[i].parms.offset = elems[i].offset;
            generators[i].parms.type = elems[i].type;
            for (j = 1; (j < FSTI_GEN_PARMS + 1) && (j < entry->len); j++) {
                fsti_cnv_vals(generators[i].parms.parameters + j - 1,
                              &entry->variants[j].value,
                              DBL,
                              entry->variants[j].type);
            }
        } else {
            generators[i].gen_func = NULL;
        }
    }
}

void fsti_event_create_agents(struct fsti_simulation *simulation)
{
    size_t i;
    struct fsti_agent agent;
    unsigned num_agents;
    struct fsti_generator generators[fsti_agent_elem_n];

    num_agents = fsti_config_at0_long(&simulation->config, "NUM_AGENTS");

    setup_generators(simulation, generators);
    for (i = 0; i < num_agents; i++) {
        create_agent(simulation, &agent, generators);
        FSTI_HOOK_CREATE_AGENT(simulation, agent);
        fsti_agent_arr_push(&simulation->agent_arr, &agent);
    }
    fsti_agent_ind_fill_n(&simulation->living, simulation->agent_arr.len);
}

void fsti_event_shuffle_living(struct fsti_simulation *simulation)
{
    fsti_agent_ind_shuffle(&simulation->living, simulation->rng);
}

void fsti_event_shuffle_mating_pool(struct fsti_simulation *simulation)
{
    fsti_agent_ind_shuffle(&simulation->mating_pool, simulation->rng);
}

void fsti_event_age(struct fsti_simulation *simulation)
{
    struct fsti_agent *a;
    FSTI_FOR_LIVING(*simulation, a, {
            a->age += simulation->time_step;
        });
}

static void outputf(struct fsti_simulation *simulation, char *desc, double val)
{
    char c = simulation->csv_delimiter;
    fprintf(simulation->results_file, "%s%c%d%c%d%c%s%c%f\n",
            simulation->name, c, simulation->sim_number,c,
            simulation->config_sim_number, c, desc, c, val);
}

static void outputl(struct fsti_simulation *simulation, char *desc, long val)
{
    char c = simulation->csv_delimiter;
    fprintf(simulation->results_file, "%s%c%d%c%d%c%s%c%ld\n",
            simulation->name, c, simulation->sim_number, c,
            simulation->config_sim_number, c, desc, c, val);
}


void fsti_event_report(struct fsti_simulation *simulation)
{
    struct fsti_agent *agent;
    double age_avg = 0.0;
    unsigned infections = 0;
    long num_partners = 0;

    if (simulation->state == DURING && simulation->iteration &&
        simulation->iteration % simulation->report_frequency != 0)
        return;

    outputl(simulation, "POPULATION_ALIVE", simulation->living.len);

    FSTI_FOR_LIVING(*simulation, agent, {
            age_avg += agent->age;
            infections += (bool) agent->infected;
            num_partners += agent->num_partners;
        });

    outputf(simulation, "AVERAGE_AGE", age_avg / simulation->agent_arr.len);
    outputf(simulation, "INFECTION_RATE",
           (double) infections / simulation->agent_arr.len);
    outputl(simulation, "NUM_PARTNERS", num_partners);
}

void fsti_event_flex_report(struct fsti_simulation *simulation)
{
    if (simulation->state == DURING && simulation->iteration &&
        simulation->iteration % simulation->report_frequency != 0)
        return;

    FSTI_FLEX_REPORT;
    FSTI_HOOK_FLEX_REPORT;
}

void fsti_event_write_results_csv_header(struct fsti_simulation *simulation)
{
    // BUG: In multithreaded run, no guarantee that this will be the top line
    // in the agent csv output, although unlikely not to be.
    if (simulation->sim_number == 0)
        FSTI_REPORT_OUTPUT_HEADER(simulation->csv_delimiter);
}

static void agent_print_csv(FILE *f, unsigned sim_no, char *time,
                            struct fsti_agent *agent, char delimiter)
{
    FSTI_AGENT_PRINT_CSV(f, sim_no, time, agent, delimiter);
}

static void agent_print_pretty(FILE *f, unsigned id, struct fsti_agent *agent)
{
    FSTI_AGENT_PRINT_PRETTY(f, id, agent);
}


static void write_agents_ind_csv(struct fsti_simulation *simulation,
                                 struct fsti_agent_ind *agent_ind)
{
    struct fsti_agent *agent;
    char *date = fsti_time_sprint(fsti_add_time_step(
                                      simulation->start_date,
                                      simulation->iteration,
                                      simulation->time_step));

    FSTI_FOR(*agent_ind, agent, {
            agent_print_csv(simulation->agents_output_file,
                            simulation->sim_number, date,
                            agent, simulation->csv_delimiter);
        });
}

static void write_agents_arr_csv(struct fsti_simulation *simulation)
{
    struct fsti_agent *agent;
    char *date = fsti_time_sprint(fsti_add_time_step(
                                      simulation->start_date,
                                      simulation->iteration,
                                      simulation->time_step));


    for (agent = simulation->agent_arr.agents;
         agent < simulation->agent_arr.agents + simulation->agent_arr.len;
         ++agent) {
        agent_print_csv(simulation->agents_output_file,
                        simulation->sim_number, date,
                        agent, simulation->csv_delimiter);
    }
}

void fsti_event_write_agents_csv_header(struct fsti_simulation *simulation)
{
    // BUG: In multithreaded run, no guarantee that this will be the top line
    // in the agent csv output, although unlikely not to be.
    if (simulation->sim_number == 0) {
        FSTI_AGENT_PRINT_CSV_HEADER(simulation->agents_output_file,
                                    simulation->csv_delimiter);
    }
}

void fsti_event_write_living_agents_csv(struct fsti_simulation *simulation)
{
    if (simulation->state == DURING && simulation->iteration &&
        simulation->iteration % simulation->report_frequency != 0)
        return;
    write_agents_ind_csv(simulation, &simulation->living);
}

void fsti_event_write_dead_agents_csv(struct fsti_simulation *simulation)
{
    if (simulation->state == DURING && simulation->iteration &&
        simulation->iteration % simulation->report_frequency != 0)
        return;
    write_agents_ind_csv(simulation, &simulation->dead);
}


void fsti_event_write_agents_csv(struct fsti_simulation *simulation)
{
    if (simulation->state == DURING && simulation->iteration &&
        simulation->iteration % simulation->report_frequency != 0)
            return;
    write_agents_arr_csv(simulation);
}

void fsti_event_write_agents_pretty(struct fsti_simulation *simulation)
{
    struct fsti_agent *agent;
    FSTI_FOR_LIVING(*simulation, agent, {
            agent_print_pretty(simulation->agents_output_file,
                               simulation->sim_number,
                               agent);
        });
}

void create_mating_pool(struct fsti_simulation *simulation, double prob_per_ts)
{
    struct fsti_agent *agent;
    fsti_agent_ind_clear(&simulation->mating_pool);
    FSTI_FOR_LIVING(*simulation, agent, {
            if (agent->num_partners == 0) {
                double prob = gsl_rng_uniform(simulation->rng);
                if (prob < prob_per_ts)
                    fsti_agent_ind_push(&simulation->mating_pool, agent->id);
            }
        });
}

void fsti_event_initial_mating_pool(struct fsti_simulation *simulation)
{
    create_mating_pool(simulation, simulation->initial_mating_pool_prob);
}

void fsti_event_mating_pool(struct fsti_simulation *simulation)
{
    create_mating_pool(simulation, simulation->mating_pool_prob);
}

static void
set_rel_period(struct fsti_simulation *simulation, struct fsti_agent *a)
{
    a->relchange[0] = 365;
}


static void
set_single_period(struct fsti_simulation *simulation, struct fsti_agent *a)
{
    a->relchange[0] = 365;
}

void fsti_event_initial_relchange(struct fsti_simulation *simulation)
{
    struct fsti_agent *a, *b;

    FSTI_FOR_LIVING(*simulation, a, {
            b = fsti_agent_partner_get0(&simulation->agent_arr, a);
            if (b) {
                set_rel_period(simulation, a);
                b->relchange[0] = a->relchange[0];
            } else {
                set_single_period(simulation, a);
            }
        });
}

void fsti_event_breakup(struct fsti_simulation *simulation)
{
    struct fsti_agent *a, *b;

    FSTI_FOR_LIVING(*simulation, a, {
            // If agent is in partnership and duration of partnership has run out
            b = fsti_agent_partner_get0(&simulation->agent_arr, a);
            if (b && simulation->iteration > a->relchange[0]) {
                fsti_agent_break_partners(a, b);
                // Determine at what day in the future these agents will start
                // looking for new partners

                set_single_period(simulation, a);
                set_single_period(simulation, b);
            }
        });

}

void fsti_event_death(struct fsti_simulation *simulation)
{
    struct fsti_agent *agent;
    double d, r;
    size_t *it;

    FSTI_ASSERT(simulation->dataset_mortality, FSTI_ERR_MISSING_DATASET, NULL);

    it = simulation->living.indices;
    while (it < (simulation->living.indices + simulation->living.len)) {
        agent = fsti_agent_ind_arrp(&simulation->living, it);
        d = fsti_dataset_lookup(simulation->dataset_mortality, agent);
        r = gsl_rng_uniform(simulation->rng);
        if (r < d)
            fsti_simulation_kill_agent(simulation, it);
        else
            ++it;
    }
}

void fsti_event_knn_match(struct fsti_simulation *simulation)
{
    struct fsti_agent *agent, *candidate, *partner;
    size_t *it, *jt, *start, *n, *begin, *end;
    size_t num_agents = simulation->mating_pool.len;
    float d, best_dist;
    size_t k = simulation->match_k;

    // Too few agents to bother
    if (num_agents < 2) return;
    // Can only match even number of agents
    if (num_agents % 2) fsti_agent_ind_pop(&simulation->mating_pool);

    begin = fsti_agent_ind_begin(&simulation->mating_pool);
    end = fsti_agent_ind_end(&simulation->mating_pool);

    for (it = begin; it < end - 1; it++) {
        agent = fsti_agent_ind_arrp(&simulation->mating_pool, it);
        if (FSTI_AGENT_HAS_PARTNER(agent)) continue;
        start = it + 1;
        n = (start + k) < end ? (start + k) : end;
        best_dist = FLT_MAX;
        partner = NULL;
        for (jt = start; jt < n; jt++) {
            candidate = fsti_agent_ind_arrp(&simulation->mating_pool, jt);
            if (FSTI_AGENT_HAS_PARTNER(candidate)) continue;
            d = FSTI_AGENT_DISTANCE(agent, candidate);
            if (d < best_dist) {
                best_dist = d;
                partner = candidate;
            }
        }
        if (partner) {
            fsti_agent_make_partners(agent, partner);
            set_rel_period(simulation, agent);
            partner->relchange[0] = agent->relchange[0];
            FSTI_HOOK_AFTER_MATCH(simulation, agent, partner);
        }
    }
}

void fsti_event_no_op(struct fsti_simulation *simulation)
{
    return;
}

void fsti_event_register_events()
{
    static bool initialized_events = false;

    if (initialized_events == false) {
        initialized_events = true;
        fsti_register_add("_READ_AGENTS", fsti_event_read_agents);
        fsti_register_add("_GENERATE_AGENTS", fsti_event_create_agents);
        fsti_register_add("_AGE", fsti_event_age);
        fsti_register_add("_DEATH", fsti_event_death);
        fsti_register_add("_INITIAL_REL", fsti_event_initial_relchange);
        fsti_register_add("_INITIAL_MATING", fsti_event_initial_mating_pool);
        fsti_register_add("_MATING_POOL", fsti_event_mating_pool);
        fsti_register_add("_BREAKUP", fsti_event_breakup);
        fsti_register_add("_SHUFFLE_LIVING", fsti_event_shuffle_living);
        fsti_register_add("_SHUFFLE_MATING", fsti_event_shuffle_mating_pool);
        fsti_register_add("_RKPM", fsti_event_knn_match);
        fsti_register_add("_REPORT", fsti_event_report);
        fsti_register_add("_FLEX_REPORT", fsti_event_flex_report);
        fsti_register_add("_WRITE_RESULTS_CSV_HEADER",
                          fsti_event_write_results_csv_header);
        fsti_register_add("_WRITE_AGENTS_CSV_HEADER",
                          fsti_event_write_agents_csv_header);
        fsti_register_add("_WRITE_AGENTS_CSV", fsti_event_write_agents_csv);
        fsti_register_add("_WRITE_LIVING_AGENTS_CSV",
                          fsti_event_write_living_agents_csv);
        fsti_register_add("_WRITE_DEAD_AGENTS_CSV",
                          fsti_event_write_dead_agents_csv);
        fsti_register_add("_WRITE_AGENTS_PRETTY", fsti_event_write_agents_pretty);
        fsti_register_add(FSTI_NO_OP, fsti_event_no_op);
        FSTI_HOOK_EVENTS_REGISTER;
    }
}
