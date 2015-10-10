#include "node/node.h"

int main(int argc, char const* argv[])
{
    Node *root, *child1, *child2, *child3;
    const char *tags[] = {
        ""
    };
    NodeManager_init();
    root = Node_new("root", NULL, 0, 1, NULL);
    NODE_GC_RETAIN(root);
    child1 = Node_new("child1", NULL, 0, 0, NULL);
    Node_set(root, 0, 0, child1);
    child2 = Node_new("child2", NULL, 0, 0, NULL);
    Node_set(root, 0, 0, child2);
    child3 = Node_new("child3", NULL, 0, 0, NULL);
    Node_set(root, 0, 0, child3);
#ifdef NODE_USE_NODE_PRINT
    Node_print(root, tags);
#endif
#ifdef MOZVM_MEMORY_USE_RCGC
    assert(root->refc == 1);
#endif
    NODE_GC_RELEASE(root);
    NodeManager_dispose();
    return 0;
}
