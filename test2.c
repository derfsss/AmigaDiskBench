#include <proto/listbrowser.h>
#undef FreeListBrowserNode
void test() { IListBrowser->FreeListBrowserNode(IListBrowser, NULL); }
