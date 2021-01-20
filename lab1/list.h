struct list_node {
	struct list_node *next;
	struct list_node *prev;
	int key;
	void *value;
};

// Basic list functions
struct list_node *allocate_node_with_key(int key);
void initialize_list_head_tail(struct list_node *head, struct list_node *tail);
void insert_node_after (struct list_node *node, struct list_node *new_node);
void del_node (struct list_node *node);
struct list_node *search_list (struct list_node *head, int search_key);

int count_list_length (struct list_node *head);
int is_list_empty (struct list_node *head);
void iterate_print_keys (struct list_node *head);

// Sorted list functions
int insert_sorted_by_key (struct list_node *head, struct list_node *new_node);
