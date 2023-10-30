// iu.cpp
//   by Derek Chiou
//      March 4, 2007
//      modified Oct. 8, 2023
// 

// STUDENTS: YOU ARE EXPECTED TO MAKE MOST OF YOUR MODIFICATIONS IN THIS FILE.
// for 382N-10

#include "types.h"
#include "helpers.h"
#include "my_fifo.h"
#include "cache.h"
#include "iu.h"


iu_t::iu_t(int __node) {
  node = __node;
  for (int i = 0; i < MEM_SIZE; ++i) 
    for (int j = 0; j < CACHE_LINE_SIZE; ++j)
      mem[i][j] = 0;
  for (int i = 0; i < MEM_SIZE; ++i) {
    set_directory_entry_modified(i, false);
    for (int j = 0; j < MAX_NUM_PROCS; j++)
      set_directory_entry_node_mask(i, j, false);
  }

  cmd_id = 0;
  dir_reply_cmd.valid_p = false;
  cache_request_cmd.valid_p = false;
  cache_reply_cmd.valid_p = false;
  dir_request_cmd.valid_p = false;
  for (int i = 0; i < MAX_NUM_PROCS; i++) {
    on_net_p[i] = false;
  }

  proc_cmd_p = false;
  proc_cmd_writeback_p = false;
}

void iu_t::bind(cache_t *c, network_t *n) {
  cache = c;
  net = n;
}

// if there is a proc_cmd, iu sends a dir_request to home node and receives a dir_reply in response
// if there is a proc_cmd_writeback, iu sends sends the data in the form of a cache_reply
// home node sends coherence information via a cache_request and iu replies with either data or ack via a cache_reply
void iu_t::advance_one_cycle() {
  NOTE_ARGS(("node = %d: bits = %d %d %d %d %d %d", node, proc_cmd_writeback_p, proc_cmd_p, dir_reply_cmd.valid_p, cache_request_cmd.valid_p, cache_reply_cmd.valid_p, dir_request_cmd.valid_p));
  std::string s = "";
  for (int i = 0; i < MAX_NUM_PROCS; i++) {
    s += std::to_string(on_net_p[i]);
  }
  NOTE_ARGS(("node = %d: on_net_p: %s", node, s.c_str()));
  if (proc_cmd_writeback_p) {
    if (!process_proc_reply(proc_cmd_writeback)) {
      proc_cmd_writeback_p = false;
    }
  } else if (proc_cmd_p) {
    if (!process_proc_request(proc_cmd)) {
      proc_cmd_p = false;
    }
  }
  if (dir_reply_cmd.valid_p || net->from_net_p(node, PRI0)) {
    NOTE_ARGS(("PRI0: %d, %d", node, !dir_reply_cmd.valid_p));
    if (!dir_reply_cmd.valid_p) {
      dir_reply_cmd = net->from_net(node, PRI0);
    }
    if (!process_dir_reply(dir_reply_cmd)) {
      dir_reply_cmd.valid_p = false;
    }
  } else if (cache_request_cmd.valid_p || net->from_net_p(node, PRI1)) {
    NOTE_ARGS(("PRI1: %d, %d", node, !cache_request_cmd.valid_p));
    if (!cache_request_cmd.valid_p) {
      cache_request_cmd = net->from_net(node, PRI1);
    }
    if (!process_cache_request(cache_request_cmd)) {
      cache_request_cmd.valid_p = false;
    }
  } else if (cache_reply_cmd.valid_p || net->from_net_p(node, PRI2)) {
    NOTE_ARGS(("PRI2: %d, %d", node, !cache_reply_cmd.valid_p));
    if (!cache_reply_cmd.valid_p) {
      cache_reply_cmd = net->from_net(node, PRI2);
    }
    if (!process_cache_reply(cache_reply_cmd)) {
      cache_reply_cmd.valid_p = false;
    }
  } else if (dir_request_cmd.valid_p || net->from_net_p(node, PRI3)) {
    NOTE_ARGS(("PRI3: %d, %d", node, !dir_request_cmd.valid_p));
    if (!dir_request_cmd.valid_p) {
      dir_request_cmd = net->from_net(node, PRI3);
    }
    if (!process_dir_request(dir_request_cmd)) {
      dir_request_cmd.valid_p = false;
    }
  }
  
}

