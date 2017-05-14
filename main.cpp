#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "reader.h"

#if 0
static void DEBUGReadFeedFromFile(feed_buffer *FeedBuffer, char *FileName)
{
    struct stat Stat = {};
    stat(FileName, &Stat);
    int FileSize = Stat.st_size;

    FILE *File = fopen(FileName, "r");
    fread(FeedBuffer->Data, sizeof(char), FileSize, File);
    fclose(File);

    FeedBuffer->Size = FileSize;
    FeedBuffer->MaximumSize = FileSize;
    FeedBuffer->Valid = true;
}
#endif

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
