// cache.c
//   by Derek Chiou
//      Oct. 5, 2023
// 

// for 382N-10

// STUDENTS: YOU ARE NOT ALLOWED TO MODIFY THIS FILE.

#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "cache.h"
#include "iu.h"
#include "helpers.h"


cache_t::cache_t(int __node, int __lg_assoc, int __lg_num_sets, int __lg_cache_line_size) {
  init(__node, __lg_assoc, __lg_num_sets, __lg_cache_line_size);
}

void cache_t::init(int __node, int __lg_assoc, int __lg_num_sets, int __lg_cache_line_size) {
  node = __node;
  lg_assoc = __lg_assoc;
  lg_num_sets = __lg_num_sets;
  lg_cache_line_size = __lg_cache_line_size;

  full_hits = partial_hits = misses = 0;

  num_sets = (1 << lg_num_sets);
  assoc = (1 << lg_assoc);

  set_shift = lg_cache_line_size;
  set_mask = (1 << lg_num_sets) - 1;

  address_tag_shift = lg_cache_line_size + lg_num_sets;

  cache_line_mask = (1 << lg_cache_line_size) - 1;
  if ((cache_line_mask + 1) != CACHE_LINE_SIZE) {
    ERROR("inconsistent cache_line_size");
  }
  
  tags = new cache_line_t*[num_sets];

  for (int i = 0; i < num_sets; ++i) {
    tags[i] = new cache_line_t[assoc];

    for (int j = 0; j < assoc; ++j) {
      tags[i][j].permit_tag = INVALID;
      tags[i][j].replacement = j;

      for (int k = 0; k < cache_line_size; ++k) {
	tags[i][j].data[k] = 0;
      }
    }
  }
}

void cache_t::bind(iu_t *i) {
  iu = i;
}

void cache_t::print_stats() {
  printf("------------------------------\n");
  printf("node %d\n", node);
  printf("full_hits      = %d\n", full_hits);
  printf("partial_hits   = %d\n", partial_hits);
  printf("misses         = %d\n", misses);
  printf("total accesses = %d\n", full_hits + partial_hits + misses);
  printf("hit rate = %f\n", hit_rate());
}

double cache_t::hit_rate() {
  return((double)full_hits / (full_hits + partial_hits + misses));
}
  

address_tag_t cache_t::gen_address_tag(address_t addr) {
  return(addr >> address_tag_shift);
}

int cache_t::gen_offset(address_t addr) {
  return(addr & cache_line_mask);
}

int cache_t::gen_set(address_t addr) {
  int set = (addr >> set_shift) & set_mask;
  NOTE_ARGS(("addr = %x, set_shift %d, set_mask %x, set %d", addr, set_shift, set_mask, set));
  return(set);
}


bool cache_t::cache_access(address_t addr, permit_tag_t permit, cache_access_response_t *car) {
  int set = gen_set(addr);
  address_tag_t address_tag = gen_address_tag(addr);

  car->set = set;

  for (int a = 0; a < assoc; ++a) {
    if (tags[set][a].address_tag == address_tag) { // found in cache, check permit
      car->address_tag = address_tag;
      car->permit_tag = tags[set][a].permit_tag;
      car->way = a;

      return (car->permit_tag >= permit);

    }
  }

  car->permit_tag = INVALID;
  return(false);
}

void cache_t::modify_permit_tag(cache_access_response_t car, permit_tag_t permit) {
  tags[car.set][car.way].permit_tag = permit;
}

void cache_t::cache_fill(cache_access_response_t car, data_t data) {
  tags[car.set][car.way].address_tag = car.address_tag;
  tags[car.set][car.way].permit_tag = car.permit_tag;

  NOTE_ARGS(("set tags[%d][%d].address_tag = %d", car.set, car.way, car.address_tag));
  NOTE_ARGS(("set tags[%d][%d].permit_tag  = %d", car.set, car.way, car.permit_tag));

  update_replacement(car);

  for (int i = 0; i < CACHE_LINE_SIZE; ++i) 
    tags[car.set][car.way].data[i] = data[i];
}

int cache_t::read_data(address_t addr, cache_access_response_t car) {
  int offset = gen_offset(addr);

  return(tags[car.set][car.way].data[offset]);
}
  

void cache_t::write_data(address_t addr, cache_access_response_t car, int data) {
  int offset = gen_offset(addr);

  tags[car.set][car.way].data[offset] = data;
}

