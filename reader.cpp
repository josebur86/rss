#include <assert.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "reader.h"

#define TEST_FEED 1
#define READER_DEBUG 0

struct feed_buffer
{
    bool Valid;

    char *Data;
    size_t Size;
    size_t MaximumSize;
};

#if TEST_FEED
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

element_node * PushElement()
{
    // TODO(joe): Stop allocating from the heap.
    element_node *Result = (element_node *)calloc(1, sizeof(element_node));

    return Result;
}

static void AddChildElementTo(element_node *Parent, element_node *Element)
{
    if (!Parent->FirstChild)
    {
        Parent->FirstChild = Element;
    }
    else
    {
        element_node *Current = Parent->FirstChild;
        for (; Current->Next; Current = Current->Next)
        {
            // Empty
        }

        Current->Next = Element;
    }
}

static void DEBUGPrintElement(element_node *Element)
{
    printf("Name: %s", Element->Name);
    if (Element->Value)
    {
        printf(" -> %s\n", Element->Value);
    }
    printf(" Attributes\n");
    for (size_t Index = 0; Index < Element->AttributeCount; ++Index)
    {
        attribute_node *Attr = Element->Attributes + Index;
        printf("%s -> %s\n", Attr->Name, Attr->Value);
    }
}

attribute_node * PushAttribute()
{
    attribute_node *Result = (attribute_node *)calloc(1, sizeof(attribute_node));
    return Result; 
}

bool AreStringsEqual(char *A, char *B)
{
    bool Result = true;

    while(A && B && (*A != '\0') && (*B != '\0') && (*A == *B))
    {
        ++A;
        ++B;
    }

    if ((*A != '\0') || (*B != '\0'))
    {
        Result = false;
    }

    return Result;
}
                    
char * CopyString(char *Source, size_t Size)
{
    char *Result = 0;

    if (Size > 0)
    {
        Result = (char *)calloc(Size, sizeof(char));

        char *Dest = Result;
        for (size_t Index = 0; Index < Size; ++Index)
        {
            *Dest++ = *Source++;    
        }
    }

    return Result;
}

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

char *GetStateText(parse_state State)
{
    switch(State)
    {
        case ParseError:
        {
            return "ParseError";
        } break;
        case ParseStart:
        {
            return "ParseStart";
        } break;
        case ParseProlog:
        {
            return "ParseProlog";
        } break;
        case ParseBeginElement:
        {
            return "ParseBeginElement";
        } break;
        case ParseResumeBeginElement:
        {
            return "ParseResumeBeginElement";
        } break;
        case ParseEndElement:
        {
            return "ParseEndElement";
        } break;
        case ParseElementValue:
        {
            return "ParseElementValue";
        } break;
        case ParseAttribute:
        {
            return "ParseAttribute";
        } break;
    };

    return "";
}

static parse_state GetCurrentState(parser *Parser)
{
    return Parser->Cursors[Parser->CurrentIndex].State;
}

static void SetCurrentState(parser *Parser, parse_state State)
{
    Parser->Cursors[Parser->CurrentIndex].State = State;
}

static parser_cursor * GetCurrentCursor(parser *Parser)
{
    parser_cursor *Cursor = Parser->Cursors + Parser->CurrentIndex;
    return Cursor;
}

static parser_cursor * PushAndGetCurrentCursor(parser *Parser, parse_state State, element_node *Element)
{
    parser_cursor *Cursor = Parser->Cursors + ++Parser->CurrentIndex;
    Cursor->State = State;
    Cursor->Element = Element;

    return Cursor;
}

static parser_cursor * PopAndGetCurrentCursor(parser *Parser)
{
    if (Parser->CurrentIndex > 0)
    {
        --Parser->CurrentIndex;
    }
    parser_cursor *Cursor = Parser->Cursors + Parser->CurrentIndex;
    return Cursor;
}

static char * AdvanceToNonWhiteSpaceChar(char *Sym)
{
    // NOTE(joe): Get the next not whitespace character.
    while (Sym)
    {
        if (*Sym == '\n' || *Sym == '\t' || *Sym == ' ')
        {
            ++Sym;
        }
        else
        {
            break;
        }
    }

    return Sym;
}

