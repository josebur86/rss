#pragma once

/* TODO(joe): At the moment, this interface just returns a raw representation of the xml document
 * without any nice way to access the data. I want to change the interface to return the actual
 * types that the consumer will want to work with (i.e. channel object, list of items and their
 * attributes, etc).
 *
 * - Internally, I'll need nicer ways of getting at the data
 *   - Visitor functions?
 * - Replace ParseFeed return type of element_node with feed object.
 */

struct attribute_node
{
    char *Name;
    char *Value;
};

#define MAX_ATTRIBUTES 16
struct element_node
{
    // NOTE(joe): I want to avoid having to keep a parent node pointer since I don't think anything
    // but the parser would need it.
    char *Name;
    char *Value;

    attribute_node Attributes[MAX_ATTRIBUTES];
    size_t AttributeCount;

    element_node *Next;
    element_node *FirstChild; // NOTE(joe): This list should be in order.
};

extern "C"
{

element_node * ParseFeed(char *FeedUrl);

//
// Temporary
//

void PrintFeed(element_node *Root);
element_node *GetFirstChildWithName(element_node *Root, char *Name);
attribute_node *GetAttributeWithName(element_node *Element, char *Name);

}
