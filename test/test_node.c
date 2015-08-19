#include "node.h"

int main(int argc, char const* argv[])
{
    Node root = Node_new(NULL, NULL, 0, 0, NULL);
    Node_print(root);
    Node_free(root);
    return 0;
}
