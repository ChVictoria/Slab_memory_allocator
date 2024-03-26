typedef struct {
    void *(*root)[];
    unsigned long levels_number;
    unsigned long items_per_level;
    unsigned long bits_per_level;
} radix_tree;

void *create_radix_tree(unsigned long long key_size, int levels_number);
void radix_tree_insert(unsigned long long key, void *value, radix_tree *tree);
void *radix_tree_get_value(unsigned long long key, radix_tree *tree);
void *radix_tree_delete_item(unsigned long long key, radix_tree *tree);
void delete_radix_tree(radix_tree *tree);
