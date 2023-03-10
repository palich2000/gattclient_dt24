/**
 * @file queue.c
 * @brief set of functions to manage queues
 * @author Gilbert Brault
 * @copyright Gilbert Brault 2015
 * the original work comes from bluez v5.39
 * value add: documenting main features
 *
 */
/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2012-2014  Intel Corporation. All rights reserved.
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "util.h"
#include "queue.h"

struct queue {
    int ref_count;
    struct queue_entry * head;
    struct queue_entry * tail;
    unsigned int entries;
};

/**
 * increment &queue->ref_count
 *
 * @param queue pointer to the queue structure
 * @return
 */
static struct queue * queue_ref(struct queue * queue) {
    if (!queue)
        return NULL;

    __sync_fetch_and_add(&queue->ref_count, 1);

    return queue;
}

/**
 * decrement &queue->ref_count and free queue structure if ref_count == 0
 *
 * @param queue  pointer to the queue structure
 */
static void queue_unref(struct queue * queue) {
    if (__sync_sub_and_fetch(&queue->ref_count, 1))
        return;

    free(queue);
}

/**
 * create a new queue structure
 *
 * @return queue reference
 */
struct queue * queue_new(void) {
    struct queue * queue;

    queue = new0(struct queue, 1);
    if (!queue)
        return NULL;

    queue->head = NULL;
    queue->tail = NULL;
    queue->entries = 0;

    return queue_ref(queue);
}

/**
 * destroy all queue structure elements
 *
 * @param queue		pointer to the queue structure
 * @param destroy	function making data deallocation
 */
void queue_destroy(struct queue * queue, queue_destroy_func_t destroy) {
    if (!queue)
        return;

    queue_remove_all(queue, NULL, NULL, destroy);

    queue_unref(queue);
}

/**
 * increment &entry->ref_count
 *
 * @param entry
 * @return	entry
 */
static struct queue_entry * queue_entry_ref(struct queue_entry * entry) {
    if (!entry)
        return NULL;

    __sync_fetch_and_add(&entry->ref_count, 1);

    return entry;
}

/**
 * decrement &entry->ref_count and free entry structure if ref_count == 0
 *
 * @param entry
 */
static void queue_entry_unref(struct queue_entry * entry) {
    if (__sync_sub_and_fetch(&entry->ref_count, 1))
        return;

    free(entry);
}

/**
 * create a new queue, set queue->data to data, increment queue->ref_count
 *
 * @param data
 * @return	new queue pointer
 */
static struct queue_entry * queue_entry_new(void * data) {
    struct queue_entry * entry;

    entry = new0(struct queue_entry, 1);
    if (!entry)
        return NULL;

    entry->data = data;

    return queue_entry_ref(entry);
}

/**
 * push a queue entry allocated with data and set it at the tail of queue
 *
 * @param queue	queue pointer where to allocate data
 * @param data	data to queue
 * @return
 */
bool queue_push_tail(struct queue * queue, void * data) {
    struct queue_entry * entry;

    if (!queue)
        return false;

    entry = queue_entry_new(data);
    if (!entry)
        return false;

    if (queue->tail)
        queue->tail->next = entry;

    queue->tail = entry;

    if (!queue->head)
        queue->head = entry;

    queue->entries++;

    return true;
}

/**
 * push a queue entry allocated with data and set it at the head of queue
 *
 * @param queue	queue pointer where to allocate data
 * @param data	data to queue
 * @return
 */
bool queue_push_head(struct queue * queue, void * data) {
    struct queue_entry * entry;

    if (!queue)
        return false;

    entry = queue_entry_new(data);
    if (!entry)
        return false;

    entry->next = queue->head;

    queue->head = entry;

    if (!queue->tail)
        queue->tail = entry;

    queue->entries++;

    return true;
}

/**
 * push a queue entry allocated with data and set it after the entry
 *
 * @param queue	queue pointer where to allocate data
 * @param entry element queue where to put the data after
 * @param data	data to queue
 * @return
 */
bool queue_push_after(struct queue * queue, void * entry, void * data) {
    struct queue_entry * qentry, *tmp, *new_entry;

    qentry = NULL;

    if (!queue)
        return false;

    for (tmp = queue->head; tmp; tmp = tmp->next) {
        if (tmp->data == entry) {
            qentry = tmp;
            break;
        }
    }

    if (!qentry)
        return false;

    new_entry = queue_entry_new(data);
    if (!new_entry)
        return false;

    new_entry->next = qentry->next;

    if (!qentry->next)
        queue->tail = new_entry;

    qentry->next = new_entry;
    queue->entries++;

    return true;
}

/**
 * pop data from the head of queue
 *
 * @param queue	queue pointer
 */
void * queue_pop_head(struct queue * queue) {
    struct queue_entry * entry;
    void * data;

    if (!queue || !queue->head)
        return NULL;

    entry = queue->head;

    if (!queue->head->next) {
        queue->head = NULL;
        queue->tail = NULL;
    } else
        queue->head = queue->head->next;

    data = entry->data;

    queue_entry_unref(entry);
    queue->entries--;

    return data;
}

/**
 * peek data from the head of the queue
 *
 * @param queue	queue pointer
 */
