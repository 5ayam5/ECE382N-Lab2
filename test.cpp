// test.cpp
//   Derek Chiou
//     Oct. 8, 2023


#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "generic_error.h"
#include "helpers.h"
#include "cache.h"
#include "test.h"

proc_t::proc_t(int __p) {
  proc = __p;
  init();
}

int proc_t::registers[MAX_NUM_PROCS_TEST][NUM_REGISTERS] = {0};
std::vector<event_t> proc_t::events;

void proc_t::init() {
  response.retry_p = false;
  ld_p = false;
  pc = 0;
  wait_cycles = 0;

  switch(args.test) {
    case 1: 
    case 2: {
      switch(proc) {
        case 0:
        case 1: { // Dekker's algorithm
          int critical = 14;
          int stride = (args.test == 1) ? 1 : (500 * args.num_procs);

          // pre-critical section
          instructions.emplace_back(INIT, 2, 1);                 //  0: R2 = 1
          instructions.emplace_back(ST, 2, proc * stride);       //  1: mem[proc] = 1
          instructions.emplace_back(INIT, 2, 0);                 //  2: R2 = 0
          instructions.emplace_back(LD, 1, (1 - proc) * stride); //  3: R1 = mem[1 - proc]
          instructions.emplace_back(BR, 1, 2, critical);         //  4: if (mem[1 - proc] == 0) goto critical section
          instructions.emplace_back(INIT, 2, proc);              //  5: R2 = proc
          instructions.emplace_back(LD, 1, 2 * stride);          //  6: R1 = mem[2] (turn)
          instructions.emplace_back(BR, 1, 2, 2);                //  7: if (turn == proc) goto instruction 2
          instructions.emplace_back(INIT, 2, 0);                 //  8: R2 = 0
          instructions.emplace_back(ST, 2, proc * stride);       //  9: mem[proc] = 0
          instructions.emplace_back(INIT, 2, 1 - proc);          // 10: R2 = 1 - proc
          instructions.emplace_back(LD, 1, 2 * stride);          // 11: R1 = mem[2] (turn)
          instructions.emplace_back(BR, 1, 2, 11);               // 12: if (turn == 1 - proc) goto instruction 11
          instructions.emplace_back(BR, 2, 2, 0);                // 13: goto instruction 0
          
          // critical section
          instructions.emplace_back(INIT, 2, 1);                 // 14: R2 = 1
          instructions.emplace_back(LD, 0, 3 * stride);          // 15: R0 = mem[3] (count)
          instructions.emplace_back(ADD, 0, 0, 2);               // 16: R0 = R0 + 1
          instructions.emplace_back(ST, 0, 3 * stride);          // 17: mem[3] = R0

          // post-critical section
          instructions.emplace_back(INIT, 2, 1 - proc);          // 18: R2 = 1 - proc
          instructions.emplace_back(ST, 2, 2 * stride);          // 19: mem[2] (turn) = 1 - proc
          instructions.emplace_back(INIT, 2, 0);                 // 20: R2 = 0
          instructions.emplace_back(ST, 2, proc * stride);       // 21: mem[proc] = 0
        }
        break;

        default:
        break;
      }

    }
    break;

    case 3: {
      if (proc == 0) {
        instructions.emplace_back(INIT, 0, 10);                 // 0: R0 = 10
        instructions.emplace_back(ST, 0, 100 * args.num_procs); // 1: mem[100] = 10
        instructions.emplace_back(INIT, 0, 20);                 // 2: R0 = 20
        instructions.emplace_back(ST, 0, 500 * args.num_procs); // 3: mem[200] = 20
      } else if (proc == args.num_procs - 1) {
        for (int i = 0; i < 25; ++i) {
          instructions.emplace_back(INIT, 0, 0);                // i: R0 = 0
        }
        instructions.emplace_back(LD, 0, 500 * args.num_procs); // 25: R0 = mem[500]
        instructions.emplace_back(LD, 1, 100 * args.num_procs); // 26: R1 = mem[100]
      }
    }
    break;

    case 4: {
      int addr_range = 1024 * 8 * args.num_procs;
      for (int i = 0; i < 10000; ++i) {
        instruction_type_t type = (random() % 2 == 0) ? LD : ST;
        int op1 = random() % NUM_REGISTERS;
        int op2 = random() % addr_range;
        instructions.emplace_back(type, op1, op2);
      }
    }
    break;
  }
}

void proc_t::bind(cache_t *c) {
  cache = c;
}


extern args_t args;
extern int addr_range;
extern cache_t **caches;

test_args_t test_args;

void init_test() {
  proc_t::events.clear();
  switch(args.test) {
  case 0:
    test_args.addr_range = 512;
    break;

  case 1:
  case 2:
  case 3:
  case 4:
    break;

  default:
    ERROR("don't recognize this test");
  }
}