// NOTE(joe): Each state operation should assume that the symbol is already at the first character
// that it needs to process and should leave the symbol at the first character that it has not
// processed.

struct parse_op_result
{
    char *Sym;
    parser_cursor *Cursor;
};

static parse_op_result OnParseStart(parser *Parser, char *Sym, parser_cursor *Cursor)
{
    if (*Sym == '<' && (Sym+1) && *(Sym+1) == '?')
    {
        SetCurrentState(Parser, ParseProlog);
        Sym += 2; // <?xml...
                  // --^
    }
    else
    {
        SetCurrentState(Parser, ParseBeginElement);
    }

    parse_op_result Result = {Sym, Cursor};
    return Result;
}

static parse_op_result OnParseProlog(parser *Parser, char *Sym, parser_cursor *Cursor)
{
    while (Sym)
    {
        if (*Sym == '?' && (Sym+1) && *(Sym+1) == '>')
        {
            SetCurrentState(Parser, ParseBeginElement);
            Sym += 2; // ?>.
                      // --^
            break;
        }
        else
        {
            ++Sym;
        }
    }

    parse_op_result Result = {Sym, Cursor};
    return Result;
}
                
static parse_op_result OnParseBeginElement(parser *Parser, char *Sym, parser_cursor *Cursor)
{
    char ElementName[32] = "\0";
    size_t NameSize = 0;

    bool Continue = true;
    while (Sym && Continue)
    {
        if (*Sym == '<' && (Sym+1) && *(Sym+1) == '/')
        {
            SetCurrentState(Parser, ParseEndElement);
            Continue = false;
        }
        else if (*Sym == '<')
        {
            if (!Cursor->Element)
            {
                Cursor->Element = PushElement();
            }
            else
            {
                element_node *Parent = Cursor->Element;
                element_node *Element = PushElement();
                AddChildElementTo(Parent, Element);
                Cursor = PushAndGetCurrentCursor(Parser, Cursor->State, Element);
            }

            ++Sym;
            while (Sym && *Sym != ' ' && *Sym != '>')
            {
                ElementName[NameSize++] = *Sym;
                ++Sym;
            }
            ElementName[NameSize] = '\0';
            Cursor->Element->Name = CopyString(ElementName, NameSize);

            if (*Sym == ' ')
            {
                SetCurrentState(Parser, ParseAttribute);
                Continue = false;
                ++Sym;
            }
            else if (*Sym == '/' && (Sym+1) && *(Sym+1) == '>')
            {
                Sym += 2;
            }
            else if (*Sym == '>')
            {
                Sym = AdvanceToNonWhiteSpaceChar(++Sym);
                if (Sym)
                {
                    SetCurrentState(Parser, ParseElementValue);
                    Continue = false;
                }
                else
                {
                    SetCurrentState(Parser, ParseError);
                    Continue = false;
                }
            }
        }
        else
        {
            SetCurrentState(Parser, ParseError);
            Continue = false;
        }
    }

    parse_op_result Result = {Sym, Cursor};
    return Result;
}

static parse_op_result OnParseResumeBeginElement(parser *Parser, char *Sym, parser_cursor *Cursor)
{
    while (Sym)
    {
        if (*Sym == '>')
        {
            ++Sym;
            SetCurrentState(Parser, ParseElementValue);
            break;
        }
        else if (*Sym == '/' && (Sym+1) && *(Sym+1) == '>')
        {
            // NOTE(joe): />
            //            |-^
            Sym += 2;
            Cursor = PopAndGetCurrentCursor(Parser);
            SetCurrentState(Parser, ParseBeginElement);
            break;
        }
        else
        {
            SetCurrentState(Parser, ParseAttribute);
            break;
        }
    }

    parse_op_result Result = {Sym, Cursor};
    return Result;
}

