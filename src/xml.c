#include "public.h"
#include "mxml.h"

static void log_cb(const char *log)
{
    LOGE("%s", log);
}

int xml_get_item(char *xml, char *path, char **value)
{
    mxml_node_t *root = NULL, *node = NULL;

    mxmlSetErrorCallback(log_cb);
    root = mxmlLoadString(NULL, xml, MXML_NO_CALLBACK);
    if (!root) {
        LOGE("load xml error");
        return -1;
    }
    node = mxmlFindPath(root, path);
    if (!node) {
        return -1;
    }
    const char *text = mxmlGetText(node, NULL);
    if (!text) {
        LOGE("get path %s error", path);
        goto err;
    }
    *value = strdup(text);

    return 0;
err:
    mxmlDelete(root);
    return -1;
}