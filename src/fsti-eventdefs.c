#include <useful.h>
#include "fsti-eventdefs.h"

static struct fsti_event_register *event_register = NULL;

void fsti_register_add(const char *event_name, fsti_event event)
{
    struct fsti_event_register *entry;
    HASH_FIND_STR(event_register, event_name, entry);
    if (entry == NULL) {
	entry = malloc(sizeof(*entry));
	if (FSTI_ERROR(entry == NULL, FSTI_ERR_NOMEM, NULL))
	    return;
	strncpy(entry->event_name, event_name, FSTI_KEY_LEN);
	HASH_ADD_STR(event_register, event_name, entry);
    }
    entry->event = event;
}

fsti_event fsti_register_get(const char *event_name)
{
    struct fsti_event_register *entry;
    HASH_FIND_STR(event_register, event_name, entry);
    FSTI_ASSERT(entry != NULL && entry->event != NULL, FSTI_ERR_EVENT_NOT_FOUND,
        event_name);
    return entry->event;
}

void fsti_register_free()
{
    struct fsti_event_register *entry, *tmp;
    HASH_ITER(hh, event_register, entry, tmp) {
	HASH_DEL(event_register, entry);
	free(entry);
    }
}
