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

word_t vaddr_read(vaddr_t addr, int len);


enum {
  TK_NOTYPE = 256,
  TK_POS, // postive number
  TK_NEG, // negitive number
  TK_DEREF, // Deference *
  TK_EQ, // ==
  TK_NEQ, // != 
  TK_GT, // >
  TK_LT, // < 
  TK_GE, // >=
  TK_LE, // <=
  TK_AND, // &
  TK_OR, // ||
  TK_NUM, // 10 & 16
  TK_REG, // register
  TK_VAR, // variance 
};


static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence(优先级) level of different rules.
   */


  {" +", TK_NOTYPE},    // spaces
  {"\\(", '('}, {"\\)", ')'},
  {"\\*", '*'}, // dereference & multiply

  {"\\+", '+'},{"-", '-'}, {"/", '/'}, // [+ - /]
  {"<", TK_LT}, {">", TK_GT}, {"<=", TK_LE}, {">=", TK_GE},
  {"==", TK_EQ}, {"!=", TK_NEQ}, 
  {"&&", TK_AND}, {"\\|\\|", TK_OR},
  // {"0x[a-f0-9]+", TK_NUM},
  {"[0-9]+|0x[a-f0-9]+", TK_NUM}, // TODO: non-capture notation (?:pattern) makes compilation failed
  
  {"\\$[a-zA-Z0-9]+", TK_REG},
  {"[A-Za-z_]\\w*", TK_VAR},
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


        if (rules[i].token_type == TK_NOTYPE) break;
        tokens[nr_token].type = rules[i].token_type;


        switch (rules[i].token_type) {
          case TK_NUM:
          case TK_REG:
          case TK_VAR:
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
            // todo: handle overflow (token exceeding size of 32B)
          
          case '+': case '-': case '*':
            if(nr_token == 0 || !(tokens[nr_token-1].type == ')' || tokens[nr_token-1].type == TK_NUM \
              || tokens[nr_token-1].type == TK_REG || tokens[nr_token-1].type == TK_VAR)){
                switch (rules[i].token_type){
                  case '-': tokens[nr_token].type = TK_NEG; break;
                  case '+': tokens[nr_token].type = TK_POS; break;
                  case '*': tokens[nr_token].type = TK_DEREF; break;
                }
            }
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
 
 
  // for(int i = 0; i < nr_token; i++) printf("%d\n", tokens[i].type);

  return true;
}

bool check_parentheses(int p, int q) {
  if (tokens[p].type=='(' && tokens[q].type==')') {
    int par = 0;
    for (int i = p; i <= q; i++) {
      if (tokens[i].type=='(') par++;
      else if (tokens[i].type==')') par--;

      if (par == 0) return i==q; // the leftest parenthese is matched
    }
  }
  return false;
}

// 找主运算符
int find_opt(int p, int q) {
  int ret = -1, par = 0, op_type = 0;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == TK_NUM || tokens[i].type == TK_VAR || tokens[i].type == TK_REG) {
      continue;
    }
    if (tokens[i].type == '(') {
      par++;
    } else if (tokens[i].type == ')') {
      if (par == 0) {
        return -1;
      }
      par--;
    } else if (par > 0) {
      continue;
    } else {
      int tmp_type = 0;
      switch (tokens[i].type) { //   优先级比较，递归处理，优先级反转

      case TK_NEG: case TK_POS: case TK_DEREF: tmp_type = 1; break;
      case '*': case '/': tmp_type = 2; break;
      case '+': case '-': tmp_type = 3; break;
      case TK_GT: case TK_LT: case TK_LE: case TK_GE: tmp_type = 4; break;
      case TK_EQ: case TK_NEQ: tmp_type = 5; break;
      case TK_AND: tmp_type = 6; break;
      case TK_OR: tmp_type = 7; break; 
      default: assert(0);
      }
      if (tmp_type > op_type) {
        op_type = tmp_type;
        ret = i;
      }
    }
  }
  if (par != 0) return -1;
  return ret;
}


sword_t eval(int p, int q, bool *success) {
  *success = true;
  if (p > q) {
    /* Bad expression */
    *success = false;
    return 0;
  }
  else if (p == q) {
    /* Single token.
     * For now this token should be a number.
     * Return the value of the number.
     */
    if(tokens[p].type == TK_REG){
      sword_t reg_val = isa_reg_str2val(tokens[p].str, success);
      if(*success) return reg_val;
      *success = false;
      Log("unclear register!");
      return 0;
    }
    int num_len = strlen(tokens[p].str);
    if(num_len > 1){

      // 16进制
      if(tokens[p].str[0] == '0' && tokens[p].str[1] == 'x') return strtol(tokens[p].str, NULL, 16);
      
      // 8进制
      else if(tokens[p].str[0] == '0') return strtol(tokens[p].str, NULL, 8);
      
      // 10 进制
      return strtol(tokens[p].str, NULL, 10);
    }

    return strtol(tokens[p].str, NULL, 10);
  }
  else if (check_parentheses(p, q) == true) {
    /* The expression is surrounded by a matched pair of parentheses.
     * If that is the case, just throw away the parentheses.
     */
    return eval(p + 1, q - 1, success);
  }
  else {
    int opt_pos = find_opt(p, q);
    if(opt_pos < 0){
      *success = false;
      return 0;
    }

    if(tokens[opt_pos].type == TK_NEG || tokens[opt_pos].type == TK_POS \
       || tokens[opt_pos].type == TK_DEREF  || tokens[opt_pos].type == TK_REG){ // 单目运算
      sword_t expr = eval(opt_pos+1, q, success);
      if (!*success) return 0;
       switch(tokens[opt_pos].type) {
        case TK_NEG: return -expr;
        case TK_POS: return expr;
        case TK_DEREF: return vaddr_read(expr, 4);
        default:
          Log("undefined operation");
          *success = false;
          return 0;
       }
    }else{  // 双目运算
      /* We should do more things here. */
      sword_t lexpr = eval(p, opt_pos-1, success);
      if (!*success) return 0;
      sword_t rexpr = eval(opt_pos+1, q, success);
      if (!*success) return 0;

      switch(tokens[opt_pos].type) {
        case TK_AND: return lexpr && rexpr;
        case TK_OR: return lexpr || rexpr;
        case TK_GT: return lexpr > rexpr;
        case TK_LT: return lexpr < rexpr;
        case TK_LE: return lexpr <= rexpr;
        case TK_GE: return lexpr >= rexpr;
        case TK_EQ: return lexpr == rexpr;
        case TK_NEQ: return lexpr != rexpr;
        case '+': return lexpr + rexpr;
        case '-': return lexpr - rexpr;
        case '*': return lexpr * rexpr;
        case '/': if (rexpr == 0) {
          *success = false;
          Log("divide zero!");
          return 0;
        } 
        return lexpr / rexpr; // e.g. -1/2, may not pass the expr test
        
        default: 
          *success = false;
          Log("undefined operation!");
          return 0;
      }
    }
   

  }
}

sword_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }
  /* TODO: Insert codes to evaluate the expression. */
  sword_t res = eval(0, nr_token - 1, success);
  if(!*success) return 0;

  return res;
}