// processor side

// this interface method buffers a non-writeback request from the processor, returns true if cannot complete
bool iu_t::from_proc(proc_cmd_t pc) {
  if (!proc_cmd_p) {
    NOTE_ARGS(("node = %d: permit_tag %d and busop %d for address %d", node, pc.permit_tag, pc.busop, pc.addr));
    proc_cmd_p = true;
    proc_cmd = pc;
    bus_tag_data_t *tag = (bus_tag_data_t *)&proc_cmd.tag;
    tag->valid_p = true;
    tag->reply_p = false;
    tag->id = cmd_id++;

    return(false);
  } else {
    return(true);
  }
}

// this interface method buffers a writeback request from the processor, returns true if cannot complete
bool iu_t::from_proc_writeback(proc_cmd_t pc) {
  if (!proc_cmd_writeback_p) {
    NOTE_ARGS(("node = %d: permit_tag %d and busop %d for address %d", node, pc.permit_tag, pc.busop, pc.addr));
    proc_cmd_writeback_p = true;
    proc_cmd_writeback = pc;
    bus_tag_data_t *tag = (bus_tag_data_t *)&proc_cmd_writeback.tag;
    tag->valid_p = true;
    tag->reply_p = true;
    tag->id = cmd_id++;

    return(false);
  } else {
    return(true);
  }
}

bool iu_t::process_proc_reply(proc_cmd_t &pcw) { 
  NOTE_ARGS(("node = %d", node));
  int dest = gen_node(pcw.addr);
  net_cmd_t reply_cmd = {true, dest, node, pcw};
  if (dest == node) {
    return(process_cache_reply(reply_cmd)); 
  }
  if (net->to_net(node, PRI2, reply_cmd)) {
    return(false);
  }
  return(true);
}

bool iu_t::process_proc_request(proc_cmd_t &pc) {
  NOTE_ARGS(("node = %d, valid = %d", node, ((bus_tag_data_t *)&pc.tag)->valid_p));
  if (!((bus_tag_data_t *)&pc.tag)->valid_p) {
    return true;
  }
  int dest = gen_node(pc.addr);
  int lcl = gen_local_cache_line(pc.addr);

  NOTE_ARGS(("node = %d: addr = %d, dest = %d", node, pc.addr, dest));

  net_cmd_t net_cmd = {true, dest, node, pc};
  if (dest == node) { // local
    if (!dir_request_cmd.valid_p) {
      ++local_accesses;
      dir_request_cmd = net_cmd;
      ((bus_tag_data_t *)&pc.tag)->valid_p = false;
    }
  } else { // global
    if (net->to_net(node, PRI3, net_cmd)) {
      ++global_accesses;
      ((bus_tag_data_t *)&proc_cmd.tag)->valid_p = false;
    }
  }
  return(true);
}

