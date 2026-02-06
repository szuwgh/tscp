
#include "hmap.h"

struct hmap *hmap_init(size_t nbuckets,
                       uint32_t (*hash_func)(const void *key))
{
    struct hmap *hmap = malloc(sizeof(struct hmap));
    if (!hmap)
        return NULL;
    hmap->buckets = calloc(nbuckets, sizeof(struct hmap_node *));
    hmap->nbuckets = nbuckets;
    hmap->len = 0;
    hmap->hash_func = hash_func;
    return hmap;
}

static void hmap_grow(struct hmap *hmap)
{
    size_t new_nbuckets = hmap->nbuckets * 2;
    struct hmap_node **new_buckets = calloc(new_nbuckets, sizeof(struct hmap_node *));
    if (!new_buckets)
        return;
    for (size_t i = 0; i < hmap->nbuckets; i++)
    {
        struct hmap_node *node = hmap->buckets[i];
        while (node)
        {
            struct hmap_node *next = node->next;
            uint32_t hash = hmap->hash_func(node->key);
            size_t new_bucket_index = hash % new_nbuckets;
            node->next = new_buckets[new_bucket_index];
            new_buckets[new_bucket_index] = node;
            node = next;
        }
    }
    free(hmap->buckets);
    hmap->buckets = new_buckets;
    hmap->nbuckets = new_nbuckets;
}

void hmap_insert(struct hmap *hmap, const void *key, void *value)
{
    // Implementation of insertion logic goes here
    uint32_t hash = hmap->hash_func(key);
    if (hmap->len >= hmap->nbuckets - (hmap->nbuckets >> 2)) // 75% load factor
    {
        hmap_grow(hmap);
    }
    size_t bucket_index = hash % hmap->nbuckets;
    struct hmap_node *node = malloc(sizeof(struct hmap_node));
    if (!node)
        return;
    node->key = key;
    node->value = value;
    node->next = hmap->buckets[bucket_index];
    hmap->buckets[bucket_index] = node;
}

void *hmap_get(struct hmap *hmap, const void *key)
{
    uint32_t hash = hmap->hash_func(key);
    size_t bucket_index = hash % hmap->nbuckets;
    struct hmap_node *node = hmap->buckets[bucket_index];
    while (node)
    {
        if (hmap->key_compare(node->key, key) == 0)
            return node->value;
        node = node->next;
    }
    return NULL;
}

void *hmap_delete(struct hmap *hmap, const void *key)
{
    uint32_t hash = hmap->hash_func(key);
    size_t index = hash % hmap->nbuckets;
    struct hmap_node *node = hmap->buckets[index];
    struct hmap_node *prev = NULL;

    while (node)
    {
        if (hmap->key_compare(node->key, key) == 0)
        {
            // 找到节点，从链表移除
            if (prev)
            {
                prev->next = node->next;
            }
            else
            {
                hmap->buckets[index] = node->next;
            }
            return 0; // 成功删除
        }
        prev = node;
        node = node->next;
    }
    return -1; // 未找到
}

bool hmap_contains(struct hmap *hmap, const void *key)
{
    uint32_t hash = hmap->hash_func(key);
    size_t bucket_index = hash % hmap->nbuckets;
    struct hmap_node *node = hmap->buckets[bucket_index];
    while (node)
    {
        if (hmap->key_compare(node->key, key) == 0)
            return true;
        node = node->next;
    }
    return false;
}

// 如何进行单元测试