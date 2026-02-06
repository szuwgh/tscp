#ifndef HMAP_H
#define HMAP_H
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

struct hmap_node
{
    struct hmap_node *next;
    const void *key;
    void *value;
};

struct hmap
{
    struct hmap_node **buckets;
    size_t nbuckets;
    size_t len; // 当前节点数
    uint32_t (*hash_func)(const void *key);
    int (*key_compare)(const void *key1, const void *key2);
};

struct hmap *hmap_init(size_t nbuckets,
                       uint32_t (*hash_func)(const void *key));

void hmap_insert(struct hmap *hmap, const void *key, void *value);

void *hmap_get(struct hmap *hmap, const void *key);

void *hmap_delete(struct hmap *hmap, const void *key);

bool hmap_contains(struct hmap *hmap, const void *key);

#endif