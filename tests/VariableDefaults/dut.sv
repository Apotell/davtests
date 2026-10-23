// IEEE 1800-2023 Sec 37.17 variable properties that always have a value:
//   details 15/22  vpiRandType is one of vpiRand, vpiRandC or vpiNotRand
//   detail 24      vpiVisibility is vpiPublicVis for a variable that is not a class member

module top;
  logic mod_logic;
  int mod_int;
  task automatic t;
    int task_local;
  endtask
endmodule

class cls;
  rand int r;
  randc bit [3:0] rc;
  int plain;
  local int loc;
  protected int prot;
endclass
