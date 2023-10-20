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

// ***** FYTD ***** 
response_t cache_t::snoop(proc_cmd_t proc_cmd) {
  response_t r;
  return(r);
}


// ***** STUDENTS: You can change writeback logic if you really need, but should be OK with the existing implementation. ***** 
void cache_t::reply(proc_cmd_t proc_cmd) {
  // fill cache.  Since processor retries until load/store completes, only need to fill cache.

  cache_access_response_t car = lru_replacement(proc_cmd.addr);

  if (tags[car.set][car.way].permit_tag == MODIFIED) { // need to writeback since replacing modified line
    proc_cmd_t wb;
    wb.busop = WRITEBACK;
    // ***** FYTD: calculate the correct writeback address ***** 
    wb.addr = 0; // need a value for now
    copy_cache_line(wb.data, tags[car.set][car.way].data);
    if (iu->from_proc_writeback(wb)) {
      ERROR("should not retry a from_proc_writeback since there should only be one outstanding request");
    }
  }

  NOTE_ARGS(("%d: replacing addr_tag %d into set %d, assoc %d", node, car.address_tag, car.set, car.way));
  
  car.permit_tag = proc_cmd.permit_tag;
  cache_fill(car, proc_cmd.data);
}

