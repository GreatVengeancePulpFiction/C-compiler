#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Token types
enum TokenType
{
    TOK_INT, TOK_IDENTIFIER, TOK_RETURN, TOK_NUMBER, TOK_SEMICOLON,
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
    NODE_PROGRAM, NODE_FUNCTION, NODE_CALL, NODE_STMT_LIST, NODE_RETURN, NODE_NUMBER
};

// AST node structure
typedef struct Node
{
    enum NodeType type;
    char* func_name;    // For function nodes
    int value;          // For number nodes
    struct Node* body;  // For function nodes
    struct Node* left;  // For return nodes
    struct Node* next;  // For program node to link functions
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
Node* parse_program();
Node* parse_function();
Node* parse_stmt_list();
Node* parse_stmt();
Node* parse_call();
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
    Node* ast = parse_program();

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
    else if (strncmp(&input[pos], "return", 6) == 0 && !isalnum(input[pos + 6]))
    {
        token.type = TOK_RETURN;
        pos += 6;
    }
    else if (isalpha(input[pos]))
    {
        char* start = &input[pos];
        while (isalnum(input[pos]))
        {
            pos++;
        }
        size_t len = &input[pos] - start;
        token.type = TOK_IDENTIFIER;
        token.value = malloc(len + 1);
        strncpy(token.value, start, len);
        token.value[len] = '\0';
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
    fprintf(output, "entry start\n");
    fprintf(output, "segment readable executable\n");

    // Generate code for all functions
    Node* current = node->next; // Skip PROGRAM node
    while (current)
    {
        if (current->type == NODE_FUNCTION)
        {
            if (!current->func_name) {
                fprintf(stderr, "Error: Function name is null\n");
                exit(1);
            }
            fprintf(output, "%s:\n", current->func_name);
            fprintf(output, "    push rbp\n");
            fprintf(output, "    mov rbp, rsp\n");

            // Generate code for function body statements
            Node* stmt = current->body;
            while (stmt)
            {
                if (stmt->type == NODE_RETURN)
                {
                    if (!stmt->left) {
                        fprintf(stderr, "Error: Return statement has no expression\n");
                        exit(1);
                    }
                    if (stmt->left->type == NODE_NUMBER)
                    {
                        fprintf(output, "    mov rax, %d\n", stmt->left->value);
                    }
                    else if (stmt->left->type == NODE_CALL)
                    {
                        if (!stmt->left->func_name) {
                            fprintf(stderr, "Error: Function call name is null in return\n");
                            exit(1);
                        }
                        fprintf(output, "    call %s\n", stmt->left->func_name);
                    }
                    else
                    {
                        fprintf(stderr, "Error: Invalid return expression type %d\n", stmt->left->type);
                        exit(1);
                    }
                    fprintf(output, "    pop rbp\n");
                    fprintf(output, "    ret\n\n");
                }
                else if (stmt->type == NODE_CALL)
                {
                    if (!stmt->func_name) {
                        fprintf(stderr, "Error: Function call name is null\n");
                        exit(1);
                    }
                    fprintf(output, "    call %s\n", stmt->func_name);
                }
                else
                {
                    fprintf(stderr, "Error: Invalid statement type %d\n", stmt->type);
                    exit(1);
                }
                stmt = stmt->next;
            }
        }
        current = current->next;
    }

    fprintf(output, "start:\n");
    fprintf(output, "    call main\n");
    fprintf(output, "    mov rdi, rax\n");
    fprintf(output, "    mov rax, 60\n"); // sys_exit
    fprintf(output, "    syscall\n");

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

Node* parse_program()
{
    Node* program = (Node*)malloc(sizeof(Node));
    program->type = NODE_PROGRAM;
    program->next = NULL;
    Node* current = program;

    while (token_pos < token_count && tokens[token_pos].type != TOK_EOF)
    {
        Node* func = parse_function();
        current->next = func;
        current = func;
    }
    return program;
}

Node* parse_function()
{
    expect(TOK_INT);
    Node* node = (Node*)malloc(sizeof(Node));
    node->type = NODE_FUNCTION;
    node->next = NULL;

    if (tokens[token_pos].type != TOK_IDENTIFIER)
    {
        fprintf(stderr, "Error: Expected function name at position %zu\n", token_pos);
        exit(1);
    }
    node->func_name = strdup(tokens[token_pos].value);
    expect(TOK_IDENTIFIER);

    expect(TOK_LPAREN);
    expect(TOK_RPAREN); // For now, supporting functions without parameters
    expect(TOK_LBRACE);
    node->body = parse_stmt_list();
    expect(TOK_RBRACE);

    return node;
}

Node* parse_stmt_list()
{
    Node* list = (Node*)malloc(sizeof(Node));
    if (!list)
    {
        fprintf(stderr, "Error: Memory allocation failed for statement list\n");
        exit(1);
    }
    list->type = NODE_STMT_LIST;
    list->next = NULL;
    list->left = NULL;
    list->body = NULL;
    list->func_name = NULL;

    Node* current = list;
    Node* prev = NULL;

    while (token_pos < token_count && tokens[token_pos].type != TOK_RBRACE)
    {
        Node* stmt = parse_stmt();
        if (!stmt)
        {
            fprintf(stderr, "Error: Failed to parse statement at position %zu\n", token_pos);
            exit(1);
        }
        if (prev)
        {
            prev->next = stmt;
        }
        else
        {
            list->next = stmt;
        }
        prev = stmt;
    }

    Node* result = list->next;
    free(list); // Free the dummy list node
    return result;
}

Node* parse_stmt()
{
    if (token_pos >= token_count)
    {
        fprintf(stderr, "Error: Unexpected end of input at position %zu\n", token_pos);
        exit(1);
    }

    Node* stmt;
    if (tokens[token_pos].type == TOK_RETURN)
    {
        stmt = parse_return();
    }
    else if (tokens[token_pos].type == TOK_IDENTIFIER)
    {
        stmt = parse_call();
    }
    else
    {
        fprintf(stderr, "Error: Expected statement at position %zu, got token type %d\n", token_pos, tokens[token_pos].type);
        exit(1);
    }
    expect(TOK_SEMICOLON);
    return stmt;
}


Node* parse_return()
{
    expect(TOK_RETURN);
    Node* node = (Node*)malloc(sizeof(Node));
    node->type = NODE_RETURN;
    node->next = NULL;

    if (tokens[token_pos].type == TOK_NUMBER)
    {
        node->left = parse_number();
    }
    else if (tokens[token_pos].type == TOK_IDENTIFIER)
    {
        node->left = parse_call();
    }
    else
    {
        fprintf(stderr, "Error: Expected number or function call at position %zu\n", token_pos);
        exit(1);
    }
    return node;
}

Node* parse_call()
{
    Node* node = (Node*)malloc(sizeof(Node));
    if (!node) {
        fprintf(stderr, "Error: Memory allocation failed for call node\n");
        exit(1);
    }
    node->type = NODE_CALL;
    node->next = NULL;
    node->left = NULL;
    if (tokens[token_pos].type != TOK_IDENTIFIER) {
        fprintf(stderr, "Error: Expected identifier for function call at position %zu\n", token_pos);
        exit(1);
    }
    node->func_name = strdup(tokens[token_pos].value);
    if (!node->func_name) {
        fprintf(stderr, "Error: Memory allocation failed for function name\n");
        exit(1);
    }
    expect(TOK_IDENTIFIER);
    expect(TOK_LPAREN);
    expect(TOK_RPAREN); // Supporting parameterless call only

    return node;
}

Node* parse_number()
{
    Node* node = malloc(sizeof(Node));
    node->type = NODE_NUMBER;
    node->value = atoi(tokens[token_pos].value);
    node->left = NULL;
    node->next = NULL;
    expect(TOK_NUMBER);
    return node;
}

void free_ast(Node* node)
{
    if (!node) return;

    if (node->type == NODE_PROGRAM || node->type == NODE_FUNCTION)
    {
        free_ast(node->next);
        if (node->type == NODE_FUNCTION)
        {
            free(node->func_name);
            free_ast(node->body);
        }
    }
    else if (node->type == NODE_RETURN)
    {
        free_ast(node->left);
        free_ast(node->next);
    }
    else if (node->type == NODE_CALL)
    {
        if (node->func_name) {
            free(node->func_name);
        }
        free_ast(node->next);
    }
    free(node);
}