static parse_op_result OnParseElementValue(parser *Parser, char *Sym, parser_cursor *Cursor)
{
    if (*Sym == '<' && (Sym+1) && *(Sym+1) == '/')
    {
        SetCurrentState(Parser, ParseEndElement);
    }
    else if (*Sym == '<' && (Sym+1) && *(Sym+1) == '!')
    {
        char Buffer[5000000]; // TODO(joe): ~5MB. Count, Alloc, MemCopy?
        size_t Size = 0;
        // NOTE(joe): <![CDATA[...]]!> is assumed for now.
        //            |--------^
        Sym += 9;

        bool Continue = true;
        while (Continue)
        {
            if (Sym && *Sym == ']' && (Sym+1) && *(Sym+1) == ']')
            {
                Continue = false;
            }
            else
            {
                Buffer[Size++] = *Sym;
                ++Sym;
            }
        }

        Buffer[Size] = '\0';
        Cursor->Element->Value = CopyString(Buffer, Size);

        // NOTE(joe): ...]]!>
        //               |---^
        Sym += 4;
        SetCurrentState(Parser, ParseEndElement);
    }
    else if (*Sym == '<')
    {
        SetCurrentState(Parser, ParseBeginElement);
    }
    else // NOTE(joe): Simple non-CDATA value.
    {
        char Buffer[2048]; // TODO(joe): Dynamically increasing buffer?
        size_t Size = 0;
        bool Continue = true;
        while (Continue)
        {
            Buffer[Size++] = *Sym++;
            if (*Sym == '<')
            {
                Continue = false;

                Buffer[Size] = '\0';
                Cursor->Element->Value = CopyString(Buffer, Size);

                SetCurrentState(Parser, ParseEndElement);
            }
        }
    }

    parse_op_result Result = {Sym, Cursor};
    return Result;
}

static parse_op_result OnParseEndElement(parser *Parser, char *Sym, parser_cursor *Cursor)
{
    bool Continue = true;
    while (*Sym != '>')
    {
        ++Sym;
    }
    ++Sym;

    Cursor = PopAndGetCurrentCursor(Parser);

    parse_op_result Result = {Sym, Cursor};
    return Result;
}

static parse_op_result OnParseAttribute(parser *Parser, char *Sym, parser_cursor *Cursor)
{
    char Buffer[512] = "\0";
    size_t Size = 0;

    attribute_node *CurrentAttribute = Cursor->Element->Attributes + Cursor->Element->AttributeCount++;

    bool Continue = true;
    bool ParseAttributeName = true;
    bool FirstValueQuote = true;
    while (Sym && Continue)
    {
        if (ParseAttributeName)
        {
            if (*Sym != '=')
            {
                Buffer[Size++] = *Sym;
            }
            else if (*Sym == '=')
            {
                Buffer[Size] = '\0';
                CurrentAttribute->Name = CopyString(Buffer, Size);
                Size = 0;
                ParseAttributeName = false;
            }
        }
        else
        {
            if (*Sym == '"' && !FirstValueQuote)
            {
                Continue = false;
                Buffer[Size] = '\0';
                CurrentAttribute->Value = CopyString(Buffer, Size);

                SetCurrentState(Parser, ParseResumeBeginElement);
            }
            else if (*Sym == '"')
            {
                FirstValueQuote = false;
            }
            else
            {
                Buffer[Size++] = *Sym;
            }
        }

        ++Sym;
    }
    parse_op_result Result = {Sym, Cursor};
    return Result;
}