void finish_test() {
  double hr;

  switch(args.test) {
  case 0:
    for (int i = 0; i < args.num_procs; ++i) {
      hr = caches[i]->hit_rate();
      if (!within_tolerance(hr, 0.5, 0.01)) {
        ERROR("out of tolerance");
      }
    }
    break;

  case 1:
  case 2: {
    int num_procs = std::min(args.num_procs, 2);
    bool counts[num_procs];
    for (int i = 0; i < num_procs; ++i) {
      counts[i] = 0;
    }
    for (int i = 0; i < num_procs; ++i) {
      int count = proc_t::registers[i][0];
      NOTE_ARGS(("count for proc %d is %d", i, count));
      if (counts[count - 1]) {
        ERROR("duplicate count");
      }
      counts[count - 1] = true;
    }
    for (int i = 0; i < num_procs; ++i) {
      if (!counts[i]) {
        ERROR("missing count");
      }
    }
  }

  case 3:
  case 4: {
    std::unordered_map<int, int> mem;
    for (auto event : proc_t::events) {
      switch(event.type) {
      case LD:
        if (event.val != mem[event.addr]) {
          ERROR("violated SC");
        }
        break;
      case ST:
        mem[event.addr] = event.val;
        break;
      }
    }
  }
  break;
    
  default: 
    ERROR("don't recognize this test");
  }
  printf("passed\n");
}

void proc_t::advance_one_cycle() {
  int data;

  switch (args.test) {
  case 0:
    if (!response.retry_p) {
      addr = random() % test_args.addr_range;
      ld_p = ((random() % 2) == 0);
    }
    if (ld_p) response = cache->load(addr, 0, &data, response.retry_p);
    else      response = cache->store(addr, 0, cur_cycle, response.retry_p);
    break;

  case 1:
  case 2:
  case 3:
  case 4: {
    if (pc == instructions.size()) {
      break;
    }
    instruction_t instruction = instructions[pc];
    int old_pc = pc;
    switch(instruction.type) {
      case LD: {
        response = cache->load(instruction.op2, 0, &data, response.retry_p);
        if (response.hit_p) {
          registers[proc][instruction.op1] = data;
          NOTE_ARGS(("proc %d: R%d = %d (mem[%d])", proc, instruction.op1, data, instruction.op2));
          events.emplace_back(instruction.type, instruction.op2, data, proc);
          pc++;
          wait_cycles = 0;
        } else if (response.retry_p) {
          if (++wait_cycles > LIVELOCK_LIMIT) {
            ERROR("livelock");
          }
        } else {
          ERROR("invalid response");
        }
        break;
      }

      case ST: {
        response = cache->store(instruction.op2, 0, registers[proc][instruction.op1], response.retry_p);
        if (response.hit_p) {
          pc++;
          wait_cycles = 0;
          NOTE_ARGS(("proc %d: mem[%d] = %d (R%d)", proc, instruction.op2, registers[proc][instruction.op1], instruction.op1));
          events.emplace_back(instruction.type, instruction.op2, registers[proc][instruction.op1], proc);
        } else if (response.retry_p) {
          if (++wait_cycles > LIVELOCK_LIMIT) {
            ERROR("livelock");
          }
        } else {
          ERROR("invalid response");
        }
        break;

        case ADD: {
          NOTE_ARGS(("proc %d: R%d = %d + %d (R%d + R%d)", proc, instruction.op1, registers[proc][instruction.op2], registers[proc][instruction.op3], instruction.op2, instruction.op3));
          registers[proc][instruction.op1] = registers[proc][instruction.op2] + registers[proc][instruction.op3];
          pc++;
          break;
        }

        case BR: {
          if (registers[proc][instruction.op1] == registers[proc][instruction.op2]) {
            NOTE_ARGS(("proc %d: branch to %d (R%d = %d, R%d = %d)", proc, instruction.op3, instruction.op1, registers[proc][instruction.op1], instruction.op2, registers[proc][instruction.op2]));
            pc = instruction.op3;
          } else {
            NOTE_ARGS(("proc %d: didn't branch to %d (R%d = %d, R%d = %d)", proc, instruction.op3, instruction.op1, registers[proc][instruction.op1], instruction.op2, registers[proc][instruction.op2]));
            pc++;
          }
          break;
        }

        case INIT: {
          registers[proc][instruction.op1] = instruction.op2;
          NOTE_ARGS(("proc %d: R%d = %d", proc, instruction.op1, instruction.op2));
          pc++;
          break;
        }
      }
    }
  }
  break;

  default:
    ERROR("don't know this test case");
  }
}
