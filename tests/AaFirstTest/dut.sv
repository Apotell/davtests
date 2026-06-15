  function void execute(uvm_component comp, uvm_phase phase);
    fork
      begin
        process proc;

        // reseed this process for random stability
        proc = process::self();
        proc.srandom(uvm_create_random_seed(phase.get_type_name(), comp.get_full_name()));

        phase.m_num_procs_not_yet_returned++;

        exec_task(comp,phase);

        phase.m_num_procs_not_yet_returned--;

      end
    join_none

  endfunction