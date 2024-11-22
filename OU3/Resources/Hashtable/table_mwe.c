#include "hashtable.h"

typedef struct value {
    uint8_t *name;
    uint8_t *email;
} value_t;

value_t *create_value(char *name, int name_length, char *email, uint32_t email_length){
    value_t *value = calloc(1, sizeof(value_t));
    if(value == NULL) {
        perror("calloc:");
        exit(EXIT_FAILURE);
    }

    if((value->name = calloc(name_length, sizeof(uint8_t))) == NULL){
        perror("calloc:");
        exit(EXIT_FAILURE);
    }

    memcpy(value->name, name,name_length);

    if((value->email = calloc(email_length, sizeof(uint8_t))) == NULL){
        perror("calloc:");
        exit(EXIT_FAILURE);
    }

    memcpy(value->email, email,email_length);

    return value;
}

void free_value(value_t *value){
    free(value->name);
    free(value->email);
    free(value);
}

int main(void){
    uint8_t key1[12], key2[12];
    value_t *value1, *value2;

    memcpy(key1, "197223058909",12); // Zahra, ZahraHermansson@armyspy.com
    memcpy(key2,"194314050879",12); //Christofer,ChristoferSandberg@teleworm.us

    value1 = create_value("Zahra", 5,"ZahraHermansson@armyspy.com", 27);
    value2 = create_value("Christofer", 10,"ChristoferSandberg@teleworm.us",30 );

    struct ht *ht = ht_create(&free_value);

    ht = ht_insert(ht, (char*)key1, (void*)value1);
    ht = ht_insert(ht, (char*)key2, (void*)value2);

    value_t *lookup = (value_t*)ht_lookup(ht, (char*)key1);

    if(lookup != NULL){
        printf("key: %.*s \nname: %.*s\nemail: %.*s\n", 12,key1, 5, lookup->name, 27, lookup->email);
    }

    fprintf(stdout, "num entries in table: %d\n",get_num_entries(ht) );

    ht = ht_remove(ht, (char*)key1);

    fprintf(stdout, "num entries after remove: %d\n",get_num_entries(ht) );

    ht_destroy(ht);


    return 0;
}
