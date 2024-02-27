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

#include "sdb.h"

#define NR_WP 32

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */
  char expr[32];
  sword_t val;
} WP;

static WP wp_pool[NR_WP] = {};

// head: The watchpoint being used
// free_: Idle watchpoint
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */
WP* new_wp(){
  if(free_ == NULL){
    Log("No free watchpoint alloction!");
    assert(0);
    return NULL;
  }
  WP* wp_alloc = free_;
  free_ = free_ -> next;

  wp_alloc -> next = head;
  head = wp_alloc;

  return wp_alloc;
}


// 回收空闲WP
void free_wp(WP *wp){
  if(wp == head){
    head = head->next;
  }else{
    WP* h = head;
    while(h && h->next != wp) h = h->next;
    assert(h);
    h->next = wp->next;
  }
  wp->next = free_;
  free_ = wp;

  return;
}


void wp_watch(char *expr, sword_t val) {
  WP* wp = new_wp();
  strcpy(wp->expr, expr);
  wp->val = val;
  printf("Watchpoint %d: %s\n", wp->NO, expr);
}

void wp_remove(int no) {
  assert(no < NR_WP && no >= 0);
  WP* wp = &wp_pool[no];
  free_wp(wp);
  printf("Delete watchpoint %d: %s\n", wp->NO, wp->expr);
}


void wp_iterate(){
  WP* h = head;
  if(h == NULL){
    Log("No watchpoint been set!");
    return;
  }
  printf("%-8s%-8s%-8s\n", "Num", "Expr", "Val");
  while (h) {
    printf("%-8d%-8s%-8lx\n", h->NO, h->expr, h->val);
    h = h->next;
  }
  return;
}


// 观测点检查
void wp_difftest() {
  WP* h = head;
  while (h) {
    bool _;
    Log("test");
    sword_t new_val = expr(h->expr, &_);
    if (h->val != new_val) {
      printf("Watchpoint %d: %s\n" "Old value = %ld\n" "New value = %ld\n"
        , h->NO, h->expr, h->val, new_val);
      h->val = new_val;
    }
    h = h->next;
  }
}



