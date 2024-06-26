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

int addr_store;
int count_0=0;
int count_1 = 0;
int finish_7 = 0;
int finish_0 = 0;
int finish_3 = 0;
int ld_value[32];


proc_t::proc_t(int __p) {
  proc = __p;
  init();
}

int proc_t::registers[MAX_NUM_PROCS_TEST][NUM_REGISTERS] = {0};
std::vector<event_t> proc_t::events;

void proc_t::init() {
  response.retry_p = false;
  ld_p = false;
  tests_empty = false;
  pc = 0;
  wait_cycles = 0;

  switch(args.test) {
    case 1: //all loads -> all stores -> all loads
    break;

    case 2: { //Dekker's algorithm
      switch(proc) {
        case 0:
        case 1: {
          int critical = 14;
          int stride = 500 * args.num_procs;

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

    case 3: { //loads and stores simultaneously
      int num_addrs = 1000;
      int addr_range = 1024 * 8 * args.num_procs;
      std::vector<int> addrs;
      for (int i = 0; i < num_addrs; i++) {
        addrs.push_back(random() % addr_range);
      }
      if (proc == 0) {
        for (int i = 0; i < num_addrs; i++) {
          instructions.emplace_back(INIT, 0, i);                 // 0: R0 = i
          instructions.emplace_back(ST, 0, addrs[i]);            // 1: mem[addrs[i]] = i
        }
      } else if (proc == args.num_procs - 1) {
        std::reverse(addrs.begin(), addrs.end());
        for (int i = 0; i < LIVELOCK_LIMIT; i++) {
          instructions.emplace_back(INIT, 0, 0);
        }
        for (int i = 0; i < num_addrs; i++) {
          instructions.emplace_back(LD, 0, addrs[i]);            // 0: R0 = mem[addrs[i]]
        }
      }
    }
    break;

    case 4: { //random test
      int addr_range = 1024 * 8 * args.num_procs;
      for (int i = 0; i < 10000; ++i) {
        instruction_type_t type = (random() % 2 == 0) ? LD : ST;
        int op1 = random() % NUM_REGISTERS;
        int op2 = random() % addr_range;
        instructions.emplace_back(type, op1, op2);
      }
    }
    break;

    case 5: { //LRU eviction
      switch(proc) {
        case 7: {
          testQueue.push(test_case_t(0, 5, 0, 2));
          testQueue.push(test_case_t(1, 65, 1));
          testQueue.push(test_case_t(1, 130, 2));
          testQueue.push(test_case_t(1, 195, 3));
          break;
        }

        case 5: {
          testQueue.push(test_case_t(1, 5, 0));
          testQueue.push(test_case_t(1, 65, 1));
          testQueue.push(test_case_t(1, 130, 2));
          testQueue.push(test_case_t(1, 195, 3));
          break;
        }
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
    addr_store = (random() % args.num_procs) * 256 + random() % 256;
    break;
  case 2:
    break;
  case 3:
    break;
  case 4:
    break;
  case 5:
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

  case 1: {
    int counts[args.num_procs + 1];
    for (int i = 0; i <= args.num_procs; ++i) {
      counts[i] = 0;
    }
    for(int i = 0; i <= args.num_procs; ++i) {
      counts[ld_value[i]]++;
    }
    bool passed = false;
    for (int i = 0; i <= args.num_procs; ++i) {
      NOTE_ARGS(("count for %d is %d", i, counts[i]));
      if (counts[i] == args.num_procs) {
        passed = true;
      }
    }
    if (!passed) {
      ERROR("test failed");
    }
  }
  break;
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

  case 5: {
    if(ld_value[3] == 10)
        printf("test passed\n");
      else
        ERROR("test failed");
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

  case 1: { //all loads -> all stores -> all loads
    test_case_t curr_test;
    if (!response.retry_p) {
      wait_cycles=0;
      if(test_idx == 0) {
        curr_test = test_case_t(1, addr_store, 0);
        test_idx = test_idx + 1;
        tests_empty = false;
      }
      else if (test_idx == 1 && count_0 == args.num_procs) {
        curr_test = test_case_t(0, addr_store, 0, proc+1);
        test_idx = test_idx + 1;

        tests_empty = false;
      }
      else if (test_idx == 2 && count_1 == args.num_procs) {
        curr_test = test_case_t(1, addr_store, 0);
        test_idx = test_idx + 1;
        tests_empty = false;
      }
      else {
        tests_empty = true;
      }
      addr = curr_test.address;
      ld_p = curr_test.type;
      tag = curr_test.bus_tag;
    }
    // Tests are not empty the we run, if they are empty we will not schedule a request.
    if ((!tests_empty) | (response.retry_p)) {
      if (ld_p) {
        response = cache->load(addr, tag, &data, response.retry_p);
      }
      else {
        response = cache->store(addr, tag, proc+1, response.retry_p);
      }
      wait_cycles++;
      if(wait_cycles > LIVELOCK_LIMIT)
              ERROR("Livelock error\n");
      if(!response.retry_p) {
        if(test_idx == 1)
          count_0++;
        if(test_idx == 2)
          count_1++;
        if(test_idx == 3)
          ld_value[proc] = data;

      }
    }

  }
  break;
  case 2: //Dekkers
  case 3: //simultaneous loads and stores
  case 4: { //random SC test
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

  case 5: { //LRU eviction
    if(count_0 == 8 && !finish_7) {
      if(proc == 7) {
        response = cache->load(260, 0, &data, response.retry_p);
        wait_cycles++;
        if(wait_cycles > LIVELOCK_LIMIT)
                ERROR("livelock error\n");
        if(!response.retry_p) {
          finish_7 = 1;
          wait_cycles = 0;
          return;
        }
      }
    }

    if(finish_7 && !finish_0) {
      if(proc == 0) {
        response = cache->store(5, 0, 10, response.retry_p);
        wait_cycles++;
        if(wait_cycles > LIVELOCK_LIMIT)
                ERROR("livelock error\n");
        if(!response.retry_p) {
          finish_0 = 1;
          wait_cycles = 0;
         }
           return;
      }

    }

    if(finish_0 && !finish_3) {
      if(proc == 3) {
        response = cache->load(5,0, &data, response.retry_p);
        wait_cycles++;
        if(wait_cycles > LIVELOCK_LIMIT)
                ERROR("livelock error\n");
        if(!response.retry_p) {
          finish_3 = 1;
          ld_value[proc] = data;
          wait_cycles = 0;
        }
        return;
      }
    }

    if (!response.retry_p) {
        // No current retrying request. Get new request from test queue
        if (!testQueue.empty()) {
          // Test queue is not empty
          test_case_t curr_test = testQueue.front();
          testQueue.pop();
          addr = curr_test.address;
          ld_p = curr_test.type;
          tag = curr_test.bus_tag;
          tests_empty = false;
          wait_cycles = 0;
        }
        else {
          // test queue is empty
          tests_empty = true;
        }
    }
      // Tests are not empty the we run, if they are empty we will not schedule a request.
    if (((!tests_empty) | (response.retry_p)) & (count_0 != 8)) {
        if (ld_p) {
          response = cache->load(addr, tag, &data, response.retry_p);
        }
        else {
          response = cache->store(addr, tag, cur_cycle, response.retry_p);
        }
        wait_cycles++;
        if(wait_cycles > LIVELOCK_LIMIT)
                ERROR("livelock error\n");

        if(!response.retry_p) {
          count_0++;
          wait_cycles = 0;
        }

    }
  }
  break;

  default:
    ERROR("don't know this test case");
  }
}
