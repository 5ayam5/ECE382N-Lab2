// iu.h
//   by Derek Chiou
//      March 4, 2007
// 

// STUDENTS: YOU ARE ALLOWED TO MODIFY THIS FILE, BUT YOU SHOULDN'T NEED TO MODIFY MUCH, IF ANYTHING.
// for 382N-10

#ifndef IU_H
#define IU_H
#include "types.h"
#include "my_fifo.h"
#include "cache.h"
#include "network.h"

const int MAX_NUM_PROCS = 32;

class iu_t {
  typedef struct {
    bool modified_p;
    bool node_mask[MAX_NUM_PROCS];
  } directory_entry_t;

  typedef struct {
    bool valid_p;
    bool reply_p;
    uint8_t id;
  } bus_tag_data_t;

  uint8_t cmd_id;

  int node;

  int local_accesses;
  int global_accesses;

  data_t mem[MEM_SIZE];
  data_t dir_mem[DIR_MEM_SIZE];

  cache_t *cache;
  network_t *net;

  net_cmd_t dir_reply_cmd;
  net_cmd_t cache_request_cmd;
  net_cmd_t cache_reply_cmd;
  net_cmd_t dir_request_cmd;
  bool on_net_p[MAX_NUM_PROCS];

  bool proc_cmd_p;
  proc_cmd_t proc_cmd;

  bool proc_cmd_writeback_p;
  proc_cmd_t proc_cmd_writeback;

  // processor side
  bool process_proc_reply(proc_cmd_t &proc_cmd_writeback);
  bool process_proc_request(proc_cmd_t &proc_cmd);

  // network side
  bool process_dir_reply(net_cmd_t net_cmd);
  bool process_cache_reply(net_cmd_t net_cmd);
  bool process_cache_request(net_cmd_t net_cmd);
  bool process_dir_request(net_cmd_t net_cmd);

  // Directory side
  permit_tag_t get_directory_entry_state(const directory_entry_t &dir_entry);
  void update_directory_entry(int lcl, directory_entry_t &dir_entry, busop_t busop, int node, data_t data, permit_tag_t permit_tag);
  int get_directory_entry_owner(const directory_entry_t &dir_entry); // return the node having the MODIFIED data or the sole node having the SHARED or EXCLUSIVE data

 public:
  iu_t(int __node);

  void bind(cache_t *c, network_t *n);

  void advance_one_cycle();
  void print_stats();

  // processor side
  bool from_proc(proc_cmd_t pc);
  bool from_proc_writeback(proc_cmd_t pc);
};
#endif
