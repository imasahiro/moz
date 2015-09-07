#include "ast.h"
#include "pstring.h"
#include <string.h>

static struct tag {
    const char *txt;
    unsigned len;
    char *tag;
} tags[] = {
#define STRING(X) {(const char *)X, sizeof(X)-1, NULL}
    STRING("String"),
    STRING("Integer"),
    STRING("List"),
    STRING("KeyValue"),
    STRING("JSON")
#undef STRING
};

int main(int argc, char const* argv[])
{
#define TAG_String   ((char *)tags[0].tag)
#define TAG_Integer  ((char *)tags[1].tag)
#define TAG_List     ((char *)tags[2].tag)
#define TAG_KeyValue ((char *)tags[3].tag)
#define TAG_JSON     ((char *)tags[4].tag)

    AstMachine *ast;
    Node *node;
    Node *key;
    Node *elm0;
    Node *elm1;
    Node *list;
    Node *kv;
    const char input[] = "{ 'key' : [12, 345] }";
    char *str = (char *)pstring_alloc(input, strlen(input));
    int i;

    for (i = 0; i < 5; i++) {
        struct tag *t = &tags[i];
        t->tag = (char *)pstring_alloc(t->txt, t->len);
    }
    NodeManager_init();
    ast = AstMachine_init(128, str);
    AstMachine_setSource(ast, str);
#ifdef MOZVM_USE_POINTER_AS_POS_REGISTER
#define POS(N) (str + (N))
#else
#define POS(N) (N)
#endif
    /*00*/ast_log_new(ast, POS(0));
    /*01*/ast_log_new(ast, POS(2));
    /*02*/ast_log_new(ast, POS(3));
    /*03*/ast_log_tag(ast, TAG_String);
    /*04*/ast_log_capture(ast, POS(6));
    /*05*/ast_commit_tx(ast, "str", 2);
    // #String[ 'key']
    key = node = ast_get_last_linked_node(ast);
    assert(Node_length(node) == 0 &&
            node->tag == TAG_String &&
            node->len == 3 && strncmp(node->pos, "key", node->len) == 0);

    /*06*/ast_log_new(ast, POS(10));
    /*07*/ast_log_new(ast, POS(11));
    /*08*/ast_log_tag(ast, TAG_Integer);
    /*09*/ast_log_capture(ast, POS(13));
    /*10*/ast_commit_tx(ast, "int1", 4);
    // #Integer[ '12']
    elm0 = node = ast_get_last_linked_node(ast);
    assert(Node_length(node) == 0 &&
            node->tag == TAG_Integer &&
            node->len == 2 && strncmp(node->pos, "12", node->len) == 0);

    /*11*/ast_log_new(ast, POS(15));
    /*12*/ast_log_tag(ast, TAG_Integer);
    /*13*/ast_log_capture(ast, POS(18));
    /*14*/ast_commit_tx(ast, "int2", 5);
    // #Integer[ '345']
    elm1 = node = ast_get_last_linked_node(ast);
    assert(Node_length(node) == 0 &&
            node->tag == TAG_Integer &&
            node->len == 3 && strncmp(node->pos, "345", node->len) == 0);

    /*15*/ast_log_tag(ast, TAG_List);
    /*16*/ast_log_capture(ast, POS(19));
    /*17*/ast_commit_tx(ast, "list", 3);
    // #List[
    //    #Integer[ '12']
    //    #Integer[ '345']
    // ]
    list = node = ast_get_last_linked_node(ast);
    assert(Node_length(node) == 2 &&
            node->tag == TAG_List &&
            node->len == 9 && strncmp(node->pos, "[12, 345]", node->len) == 0);
    assert(Node_get(node, 0) == elm0 && Node_get(node, 1) == elm1);

    /*18*/ast_log_tag(ast, TAG_KeyValue);
    /*19*/ast_log_capture(ast, POS(19));
    /*20*/ast_commit_tx(ast, "kv", 1);
    kv = node = ast_get_last_linked_node(ast);
    assert(Node_length(node) == 2 &&
            node->tag == TAG_KeyValue &&
            node->len == 17 &&
            strncmp(node->pos, "'key' : [12, 345]", node->len) == 0);
    assert(Node_get(node, 0) == key);
    assert(Node_get(node, 1) == list);

    // #KeyValue[
    //    #String[ 'key']
    //    #List[
    //       #Integer[ '12']
    //       #Integer[ '345']
    //    ]
    // ]
    /*21*/ast_log_tag(ast, TAG_JSON);
    /*22*/ast_log_capture(ast, POS(21));
    node = ast_get_parsed_node(ast);
    assert(Node_length(node) == 1 &&
            node->tag == TAG_JSON &&
            node->len == pstring_length(str) &&
            strncmp(node->pos, str, node->len) == 0);
    assert(Node_get(node, 0) == kv);

    // #JSON[
    //    #KeyValue[
    //       #String['key']
    //       #List[
    //          #Integer['12']
    //          #Integer['345']
    //       ]
    //    ]
    // ]
#ifdef NODE_USE_NODE_PRINT
    Node_print(node);
#endif
    // asm volatile("int3");
    NODE_GC_RELEASE(node);
    AstMachine_dispose(ast);
    for (i = 0; i < 5; i++) {
        struct tag *t = &tags[i];
        pstring_delete(t->tag);
    }
    pstring_delete(str);
    NodeManager_dispose();
    return 0;
}
