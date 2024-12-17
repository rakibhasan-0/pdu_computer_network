/**
* Datakommunikation och Datornät HT22
* OU3
*
* File: hashtable.c
* A hashtable implementation handling collisions by chaining.
* Author: Hanna Littorin
* Username: c19hln
* Version: 1.0
**/
#include "hashtable.h"


ht* ht_create(free_function value_free_function){
    struct ht *ht = calloc(1, sizeof(struct ht));
    if(ht == NULL){
        perror("Calloc:");
        exit(EXIT_FAILURE);
    }

    ht->entries = calloc(MAX_SIZE, sizeof(struct node_t*));
    if(ht->entries == NULL){
        perror("Calloc:");
        exit(EXIT_FAILURE);
    }

    ht->value_free_function = value_free_function;
    ht->length = 0;
    ht->num_entries = 0;

    return ht;
}

ht* ht_insert(struct ht *ht, char *key, void *value){
    hash_t index = hash_ssn(key);

    node_t *n = calloc(1, sizeof(node_t));
    if(n == NULL){
        perror("calloc:");
        exit(EXIT_FAILURE);
    }
    n->key = key;
    n->value = value;
    n->next = NULL;


    if(ht->entries[index] == NULL){
        ht->entries[index] = n;
        ht->length++;
        ht->num_entries++;
    }

    else {

        node_t *entry = ht->entries[index], *prev;

        /*
        * As long as we are not in the end of the chain and as long as we don't
        * find duplicates.
        */
        while(entry != NULL && strncmp(entry->key,key, KEY_LEN) != 0){
            prev = entry;
            entry = entry->next;
        }


        if(entry == NULL){
            // no duplicates found
            entry = prev;
            entry->next = n;
            ht->length++;
        }

        else {
            // duplicate keys found, replace the old value.
            if(ht->value_free_function != NULL) {
                (ht->value_free_function)(entry->value);
            }

            entry->value = value;
            free(n);

        }


    }


    return ht;
}

void* ht_lookup(struct ht *ht, char *key){
    hash_t index = hash_ssn(key);

    if(ht->entries[index] == NULL){
        return NULL;
    }

    node_t *entry = ht->entries[index];
    if(strncmp(entry->key,key, KEY_LEN) == 0){
        return entry->value;
    }

    else {


        while(entry != NULL){

            if(strncmp(entry->key,key, KEY_LEN) == 0){
                return entry->value;
            }

            entry = entry->next;
        }

    }

    return NULL;
}

ht* ht_remove(struct ht *ht, char *key){
    hash_t index = hash_ssn(key);
    node_t *entry = ht->entries[index], *prev;

    if(entry == NULL){
        fprintf(stderr, "%s: no such key exists in table\n",key);
    }

    else {


        if(strncmp(entry->key,key,KEY_LEN) == 0){
            ht->entries[index] = entry->next;
            if(ht->value_free_function != NULL) {
                (ht->value_free_function)(entry->value);
            }

            free(entry);
            ht->length--;

            // bucket is empty.
            if(ht->entries[index] == NULL) {
                ht->num_entries--;
            }
        }

        else {
            /*
            * As long as we are not in the end of the chain and as long as we
            * don't have a match, keep searching.
            */
            while(entry != NULL && strncmp(entry->key,key, KEY_LEN) != 0){
                prev = entry;
                entry = entry->next;
            }

            // if there was a match, delete that match, and unlink it from the chain.
            if(entry != NULL) {
                prev->next = entry->next;
                if(ht->value_free_function != NULL) {
                    (ht->value_free_function)(entry->value);
                }
                free(entry);
                ht->length--;
            }
        }

    }

    return ht;
}

void ht_destroy(struct ht *ht){

    int i = 0;

    while (!is_empty(ht)) {
        node_t *entry = ht->entries[i], *temp;


        while(entry != NULL) {
            temp = entry;
            entry = entry->next;
            ht = ht_remove(ht, temp->key);
        }

        i++;
    }


    free(ht->entries);
    free(ht);
}

bool is_empty(struct ht *ht){
    return ht->length == 0;
}

int get_num_entries(struct ht *ht){
    return ht->num_entries;
}
