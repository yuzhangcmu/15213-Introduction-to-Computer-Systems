#include "csapp.h"
#include "cache.h"

#if 0

int main()
{
    Cache* cache = create_cache();
    int i;
    static int cnt = 0;
    char tmp[100];

    for(i=0; i<3; i++){
        sprintf(tmp, "_%d", cnt++);
        Node* node = create_node(tmp, 100);
        strcpy(node->content, tmp);
        insert_node_tail(cache, node);
    }

    for(i=0; i<3; i++){
        sprintf(tmp, "_%d", cnt++);
        Node* node = create_node(tmp, 100);
        strcpy(node->content, tmp);
        insert_node_head(cache, node);
    }

    delete_node_by_id(cache, "_3");

    delete_node_from_head(cache);

    print_cache(cache);

    printf("--------------------------\n");
    Node* find = find_node_by_id(cache, "_4");
    print_node(find);
}

#endif