static element_node * ParseFeed(feed_buffer *FeedBuffer, parser *Parser)
{
    parser_cursor *Cursor = GetCurrentCursor(Parser);
    Cursor->State = ParseStart;
    Cursor->Element = 0;

    char *Sym = AdvanceToNonWhiteSpaceChar(FeedBuffer->Data);
    while (Sym && *Sym != '\0')
    {
#if READER_DEBUG
        if (Cursor->Element)
        {
            printf("Element: %s State: %s\n", Cursor->Element->Name, GetStateText(Cursor->State));
            DEBUGPrintElement(Cursor->Element);
        }
#endif
        switch(Cursor->State)
        {
            case ParseStart:
            {
                parse_op_result Result = OnParseStart(Parser, Sym, Cursor);
                Sym = Result.Sym;
                Cursor = Result.Cursor;
            } break;
            case ParseProlog:
            {
                parse_op_result Result = OnParseProlog(Parser, Sym, Cursor);
                Sym = Result.Sym;
                Cursor = Result.Cursor;
            } break;
            case ParseBeginElement:
            {
                parse_op_result Result = OnParseBeginElement(Parser, Sym, Cursor);
                Sym = Result.Sym;
                Cursor = Result.Cursor;
            } break;
            case ParseResumeBeginElement:
            {
                parse_op_result Result = OnParseResumeBeginElement(Parser, Sym, Cursor);
                Sym = Result.Sym;
                Cursor = Result.Cursor;
            } break;
            case ParseElementValue:
            {
                parse_op_result Result = OnParseElementValue(Parser, Sym, Cursor);
                Sym = Result.Sym;
                Cursor = Result.Cursor;
            } break;
            case ParseEndElement:
            {
                parse_op_result Result = OnParseEndElement(Parser, Sym, Cursor);
                Sym = Result.Sym;
                Cursor = Result.Cursor;
            } break;
            case ParseAttribute:
            {
                parse_op_result Result = OnParseAttribute(Parser, Sym, Cursor);
                Sym = Result.Sym;
                Cursor = Result.Cursor;
            } break;
            default:
            {
                printf("ParseError\n");
                printf("Xml is in an unexpected format.\n");
                return 0;
            }
        }

        Sym = AdvanceToNonWhiteSpaceChar(Sym);
    }

    return Cursor->Element;
}

extern "C"
{

// TODO(joe): Update this file so that this is the only symbol that is accessible.
element_node * ParseFeed(char *FeedUrl)
{
    feed_buffer FeedBuffer = {};
    FeedBuffer.MaximumSize = 1100000;
    FeedBuffer.Data = (char *)calloc(FeedBuffer.MaximumSize, sizeof(char));

#if TEST_FEED
    DEBUGReadFeedFromFile(&FeedBuffer, "feed.xml");
#else
    FetchFeed(&FeedBuffer, FeedUrl);
#endif

    if (FeedBuffer.Valid)
    {
        parser Parser = {};
        element_node *FeedRoot = ParseFeed(&FeedBuffer, &Parser);
        return FeedRoot;
    }
    else
    {
        printf("Invalid Feed!\n");
    }

    free(FeedBuffer.Data);

    return 0;
}

static void VisitAttribute(attribute_node *Attribute)
{
    printf("%s -> %s\n", Attribute->Name, Attribute->Value);
}

static void VisitElement(element_node *Element)
{
    printf("%s -> %s\n", Element->Name, Element->Value);

    for (int Index = 0; Index < Element->AttributeCount; ++Index)
    {
        VisitAttribute(Element->Attributes + Index);
    }

    if (Element->FirstChild)
    {
        VisitElement(Element->FirstChild);
    }
    
    if (Element->Next)
    {
        VisitElement(Element->Next);
    }
}

void PrintFeed(element_node *Root)
{
    VisitElement(Root);
}

element_node *GetFirstChildWithName(element_node *Root, char *Name)
{
    element_node *Result = 0;

    element_node *Current = Root;
    bool Found = false;
    if (Current->FirstChild)
    {
        Current = Current->FirstChild;
        if (AreStringsEqual(Current->Name, Name))
        {
            Found = true;
        }
        else
        {
            while(Current->Next && !Found)
            {
                Current = Current->Next;
                if (AreStringsEqual(Current->Name, Name))
                {
                    Found = true;
                }
            }
        }
    }

    if (Found)
    {
        Result = Current;
    }

    return Result;
}

attribute_node *GetAttributeWithName(element_node *Element, char *Name)
{
    attribute_node *Result = 0;
    for (int Index = 0; Index < Element->AttributeCount; ++Index)
    {
        attribute_node *Current = Element->Attributes + Index;
        if (AreStringsEqual(Current->Name, Name))
        {
            Result = Current;
            break;
        }
    }

    return Result;
}

}

