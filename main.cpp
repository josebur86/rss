#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "reader.h"

static void PrintElement(element_node *Root, int IndentLevel=0)
{
    for(int Indent = 0; Indent < IndentLevel; ++Indent)
    {
        printf("    ");
    }

#if 1
    printf("%s: %s\n", Root->Name, Root->Value);
#else
    printf("%s\n", Root->Name);
#endif
    if (Root->FirstChild)
    {
        PrintElement(Root->FirstChild, IndentLevel+1);
    }
    
    if (Root->Next)
    {
        PrintElement(Root->Next, IndentLevel);
    }
}

int main(int argc, char** argv)
{
    element_node *Root = ParseFeed("http://waitbutwhy.com/feed");
    PrintElement(Root);

    return 0;
}
