#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Token types
enum TokenType
{
    TOK_INT, TOK_MAIN, TOK_RETURN, TOK_NUMBER, TOK_SEMICOLON,
    TOK_LBRACE, TOK_RBRACE, TOK_LPAREN, TOK_RPAREN, TOK_EOF, TOK_UNKNOWN
};

// Token structure
typedef struct Token
{
    enum TokenType type;
    char* value;
} Token;

// Node types for AST
enum NodeType
{
    NODE_RETURN, NODE_NUMBER
};

// AST node structure
typedef struct Node
{
    enum NodeType type;
    int value;
    struct Node* left;
} Node;

// Global variables
char* input;            // Input C code
size_t input_size;
size_t pos;             // Current position in input
Token* tokens;          // Array of tokens
size_t token_count;     
size_t token_pos;       // Current token position
FILE* output;           // Output assembly file

// Function signatures
void compile(const char* input_file, const char* output_file);
void read_input(const char* filename);
void tokenize();
void generate_code(Node* node);
void expect(enum TokenType type);
void free_ast(Node *node);
Node* parse_main();
Node* parse_return();
Node* parse_number();
Token next_token();

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <input.c> <output>\n", argv[0]);
        return 1;
    }

    char asmFile[50] = {0};
    strcpy(asmFile, argv[2]);
    strcat(asmFile, ".asm");

    compile(argv[1], asmFile);

    char asmCommand[50] = {0};
    strcpy(asmCommand, "fasm ");
    strcat(asmCommand, asmFile);

    system(asmCommand);
    return 0;
}

void compile(const char* input_file, const char* output_file)
{
    read_input(input_file);
    tokenize();
    token_pos = 0;
    Node* ast = parse_main();

    output = fopen(output_file, "w");
    if (!output)
    {
        fprintf(stderr, "Error: cannot open output file %s\n", output_file);
        exit(1);
    }

    generate_code(ast);
    fclose(output);

    // Clean up
    free_ast(ast);
    for(size_t i = 0; i < token_count; i++)
    {
        if (tokens[i].value) free(tokens[i].value);
    }
    free(tokens);
    free(input);
}

void read_input(const char* filename)
{
    FILE* file = fopen(filename, "r");
    if (!file)
    {
        fprintf(stderr, "Error: cannot open input file %s\n", filename);
        exit(1);
    }

    // Get amount of characters in the file
    fseek(file, 0, SEEK_END);
    input_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read the characters from the file
    input = (char*)malloc(input_size + 1);
    fread(input, 1, input_size, file);
    input[input_size] = '\0';
    fclose(file);
}

void tokenize()
{
    tokens = malloc(input_size * sizeof(Token)); // Correct allocation size
    token_count = 0;
    pos = 0;
    while (1)
    {
        Token token = next_token();
        tokens[token_count++] = token;
        if (token.type == TOK_EOF) break;
    }
}

// Lexer: Get next token
Token next_token()
{
    Token token = {TOK_UNKNOWN, NULL};
    while (pos < input_size && isspace(input[pos])) pos++;

    if (pos >= input_size)
    {
        token.type = TOK_EOF;
        return token;
    }

    if (strncmp(&input[pos], "int", 3) == 0 && !isalnum(input[pos + 3]))
    {
        token.type = TOK_INT;
        pos += 3;
    }
    else if (strncmp(&input[pos], "main", 4) == 0 && !isalnum(input[pos + 4]))
    {
        token.type = TOK_MAIN;
        pos += 4;
    }
    else if (strncmp(&input[pos], "return", 6) == 0 && !isalnum(input[pos + 6]))
    {
        token.type = TOK_RETURN;
        pos += 6;
    }
    else if (isdigit(input[pos]))
    {
        char* start = &input[pos];
        while (isdigit(input[pos]))
        {
            pos++;
        }
        size_t len = &input[pos] - start;
        token.type = TOK_NUMBER;
        token.value = malloc(len + 1);
        strncpy(token.value, start, len);
        token.value[len] = '\0';
    }
    else if (input[pos] == ';')
    {
        token.type = TOK_SEMICOLON;
        pos++;
    }
    else if (input[pos] == '{')
    {
        token.type = TOK_LBRACE;
        pos++;
    }
    else if (input[pos] == '}')
    {
        token.type = TOK_RBRACE;
        pos++;
    }
    else if (input[pos] == '(')
    {
        token.type = TOK_LPAREN;
        pos++;
    }
    else if (input[pos] == ')')
    {
        token.type = TOK_RPAREN;
        pos++;
    }
    else
    {
        token.type = TOK_UNKNOWN;
    }

    return token;
}

void generate_code(Node* node)
{
    fprintf(output, "format ELF64 executable 3\n");
    fprintf(output, "entry main\n");
    fprintf(output, "segment readable executable\n");
    fprintf(output, "main:\n");

    if (node->type == NODE_RETURN)
    {
        fprintf(output, "    mov rax, %d\n", node->left->value);
        fprintf(output, "    mov rdi, rax\n");
        fprintf(output, "    mov rax, 60\n"); // sys_exit
        fprintf(output, "    syscall\n");
    }
    
    fprintf(output, "segment readable writable\n");
}

void expect(enum TokenType type)
{
    if (token_pos >= token_count || tokens[token_pos].type != type)
    {
        fprintf(stderr, "Error: Expected token type %d, got %d at position %zu\n", type, tokens[token_pos].type, token_pos);
        exit(1);
    }
    token_pos++;
}

Node* parse_main()
{
    expect(TOK_INT);
    expect(TOK_MAIN);
    expect(TOK_LPAREN);
    expect(TOK_RPAREN);
    expect(TOK_LBRACE);
    Node* node = parse_return();
    expect(TOK_RBRACE);
    return node;
}

Node* parse_return()
{
    expect(TOK_RETURN);
    Node* node = malloc(sizeof(Node));
    node->type = NODE_RETURN;
    node->left = parse_number();
    expect(TOK_SEMICOLON);
    return node;
}

Node* parse_number()
{
    Node* node = malloc(sizeof(Node));
    node->type = NODE_NUMBER;
    node->value = atoi(tokens[token_pos].value);
    node->left = NULL;
    expect(TOK_NUMBER);
    return node;
}

void free_ast(Node *node)
{
    if (!node) return;
    free_ast(node->left);
    free(node);
}