void * queue_peek_head(struct queue * queue) {
    if (!queue || !queue->head)
        return NULL;

    return queue->head->data;
}

/**
 * peek data from the tail of the queue
 *
 * @param queue	queue pointer
 */
void * queue_peek_tail(struct queue * queue) {
    if (!queue || !queue->tail)
        return NULL;

    return queue->tail->data;
}

/**
 * iterator for the queue
 *
 * @param queue			queue pointer
 * @param function		function(void *data, void *user_data) to call for each element
 * @param user_data		user pointer to pass to function
 */
void queue_foreach(struct queue * queue, queue_foreach_func_t function,
                   void * user_data) {
    struct queue_entry * entry;

    if (!queue || !function)
        return;

    entry = queue->head;
    if (!entry)
        return;

    queue_ref(queue);
    while (entry && queue->head && queue->ref_count > 1) {
        struct queue_entry * next;

        queue_entry_ref(entry);

        function(entry->data, user_data);

        next = entry->next;

        queue_entry_unref(entry);

        entry = next;
    }
    queue_unref(queue);
}

/**
 * compare a with b
 *
 * @param a	first parameter
 * @param b	second parameter
 *
 * @return true if both element match
 */
static bool direct_match(const void * a, const void * b) {
    return a == b;
}

/**
 *
 *
 * @param queue			queue pointer
 * @param function		function call to make comparison
 * @param match_data	data to match with queue entry
 *
 * @return found entry and return pointer to user data (queued data) or NULL if not found
 */
void * queue_find(struct queue * queue, queue_match_func_t function,
                  const void * match_data) {
    struct queue_entry * entry;

    if (!queue)
        return NULL;

    if (!function)
        function = direct_match;

    for (entry = queue->head; entry; entry = entry->next)
        if (function(entry->data, match_data))
            return entry->data;

    return NULL;
}

/**
 * remove queue element holding data
 *
 * @param queue	queue pointer
 * @param data	data pointer
 *
 * @return false if not found, true if element removed
 */
bool queue_remove(struct queue * queue, void * data) {
    struct queue_entry * entry, *prev;

    if (!queue)
        return false;

    for (entry = queue->head, prev = NULL; entry;
            prev = entry, entry = entry->next) {
        if (entry->data != data)
            continue;

        if (prev)
            prev->next = entry->next;
        else
            queue->head = entry->next;

        if (!entry->next)
            queue->tail = prev;

        queue_entry_unref(entry);
        queue->entries--;

        return true;
    }

    return false;
}

/**
 * remove element match by function(void *data, void *match_data) where match_data = user_data
 *
 * @param queue		queue pointer
 * @param function	function call to make comparison
 * @param user_data data to match queue element in function
 *
 * @return NULL if not found or queue entry matching criteria
 */
void * queue_remove_if(struct queue * queue, queue_match_func_t function,
                       void * user_data) {
    struct queue_entry * entry, *prev = NULL;

    if (!queue || !function)
        return NULL;

    entry = queue->head;

    while (entry) {
        if (function(entry->data, user_data)) {
            void * data;

            if (prev)
                prev->next = entry->next;
            else
                queue->head = entry->next;

            if (!entry->next)
                queue->tail = prev;

            data = entry->data;

            queue_entry_unref(entry);
            queue->entries--;

            return data;
        } else {
            prev = entry;
            entry = entry->next;
        }
    }

    return NULL;
}

/**
 * remove all queue element
 *
 * @param queue		queue pointer
 * @param function	function used to match data deallocation
 * @param user_data	data pointer used in deallocation function
 * @param destroy	function for data deallocation
 *
 * @return			count of element destroyed (0 if none)
 */
unsigned int queue_remove_all(struct queue * queue, queue_match_func_t function,
                              void * user_data, queue_destroy_func_t destroy) {
    struct queue_entry * entry;
    unsigned int count = 0;

    if (!queue)
        return 0;

    entry = queue->head;

    if (function) {
        while (entry) {
            void * data;
            unsigned int entries = queue->entries;

            data = queue_remove_if(queue, function, user_data);
            if (entries == queue->entries)
                break;

            if (destroy)
                destroy(data);

            count++;
        }
    } else {
        queue->head = NULL;
        queue->tail = NULL;
        queue->entries = 0;

        while (entry) {
            struct queue_entry * tmp = entry;

            entry = entry->next;

            if (destroy)
                destroy(tmp->data);

            queue_entry_unref(tmp);
            count++;
        }
    }

    return count;
}

/**
 * return queue head pointer
 *
 * @param queue	queue pointer
 *
 * @return	queue head
 */
const struct queue_entry * queue_get_entries(struct queue * queue) {
    if (!queue)
        return NULL;

    return queue->head;
}

/**
 * return queue length
 *
 * @param queue	queue pointer
 *
 * @return	return 0 or queue elements count
 */
unsigned int queue_length(struct queue * queue) {
    if (!queue)
        return 0;

    return queue->entries;
}

/**
 * test if queue is empty
 *
 * @param queue	queue pointer
 * @return
 */
bool queue_isempty(struct queue * queue) {
    if (!queue)
        return true;

    return queue->entries == 0;
}