// receive a directory request
bool iu_t::process_dir_request(net_cmd_t net_cmd) {
  proc_cmd_t pc = net_cmd.proc_cmd;

  int lcl = gen_local_cache_line(pc.addr);
  int src = net_cmd.src;

  // sanity check
  if (gen_node(pc.addr) != node) 
    ERROR("sent to wrong home site!");
  if (pc.busop != READ)
    ERROR("only READ requests are allowed to the directory");

  net_cmd_t net_cmd_request = {true, -1, node, {INVALIDATE, pc.addr, 0, INVALID, {0}}};
  bus_tag_data_t *tag = ((bus_tag_data_t *)&net_cmd_request.proc_cmd.tag);
  tag->valid_p = true;
  tag->reply_p = false;
  tag->id = ((bus_tag_data_t *)&pc.tag)->id;
  net_cmd_t net_cmd_reply = {false, src, node, pc};
  NOTE_ARGS(("node = %d, addr = %d, lcl = %d, src = %d", node, pc.addr, lcl, src));
  permit_tag_t permit_tag = get_directory_entry_state(lcl);
  switch(permit_tag) {
    case INVALID:
      net_cmd_reply.valid_p = true;
      break;
    
    case SHARED: {
      if (pc.permit_tag == SHARED || (pc.permit_tag == MODIFIED && get_directory_entry_owner(lcl) == src)) { // proc only requests in SHARED or MODIFIED
        net_cmd_reply.valid_p = true;
        break;
      }
      NOTE_ARGS(("(SHARED) sending invalidate for line %d", lcl));
      for (int i = 0; i < MAX_NUM_PROCS; i++) {
        if (on_net_p[i] || i == src || get_directory_entry_node_mask(lcl, i) == 0)
          continue;
        if (i == node) {
          response_t response = cache->snoop(net_cmd_request.proc_cmd);
          if (!response.retry_p) {
            on_net_p[i] = true;
          }
        } else {
          net_cmd_request.dest = i;
          if (net->to_net(node, PRI1, net_cmd_request)) {
            NOTE_ARGS(("sending invalidate to %d", i));
            on_net_p[i] = true;
          }
        }
      }
      break;
    }
    case MODIFIED: {
      int owner = get_directory_entry_owner(lcl);
      if (owner == src) {
        ERROR("requestor already has the cache in MODIFIED state");
      }
      NOTE_ARGS(("(MODIFIED) sending invalidate for line %d", lcl));
      net_cmd_reply.valid_p = false;
      net_cmd_request.proc_cmd.busop = WRITEBACK;
      if (on_net_p[owner])
        break;
      if (owner == node) {
        response_t response = cache->snoop(net_cmd_request.proc_cmd);
        if (!response.retry_p) {
          on_net_p[owner] = true;
        }
      } else {
        net_cmd_request.dest = owner;
        if (net->to_net(node, PRI1, net_cmd_request)) {
          NOTE_ARGS(("sending invalidate to %d", owner));
          on_net_p[owner] = true;
        }
      }
      break;
    }
  }

  if (net_cmd_reply.valid_p) {
    for (int i = 0; i < MAX_NUM_PROCS; i++) {
      on_net_p[i] = false;
    }
    copy_cache_line(net_cmd_reply.proc_cmd.data, mem[lcl]);
    if (src == node) {
      update_directory_entry(lcl, pc.busop, src, pc.data, pc.permit_tag);
      ((bus_tag_data_t *)&proc_cmd.tag)->valid_p = false;
      return(process_dir_reply(net_cmd_reply));
    } else if (net->to_net(node, PRI0, net_cmd_reply)) {
      update_directory_entry(lcl, pc.busop, src, pc.data, pc.permit_tag);
      return(false);
    }
  } 
  return(true);
}

bool iu_t::get_directory_entry_modified(int lcl) {
  uint8_t *entry = &((uint8_t *)dir_mem)[lcl * DIRECTORY_SIZE];
  return(entry[0] & 0x1);
}

void iu_t::set_directory_entry_modified(int lcl, bool modified) {
  uint8_t *entry = &((uint8_t *)dir_mem)[lcl * DIRECTORY_SIZE];
  entry[0] = modified ? 0x1 : 0x0;
}

bool iu_t::get_directory_entry_node_mask(int lcl, int node) {
  uint32_t *node_mask = (uint32_t *)&((uint8_t *)dir_mem)[lcl * DIRECTORY_SIZE + 1];
  return((*node_mask >> node) & 0x1);
}

void iu_t::set_directory_entry_node_mask(int lcl, int node, bool present) {
  uint32_t *node_mask = (uint32_t *)&((uint8_t *)dir_mem)[lcl * DIRECTORY_SIZE + 1];
  if (present) {
    *node_mask |= (1 << node);
  } else {
    *node_mask &= ~(1 << node);
  }
}

bool iu_t::process_dir_reply(net_cmd_t net_cmd) {
  NOTE_ARGS(("node = %d", node));
  proc_cmd_t pc = net_cmd.proc_cmd;
  proc_cmd_p = false; // clear out request that this reply is a reply to

  switch(pc.busop) {
  case READ:
    if (proc_cmd_writeback_p) {
      return(true);
    }
    cache->reply(pc);
    return(false);
      
  case WRITEBACK:
  case INVALIDATE:
    ERROR("should not have gotten a reply back from a write or an invalidate, since we are incoherent");
  default:
    ERROR("should not reach default");
  }
}