cache_access_response_t cache_t::lru_replacement(address_t addr) {
  cache_access_response_t car;

  // already in the cache?
  if (cache_access(addr, INVALID, &car)) {
    return(car);
  }


  bool done_p = false;
  int set = gen_set(addr);

  // find LRU way
  car.address_tag = gen_address_tag(addr);
  car.set = set;

  for (int a = 0; a < assoc; ++a) {
    if (tags[set][a].replacement == 0) {
      car.way = a;
      update_replacement(car);
      return(car);
    }
  }

  ERROR("should be a way that is LRU");
  exit(1);
}

// perfect LRU
void cache_t::update_replacement(cache_access_response_t car) {
  /* perfect LRU implemented using the replacement field.
     The most recently accessed cacheline will be at assoc - 1,
     with second most recently accessed cacheline will be at assoc -2,
     etc.  Make the accessed way the most recently used and shift the
     others down.  
  */
  
  int cur_replacement = tags[car.set][car.way].replacement;

  
  for (int a = 0; a < assoc; ++a) {
    // demote more recently used, if not, don't change.
    if (tags[car.set][a].replacement > cur_replacement) 
      --tags[car.set][a].replacement;
  }
  tags[car.set][car.way].replacement = assoc - 1;

  // consistency check.  We should have one of each value of replacement
  bool present[assoc];
  
  for (int a = 0; a < assoc; ++a) 
    present[a] = false;
  for (int a = 0; a < assoc; ++a) {
    int r = tags[car.set][a].replacement;

    if (present[r] == true) {
      ERROR("inconsistent replacement values");
    } else {
      present[r] = true;
    }
  }
}


response_t cache_t::load(address_t addr, bus_tag_t tag, int *data, bool retried_p) {
  response_t r;
  cache_access_response_t car;
  int a;
  

  if (cache_access(addr, SHARED, &car)) {
    if (!retried_p) ++full_hits;
    r.hit_p = true;
    r.retry_p = false;

    *data = read_data(addr, car);

    NOTE_ARGS(("%d: hit: addr %d, tag %d", node, addr, tag));

    update_replacement(car);

  } else {  // miss, service request
    if (!retried_p) {
      ++misses;
      proc_cmd_t proc_cmd = (proc_cmd_t){READ, addr, tag, SHARED};

      NOTE_ARGS(("%d: miss: addr %d, tag %d", node, addr, tag));

      // issue a read to next level of memory hierarchy.  In this case,
      // it's the IU
      
      if (iu->from_proc(proc_cmd)) {
	      ERROR("should not retry from_proc:load");
      }


      // create a response.  We know that it will take at least one
      // cycle to get a response and we are modeling a blocking cache
      // for now (if there is an outstanding miss, retry all memory
      // operations until that miss is statisfied.)
    }

    r.hit_p = false;
    r.retry_p = true; // blocking cache for now
  }
  return(r);
}


response_t cache_t::store(address_t addr, bus_tag_t tag, int data, bool retried_p) {
  response_t r;
  cache_access_response_t car;
  int a;

  cache_access(addr, EXCLUSIVE, &car);

  switch (car.permit_tag) {
  case INVALID: {
    if (!retried_p) {
      proc_cmd_t proc_cmd = (proc_cmd_t){READ, addr, tag, MODIFIED};
      if (iu->from_proc(proc_cmd)) {
	ERROR("should not retry from_proc:store INVALID");
      }

      ++misses;
      NOTE_ARGS(("%d: miss: addr %d, tag %d", node, addr, tag));
    }

    r.hit_p = false;
    r.retry_p = true;
    break;
  }

  case SHARED: {
    if (!retried_p) {
      proc_cmd_t proc_cmd = (proc_cmd_t){READ, addr, tag, MODIFIED};
      if (iu->from_proc(proc_cmd)) {
	ERROR("should not retry from_proc:store SHARED");
      }

      ++partial_hits;
      NOTE_ARGS(("%d: partial_hit: addr %d, tag %d", node, addr, tag));
    }

    r.hit_p = false;
    r.retry_p = true;
    break;
  }

  case EXCLUSIVE:
    if (!retried_p) ++full_hits;

    modify_permit_tag(car, MODIFIED);
    update_replacement(car);
    write_data(addr, car, data);

    r.hit_p = true;
    r.retry_p = false;
    break;

  case MODIFIED:
    if (!retried_p) ++full_hits;

    r.hit_p = true;
    r.retry_p = false;
    update_replacement(car);
    write_data(addr, car, data);
    NOTE_ARGS(("%d: hit: addr %d, tag %d", node, addr, tag));
    break;
  default:
    ERROR("store: illegal permission");
  }

  return(r);
}
