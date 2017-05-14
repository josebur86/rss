#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "reader.h"

int main(int argc, char** argv)
{
    element_node *Root = ParseFeed("http://waitbutwhy.com/feed");

    printf("TEST\nGetting the Channel...\n\n");

    element_node *Channel = GetFirstChildWithName(Root, "channel");
    if (Channel)
    {
        element_node *Title = GetFirstChildWithName(Channel, "title");
        if (Title)
        {
            printf("Channel Title: %s\n", Title->Value);
        }

        element_node *Link = GetFirstChildWithName(Channel, "link");
        if (Link)
        {
            printf("Channel Link: %s\n", Link->Value);
        }

        element_node *FeedLink = GetFirstChildWithName(Channel, "atom:link");
        if (FeedLink)
        {
            attribute_node *HRef = GetAttributeWithName(FeedLink, "href");
            if (HRef)
            {
                printf("Channel Feed Link: %s\n", HRef->Value);
            }
        }
    }

    return 0;
}