bool iu_t::process_cache_request(net_cmd_t net_cmd) {
  NOTE_ARGS(("node = %d", node));
  proc_cmd_t pc = net_cmd.proc_cmd;
  response_t response = cache->snoop(pc);
  if (response.retry_p) {
    return(true);
  }
  return(false);
}

bool iu_t::process_cache_reply(net_cmd_t net_cmd) {
  proc_cmd_t pc = net_cmd.proc_cmd;
  bus_tag_data_t *tag = (bus_tag_data_t *)&pc.tag;

  int lcl = gen_local_cache_line(pc.addr);
  int src = net_cmd.src;

  NOTE_ARGS(("node = %d: src = %d, lcl = %d, busop = %d, permit_tag = %d", node, src, lcl, pc.busop, pc.permit_tag));

  // sanity check
  if (gen_node(pc.addr) != node) 
    ERROR("sent to wrong home site!");
  if (!tag->reply_p)
    ERROR("should be a reply message");
  
  update_directory_entry(lcl, pc.busop, net_cmd.src, pc.data, pc.permit_tag);
  return(false);
}

permit_tag_t iu_t::get_directory_entry_state(int lcl)
{
  if (get_directory_entry_modified(lcl)) {
    int count = 0;
    for (int i = 0; i < MAX_NUM_PROCS; i++)
      if (get_directory_entry_node_mask(lcl, i))
        count++;
    if (count > 1)
      ERROR("more than one nodes have data even though it is in MODIFIED state");
    return(MODIFIED);
  }
  for (int i = 0; i < MAX_NUM_PROCS; i++) {
    if (get_directory_entry_node_mask(lcl, i))
      return(SHARED);
  }
  return(INVALID);
}

void iu_t::update_directory_entry(int lcl, busop_t busop, int node, data_t data, permit_tag_t permit_tag) {
  switch(busop) {
    case INVALIDATE:
      set_directory_entry_node_mask(lcl, node, false);
      break;

    case WRITEBACK: {
      int count = 0;
      for (int i = 0; i < MAX_NUM_PROCS; i++)
        if (get_directory_entry_node_mask(lcl, i))
          count++;
      if (count > 1 || count == 0)
        ERROR("should not have gotten a writeback request for a cache line that is not in MODIFIED state");
      set_directory_entry_node_mask(lcl, node, false);
      set_directory_entry_modified(lcl, false);
      copy_cache_line(mem[lcl], data);
      break;
    }
    
    case READ: {
      int count = 0;
      for (int i = 0; i < MAX_NUM_PROCS; i++)
        if (get_directory_entry_node_mask(lcl, i))
          count++;
      if (count > 1 && permit_tag == MODIFIED)
        ERROR("should not MODIFY for a SHARED cache line");
      set_directory_entry_node_mask(lcl, node, true);
      set_directory_entry_modified(lcl, (permit_tag == MODIFIED));
      break;
    }
  }
  NOTE_ARGS(("updated directory entry at cache line %d, busop %d, permit_tag %d, node %d", lcl, busop, permit_tag, node));
  std::string s = "";
  for (int i = 0; i < MAX_NUM_PROCS; i++) {
    s += std::to_string(get_directory_entry_node_mask(lcl, i));
  }
  NOTE_ARGS(("node_mask: %s", s.c_str()));
}

int iu_t::get_directory_entry_owner(int lcl)
{
  int count = 0, node = -1;
  for (int i = 0; i < MAX_NUM_PROCS; i++)
    if (get_directory_entry_node_mask(lcl, i)) {
      count++;
      node = i;
    }
  if (count > 1 || count == 0)
    return -1;
  return node;  
}

void iu_t::print_stats() {
  printf("------------------------------\n");
  printf("%d: iu\n", node);
  
  printf("num local  accesses = %d\n", local_accesses);
  printf("num global accesses = %d\n", global_accesses);
}
