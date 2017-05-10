#include <assert.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <curl/curl.h>

#include "reader.h"

#define TEST_FEED 1
#define READER_DEBUG 0

static size_t StoreFeed(char *Data, size_t Size, size_t Count, void *User)
{
    feed_buffer *Buffer = (feed_buffer *)User;
    char *BufferData = Buffer->Data + Buffer->Size;

    for (size_t Index = 0; Index < Count; ++Index)
    {
        assert(Buffer->Size < Buffer->MaximumSize);

        *BufferData++ = *Data++;
        Buffer->Size += Size;
    }

    return Size * Count;
}
static void FetchFeed(feed_buffer *FeedBuffer, char *URL)
{
    CURL *Curl = curl_easy_init();
    if (Curl)
    {
        CURLcode CurlResult;

#if READER_DEBUG
        CurlResult = curl_easy_setopt(Curl, CURLOPT_VERBOSE, 1);
        assert(CurlResult == CURLE_OK);
#endif

        CurlResult = curl_easy_setopt(Curl, CURLOPT_URL, URL);
        assert(CurlResult == CURLE_OK);

        CurlResult = curl_easy_setopt(Curl, CURLOPT_WRITEFUNCTION, &StoreFeed);
        assert(CurlResult == CURLE_OK);

        CurlResult = curl_easy_setopt(Curl, CURLOPT_WRITEDATA, FeedBuffer);
        assert(CurlResult == CURLE_OK);

        CurlResult = curl_easy_perform(Curl);
        if (CurlResult == CURLE_OK)
        {
            FeedBuffer->Valid = true;
        }
        else
        {
            // TODO(joe): Print the error.
            printf("Feed fetch unsuccessful!\n");
        }
    }
    else
    {
        printf("Curl not initialized!\n");
    }

    curl_easy_cleanup(Curl);
}

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
    feed_buffer FeedBuffer = {};
    FeedBuffer.MaximumSize = 1100000;
    FeedBuffer.Data = (char *)calloc(FeedBuffer.MaximumSize, sizeof(char));

#if TEST_FEED
    DEBUGReadFeedFromFile(&FeedBuffer, "feed.xml");
#else
    FetchFeed(&FeedBuffer, "http://waitbutwhy.com/feed");
#endif

    if (FeedBuffer.Valid)
    {
        parser Parser = {};
        element_node *FeedRoot = ParseFeed(&FeedBuffer, &Parser);
        PrintElement(FeedRoot);
    }
    else
    {
        printf("Invalid Feed!\n");
    }

    free(FeedBuffer.Data);

    return 0;
}
