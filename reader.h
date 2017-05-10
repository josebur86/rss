#pragma once

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

enum parse_state
{
    ParseError = 0,
    ParseStart,
    ParseProlog,
    ParseBeginElement,
    ParseResumeBeginElement,
    ParseEndElement,
    ParseElementValue,
    ParseAttribute,
};

struct parser_cursor
{
    parse_state State;
    element_node *Element;
};

struct parser
{
    int CurrentIndex;
    parser_cursor Cursors[256];
};

struct feed_buffer
{
    bool Valid;

    char *Data;
    size_t Size;
    size_t MaximumSize;
};

extern "C"
{

element_node * ParseFeed(feed_buffer *FeedBuffer, parser *Parser);

}
