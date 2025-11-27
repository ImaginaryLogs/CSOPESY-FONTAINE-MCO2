#include "kernel/scheduler.hpp"

// === Paging & Swapping (Medium-term scheduler) ===

void handle_page_fault(std::shared_ptr<Process> p, uint64_t fault_addr);


void swap_out_process(std::shared_ptr<Process> p);


void swap_in_process(std::shared_ptr<Process> p);

void medium_term_check(){
    
};