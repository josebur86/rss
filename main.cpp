#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <curl/curl.h>


#define TEST_FEED 1
#define READER_DEBUG 1

struct feed_buffer
{
    bool Valid;

    char *Data;
    size_t Size;
    size_t MaximumSize;
};

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

    element_node *FirstChild; // NOTE(joe): This list should be in order.
};

element_node * PushElement()
{
    // TODO(joe): Stop allocating from the heap.
    element_node *Result = (element_node *)calloc(1, sizeof(element_node));

    return Result;
}

attribute_node * PushAttribute()
{
    attribute_node *Result = (attribute_node *)calloc(1, sizeof(attribute_node));
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
    parser_cursor *Cursor = Parser->Cursors + Parser->CurrentIndex++;
    Cursor->State = State;
    Cursor->Element = Element;

    return Cursor;
}

static parser_cursor * PopAndGetCurrentCursor(parser *Parser)
{
    parser_cursor *Cursor = Parser->Cursors + --Parser->CurrentIndex;
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

#if 1

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

parse_op_result OnParseProlog(parser *Parser, char *Sym, parser_cursor *Cursor)
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
                
parse_op_result OnParseBeginElement(parser *Parser, char *Sym, parser_cursor *Cursor)
{
    char ElementName[32] = "\0";
    size_t NameSize = 0;

    bool Continue = true;
    while (Sym && Continue)
    {
        if (*Sym == '<')
        {
            Cursor->Element = PushElement();

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

parse_op_result OnParseResumeBeginElement(parser *Parser, char *Sym, parser_cursor *Cursor)
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
             // TODO(joe): I think we'll need to pop the the current element's parent here.
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

parse_op_result OnParseAttribute(parser *Parser, char *Sym, parser_cursor *Cursor)
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
    while (Sym)
    {
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
#else

/* TODO(joe): 
 *  - Now that I'm more comfortable with how this is shaping up I think it's time to work in
 *  grabbing the data out of the xml itself. The parser can still have it's own state but the data
 *  should go into it's own set of tree nodes. 
 */
static void ParseFeed(feed_buffer *FeedBuffer, parser *Parser)
{
    SetCurrentState(Parser, ParseStart);

    element_node *CurrentElement = 0;
    attribute_node *CurrentAttribute = 0;

    char *Sym = FeedBuffer->Data;
    while (Sym)
    {
        switch (GetCurrentState(Parser))
        {
            case ParseStart:
            {
                //printf("ParseStart\n");
                if (*Sym == '<' && (Sym+1) && *(Sym+1) == '?')
                {
                    SetCurrentState(Parser, ParseProlog);
                    ++Sym;
                }
                else
                {
                    SetCurrentState(Parser, ParseBeginElement);
                }
            } break;
            case ParseProlog:
            {
                //printf("ParseProlog\n");
                if (*Sym == '?' && (Sym+1) && *(Sym+1) == '>')
                {
                    SetCurrentState(Parser, ParseBeginElement);
                    ++Sym;
                }
                else if (!Sym)
                {
                    SetCurrentState(Parser, ParseError);
                }
            } break;
            case ParseBeginElement:
            {
                Sym = AdvanceToNonWhiteSpaceChar(Sym);

                //printf("ParseBeginElement\n");
                char ElementName[32] = "\0";
                size_t NameSize = 0;
                if (*Sym == '<')
                {
                    CurrentElement = PushElement();

                    while (++Sym && *Sym != ' ' && *Sym != '>')
                    {
                        ElementName[NameSize++] = *Sym;
                    }
                    ElementName[NameSize] = '\0';
                    CurrentElement->Name = CopyString(ElementName);

                    if (*Sym == ' ')
                    {
                        PushState(Parser, ParseAttributeName);
                    }
                    else if (*Sym == '/' && (Sym+1) && *(Sym+1) == '>')
                    {

                    }
                    else if (*Sym == '>')
                    {
                        Sym = AdvanceToNonWhiteSpaceChar(++Sym);
                        if (Sym)
                        {
                            if (*Sym == '<')
                            {
                                --Sym;
                                SetCurrentState(Parser, ParseBeginElement);
                            }
                            else
                            {
                                --Sym;
                                SetCurrentState(Parser, ParseElementValue);
                            }
                        }
                        else
                        {
                            SetCurrentState(Parser, ParseError);
                        }
                    }
                    printf("ElementName: %s\n", ElementName);
                }
                else if (Sym)
                {
                    if (*Sym == '>')
                    {
                        // TODO(joe): I wonder if it would be better to switch to grabbing the
                        // element value here and let that code decide whether to parse a nested
                        // element tree?
                        Sym = AdvanceToNonWhiteSpaceChar(++Sym);
                        if (*Sym == '<')
                        {
                            --Sym;
                            SetCurrentState(Parser, ParseBeginElement);
                        }
                        else
                        {
                            --Sym;
                            SetCurrentState(Parser, ParseElementValue);
                        }
                    }
                    else if (*Sym == '/' && (Sym+1) && *(Sym+1) == '>')
                    {
                        Sym += 2;
                        Sym = AdvanceToNonWhiteSpaceChar(Sym);
                        if (*Sym == '<')
                        {
                            --Sym;
                            SetCurrentState(Parser, ParseBeginElement);
                        }
                        else
                        {
                            --Sym;
                            SetCurrentState(Parser, ParseElementValue);
                        }                    
                    }
                    else
                    {
                        --Sym;
                        PushState(Parser, ParseAttributeName);
                    }
                }
            } break;
            case ParseEndElement:
            {
                while(Sym)
                {
                    if (*Sym == '>')
                    {
                        SetCurrentState(Parser, ParseBeginElement);
                        break;
                    }
                    ++Sym;
                }

                if (!Sym)
                {
                    SetCurrentState(Parser, ParseError);
                }
            } break;
            case ParseElementValue:
            {
                Sym = AdvanceToNonWhiteSpaceChar(Sym);
                char ElementValue[2048] = "\0";
                size_t ValueSize = 0;
                while (Sym)
                {
                    if (*Sym == '<' && (Sym+1) && *(Sym+1) != '!')
                    {
                        ElementValue[ValueSize++] = '\0';
                        SetCurrentState(Parser, ParseEndElement);
                        break;
                    }

                    ElementValue[ValueSize++] = *Sym;
                    ++Sym;
                }
            } break;
            case ParseAttributeName:
            {
                CurrentAttribute = CurrentElement->Attributes + CurrentElement->AttributeCount++;

                //printf("ParseAttributeName\n");
                char AttributeName[32] = "\0";
                size_t NameSize = 0;
                while (Sym)
                {
                    if (*Sym != '=')
                    {
                        AttributeName[NameSize++] = *Sym;
                    }
                    else if (*Sym == '=')
                    {
                        AttributeName[NameSize] = '\0';
                        printf("AttributeName: %s\n", AttributeName);
                        CurrentAttribute->Name = CopyString(AttributeName);
                        SetCurrentState(Parser, ParseAttributeValue);
                        break;
                    }
                    else
                    {
                        printf("Xml is in an unexpected format.\n");
                    }

                    ++Sym;
                }
            } break;
            case ParseAttributeValue:
            {
                //printf("ParseAttributeValue\n");
                char AttributeValue[32] = "\0";
                size_t ValueSize = 0;

                if (Sym && *Sym == '"')
                {
                    ++Sym;
                    while (Sym)
                    {
                        if (*Sym != '"')
                        {
                            AttributeValue[ValueSize++] = *Sym;
                        }
                        else if (*Sym == '"')
                        {
                            AttributeValue[ValueSize] = '\0';
                            printf("AttributeValue: %s\n", AttributeValue);
                            CurrentAttribute->Value = CopyString(AttributeValue);
                            CurrentAttribute = 0;
                            PopState(Parser);
                            break;
                        }

                        ++Sym;
                    }
                }
            } break;
            case ParseError:
            default:
            {
                printf("ParseError\n");
                printf("Xml is in an unexpected format.\n");
                return;
            }
        }

        ++Sym;
    }
}
#endif

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
        ParseFeed(&FeedBuffer, &Parser);
    }
    else
    {
        printf("Invalid Feed!\n");
    }

    free(FeedBuffer.Data);

    return 0;
}
