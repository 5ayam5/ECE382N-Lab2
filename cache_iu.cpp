// cache.c
//   by Derek Chiou
//      Oct. 8, 2023
// 

// for 382N-10

// STUDENTS: YOU ARE EXPECTED TO MODIFY THIS FILE.

#include <stdio.h>
#include "types.h"
#include "cache.h"
#include "iu.h"
#include "helpers.h"

response_t cache_t::snoop(proc_cmd_t wb) {
  response_t response;

  cache_access_response_t car;
  if (!cache_access(wb.addr, INVALID, &car)) {
    response.hit_p = false;
  } else {
    response.hit_p = true;
  }

  switch (wb.busop) {
    case WRITEBACK: {
      if (!response.hit_p) { // only conclusion is that the cache line has been evicted and writeback is still pending (which is why directory doesn't have up to date information)
        return(response);
      }
    }
    copy_cache_line(wb.data, tags[car.set][car.way].data);
    case READ:
    case INVALIDATE: {
      if (response.hit_p) {
        modify_permit_tag(car, wb.permit_tag);
      }
      response.hit_p = true;
      if (iu->from_proc_writeback(wb)) {
        response.retry_p = true;
        return(response);
      }
      NOTE_ARGS(("%d: writeback for addr_tag %d with new permit_tag %d", node, car.address_tag, wb.permit_tag));
      return(response);
    }

    default:
      ERROR("invalid busop");
  }
}


// ***** STUDENTS: You can change writeback logic if you really need, but should be OK with the existing implementation. ***** 
void cache_t::reply(proc_cmd_t proc_cmd) {
  // fill cache.  Since processor retries until load/store completes, only need to fill cache.

  cache_access_response_t car = lru_replacement(proc_cmd.addr);

  if (tags[car.set][car.way].permit_tag == MODIFIED) { // need to writeback since replacing modified line
    proc_cmd_t wb;
    wb.busop = WRITEBACK;
    wb.addr = (tags[car.set][car.way].address_tag << address_tag_shift) | (car.set << set_shift);
    copy_cache_line(wb.data, tags[car.set][car.way].data);
    if (iu->from_proc_writeback(wb)) {
      ERROR("should not retry a from_proc_writeback since there should only be one outstanding request");
    }
  }

  NOTE_ARGS(("%d: replacing addr_tag %d into set %d, assoc %d", node, car.address_tag, car.set, car.way));
  
  car.permit_tag = proc_cmd.permit_tag;
  cache_fill(car, proc_cmd.data);
}

