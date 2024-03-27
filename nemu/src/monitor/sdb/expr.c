/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256, TK_EQ,

  /* TODO: Add more token types */
  TK_INT,
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces
  {"\\+", '+'},         // plus
  {"-", '-'},         // plus
  {"\\*", '*'},         // plus
  {"/", '/'},         // plus
  {"==", TK_EQ},        // equal

  {"[0-9]+", TK_INT},
  {"\\(" , '('},
  {"\\)" , ')'}

};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        if(rules[i].token_type == TK_NOTYPE)
          break;

        assert(nr_token<32);
        tokens[nr_token].type  = rules[i].token_type;
        
        if(rules[i].token_type == TK_INT) {
            assert(substr_len<32);
            memcpy(tokens[nr_token].str , substr_start , substr_len);
        }
        nr_token++;

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

void dump_tokens(int p , int q , char * out){
  for(int i = p;i<=q;i++){
    if(tokens[i].type == TK_INT) {
      sprintf(out,"%s" , tokens[i].str);
      out += strlen(tokens[i].str);
    }else
      sprintf(out++,"%c" , tokens[i].type);
  }

  // sprintf(out++,"\n");
}


int check_parentheses(int p , int q) {
  if(q-p<2)
    return -1;
  int match = 0;
  for(int i = p;i<q+1;i++){
    if(tokens[i].type == '('){
      match++;
    }
    if(tokens[i].type == ')'){
      match--;
    }
  }

  if(tokens[p].type == '(' && tokens[q].type == ')')
    return match != 0 ? -2 : 0;
  else
    return -3;
}

int find_main_op(int p , int q , char *op_type){
  int matching = 0;
  *op_type = 'u';
  int index = p;

  for(int i = p;i<q+1;i++){
    if(tokens[i].type == '(' )
      matching++;
    
    if(tokens[i].type == ')')
      matching--;

    
    if(matching==0 && i != q){
      bool newIsMul = tokens[i].type == '*' || tokens[i].type == '/';
      bool newIsAdd = tokens[i].type == '+' || tokens[i].type == '-' ;
      bool oldIsAdd = *op_type == '+' || *op_type == '-' ;

      if(newIsAdd || (newIsMul && !oldIsAdd)){
        *op_type = tokens[i].type;
      index = i;
      }
    }
  }

  if(*op_type == 'u'){
    assert(0);
  }else{
    return index;
  }
}

uint32_t eval(int p , int q) {
  char buf [100];
  dump_tokens(p,q,buf);
  printf("%s\n",buf );
  if(p>q){
    assert(0);
  }else if(p == q){
    return atoi(tokens[p].str);
  }else if(check_parentheses(p,q) == 0){
    eval(p+1,q-1);
  }else{
    char op_type ;
    int op = find_main_op(p,q , &op_type);
    uint32_t val1 = eval(p,op-1);
    uint32_t val2 = eval(op+1,q);
    switch(op_type)
      {
        case '+':
          return val1 + val2;
        case '-':
          return val1 - val2;
        case '*':
          return val1 * val2;
        case '/':
          return val1 / val2;
        default:
          assert(0);
      }
  }
  return 0;
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  int result =  eval(0 , nr_token-1);
  printf("%s result is %d\n" , e , result);

  return result;

}
