#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*---------------------------structure------------------------------*/
// Token type
typedef enum{
    TK_RESERVED, //記号 1
    TK_NUM,      //整数トークン 2
    TK_EOF,      //入力の終わりを表すトークン 3
} TokenKind;

// Node type
typedef enum{
    ND_ADD, // +
    ND_SUB, // -
    ND_MUL, // *
    ND_DIV, // /
    ND_EQ,  // ==
    ND_NE,  // !=
    ND_LT,  // <
    ND_LE,  // <=
    ND_NUM, //整数
}NodeKind;

typedef struct Token Token;
typedef struct Node Node;

//トークン型
struct Token{
    TokenKind kind; //トークンの型
    Token *next;    //次の入力トークン
    int val;        //kindがTK_NUMの場合、その数値
    char *str;      //トークン文字列
    int len;        //トークンの長さ
};
//ノード型
struct Node{
    NodeKind kind;
    Node *lhs;
    Node *rhs;
    int val;
};
/*-----------------------------functions-----------------------------------*/

char *user_input;

//現在着目しているトークン
Token *token;

//エラーを報告するための関数
//printfと同じ引数を取る
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}
//...は可変長引数 https://qiita.com/subaruf/items/657c67a1809515589a7c
void error_at(char *loc, char *fmt, ...){
    va_list ap;
    va_start(ap, fmt);

    int pos = loc - user_input;
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, " ");
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

//次のトークンが期待している記号のときには、トークンを1つ読み進めて真を返す。
//それ以外の場合には偽を返す。
bool consume(char *op){
    if(token->kind != TK_RESERVED || 
       strlen(op) != token->len ||
       memcmp(token->str, op, token->len))
        return false;
    token = token->next;
    return true;
}

//次のトークンが期待している記号のときには、トークンを1つ読み進める。
//それ以外の場合にはエラーを報告する。
void expect(char *op){
    if(token->kind != TK_RESERVED || strlen(op) != token->len || 
       memcmp(token->str, op, token->len))
      error_at(token->str, "'%c'ではありません", op);
    token = token->next;
}

//次のトークンが数値のときには、トークンを1つ読み進めてその数値を返す。
//それ以外の場合にはエラーを報告する。
int expect_number(){
    if(token->kind != TK_NUM)
        error_at(token->str, "数ではありません");
    int val = token->val;
    token = token->next;
    return val;
}

bool at_eof(){
    return token->kind == TK_EOF;
}

//新しいトークンを作成してcurに繋げる
Token *new_token(TokenKind kind, Token *cur, char *str, int len){
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind; //トークンの型
    tok->str = str; //トークン文字列
    tok->len = len; //トークンの長さ
    cur->next = tok; //curの次のトークンをtokにする
    return tok;
}

bool startswith(char *p, char *q) {
  return memcmp(p, q, strlen(q)) == 0;
}
//入力文字列pをトークナイズしてそれを返す
Token *tokenize() {
  char *p = user_input;
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while (*p) {
    // Skip whitespace characters.
    if (isspace(*p)) {
      p++;
      continue;
    }

    // Multi-letter punctuator
    if (startswith(p, "==") || startswith(p, "!=") ||
        startswith(p, "<=") || startswith(p, ">=")) {
      cur = new_token(TK_RESERVED, cur, p, 2);
      p += 2;
      continue;
    }

    // Single-letter punctuator
    if (strchr("+-*/()<>", *p)) {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }

    // Integer literal
    if (isdigit(*p)) {
      cur = new_token(TK_NUM, cur, p, 0);
      char *q = p;
      cur->val = strtol(p, &p, 10);
      cur->len = p - q;
      continue;
    }

    error_at(p, "invalid token");
  }
  new_token(TK_EOF, cur, p, 0);
  return head.next;
}


Node *new_node(NodeKind kind) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  return node;
}
Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}
Node *new_num(int val) {
  Node *node = new_node(ND_NUM);
  node->val = val;
  return node;
}

Node *expr();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *primary();

//expr = equality
Node *expr() {
  return equality();
}

//equality = relational ("==" relational | "!=" relational)*
Node *equality() {
  Node *node = relational();

  for (;;) {
    if (consume("=="))
      node = new_binary(ND_EQ, node, relational());
    else if (consume("!="))
      node = new_binary(ND_NE, node, relational());
    else
      return node;
  }
}

//relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational() {
  Node *node = add();

  for (;;) {
    if (consume("<"))
      node = new_binary(ND_LT, node, add());
    else if (consume("<="))
      node = new_binary(ND_LE, node, add());
    else if (consume(">"))
      node = new_binary(ND_LT, add(), node);
    else if (consume(">="))
      node = new_binary(ND_LE, add(), node);
    else
      return node;
  }
}

//add = mul ("+" mul | "-" mul)*
Node *add() {
  Node *node = mul();

  for (;;) {
    if (consume("+"))
      node = new_binary(ND_ADD, node, mul());
    else if (consume("-"))
      node = new_binary(ND_SUB, node, mul());
    else
      return node;
  }
}

//mul = unary ("*" unary | "/" unary)*
Node *mul(){
    Node *node = unary();

    for(;;){
        if(consume("*"))
            node = new_binary(ND_MUL, node, unary());
        else if(consume("/"))
            node = new_binary(ND_DIV, node, unary());
        else
            return node;
    }
}
Node *unary(){
    if(consume("+"))
        return unary();
    if(consume("-"))
        return new_binary(ND_SUB, new_num(0), unary());
    return primary();
}
Node *primary(){
    //次のトークンが"("なら、"(" expr ")"のはず
    if(consume("(")){
        Node *node = expr();
        expect(")");
        return node;
    }

    //そうでなければ数値のはず
    return new_num(expect_number());
}
//
//gen関数（スタック操作命令）
//
void gen(Node *node) {
  if (node->kind == ND_NUM) {
    printf("  push %d\n", node->val);
    return;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->kind) {
  case ND_ADD:
    printf("  add rax, rdi\n");
    break;
  case ND_SUB:
    printf("  sub rax, rdi\n");
    break;
  case ND_MUL:
    printf("  imul rax, rdi\n");
    break;
  case ND_DIV:
    printf("  cqo\n");
    printf("  idiv rdi\n");
    break;
  case ND_EQ:
    printf("  cmp rax, rdi\n");
    printf("  sete al\n");
    printf("  movzb rax, al\n");
    break;
  case ND_NE:
    printf("  cmp rax, rdi\n");
    printf("  setne al\n");
    printf("  movzb rax, al\n");
    break;
  case ND_LT:
    printf("  cmp rax, rdi\n");
    printf("  setl al\n");
    printf("  movzb rax, al\n");
    break;
  case ND_LE:
    printf("  cmp rax, rdi\n");
    printf("  setle al\n");
    printf("  movzb rax, al\n");
    break;
  }

  printf("  push rax\n");
}
/*----------------------------MainFunction-----------------------------*/

int main(int argc, char **argv) {
  if (argc != 2) {
    error("引数の個数が正しくありません\n", argv[0]);
    return 1;
  }

  //トークナイズしてパースする
  user_input = argv[1];
  token = tokenize(user_input);
  Node *node = expr();

  //アセンブリの前半部分を出力
  printf(".intel_syntax noprefix\n");
  printf(".globl main\n");
  printf("main:\n");

  //抽象構文木を下りながらコード生成
  gen(node);

  printf("  pop rax\n");
  printf("  ret\n");
  return 0;

}