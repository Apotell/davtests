module top;
  event m_ev;

  task static st();
    event st_ev;
  endtask

  task automatic at();
    event at_ev;
  endtask

  function automatic void af();
    event af_ev;
  endfunction
endmodule

class C;
  event c_ev;
  static event c_sev;

  task t();
    event ct_ev;
  endtask
endclass
