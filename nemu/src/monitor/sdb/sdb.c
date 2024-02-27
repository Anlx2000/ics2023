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
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

word_t vaddr_read(vaddr_t addr, int len);

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

// cmd -c
static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

// cmd -q   --> quit
static int cmd_q(char *args) {
  return -1;
}

static int cmd_help(char *args);
static int cmd_si(char *args);
static int cmd_info(char *args);
static int cmd_x(char *args);
static int cmd_p(char *args);
static int cmd_w(char *args);
static int cmd_d(char *args);
static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },

  /* TODO: Add more commands */
  { "si", "Usage: si 2. Single/Multi step execution", cmd_si},
  { "info", "Usage: info r/w. Print register/watch_point information", cmd_info},
  { "x", "Usage: x N EXPR. Scan the memory from EXPR by N bytes", cmd_x},
  { "p", "Usage: p EXPR. Calculate the expression, e.g. p $eax + 1", cmd_p },
  { "w", "Usage: w EXPR. Watch for the variation of the result of EXPR, pause at variation point", cmd_w },
  { "d", "Usage: d N. Delete watchpoint of wp.NO=N", cmd_d },
};

#define NR_CMD ARRLEN(cmd_table)



static int cmd_si(char *args){
  char *arg = strtok(NULL, " ");
  int step = 1;
  if (arg != NULL) step = atoi(arg);
  cpu_exec(step);
  return 0;
}

static int cmd_info(char *args){
  // char *arg = strtok(NULL, " ");
  switch (*args)
  {
    case 'w':
      wp_iterate();
      break;

    case 'r':
      isa_reg_display();
      break;

    default:
      printf("illegal option! \n\tusage: info [-w | -r]\n");
      break;
  }

  return 0; 
}

static int cmd_x(char *args){
  char *arg = strtok(NULL, " ");
  if(arg == NULL){
    Log("Please input length of address!");
    return 0;
  }
  int N = strtol(arg, NULL, 10);
  arg = strtok(NULL, " ");
  vaddr_t addr = strtol(arg, NULL, 16);
  
  for(int i = 0, step = 4; i < N; i++){
    Log(" "FMT_WORD" = 0x%08"PRIX64" ", addr + i * step, vaddr_read(i * step + addr, step));
  }
  return 0;

}
static int cmd_p(char *args){
  bool success = true;
  sword_t res = expr(args, &success);
  if(!success){
    Log("Invalid expr!");
    return 0;
  }
  Log(" expr = %ld\n", res);
  return 0;
}

static int cmd_w(char *args){
  // char *arg = strtok(NULL, " ");

  bool success = true;
  sword_t val = expr(args, &success);

 
  if(!success){
    Log("Invalid expression!");
    return 0;
  }
  wp_watch(args, val);
  return 0;
}

static int cmd_d(char* args) {
  char *arg = strtok(NULL, "");
  if (!arg) {
    printf("Usage: d N\n");
    return 0;
  }
  int no = strtol(arg, NULL, 10);
  wp_remove(no);
  return 0;
}


static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
