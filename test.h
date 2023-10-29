// test.h
//   Derek Chiou
//     Oct. 8, 2023

// STUDENTS: YOU ARE EXPECTED TO PUT YOUR TESTS IN THIS FILE, ALONG WITH TEST.cpp

#include "types.h"
#include <queue>

const int NUM_REGISTERS = 8;
const int MAX_NUM_PROCS_TEST = 32;
typedef enum {LD, ST, ADD, BR, INIT} instruction_type_t;
const int LIVELOCK_LIMIT = 10000;

class instruction_t {
public:
  instruction_type_t type;
  int op1;
  int op2;
  int op3;

  instruction_t(instruction_type_t t, int o1, int o2, int o3) : type(t), op1(o1), op2(o2), op3(o3) {}
  instruction_t(instruction_type_t t, int o1, int o2) : type(t), op1(o1), op2(o2) {}
};

class event_t {
public:
  instruction_type_t type;
  int addr;
  int val;
  int proc;

  event_t(instruction_type_t t, int a, int v, int p) : type(t), addr(a), val(v), proc(p) {}
};

// models a processor's ld/st stream
class proc_t {
  int proc;
  response_t response;

  address_t addr;
  bus_tag_t tag = 0;

  bool ld_p;

  cache_t *cache;
  
  // For testing
  std::vector<instruction_t> instructions;
  int wait_cycles;
  int pc;

 public:
  proc_t(int p);
  void init();
  void bind(cache_t *c);
  void advance_one_cycle();
  
  static int registers[MAX_NUM_PROCS_TEST][NUM_REGISTERS];
  static std::vector<event_t> events;
};



void init_test();
void finish_test();

// ***** FYTD ***** 

typedef struct {
  int addr_range;
} test_args_t;
extern test_args_t test_args;
