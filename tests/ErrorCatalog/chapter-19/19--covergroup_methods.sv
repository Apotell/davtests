/*
:name: covergroup_methods
:description: IEEE 1800-2023 Table 19-5 predefined coverage methods
:tags: 19.8 19.9
*/

// Google has no coverage of any of the six predefined coverage methods -- the only
// ".start(" calls under tests/Google are UVM sequence starts, not covergroup ones -- so
// this file is their whole coverage.
//
// The instance is deliberately never constructed. Binding these methods is a static,
// declared-type question, and calling new() would only add the unrelated
// constructor-binding gap to the golden.

module top();
  bit clk;
  bit [3:0] a, b;
  real r;

  covergroup cg1 @(posedge clk);
    cp_a : coverpoint a;
    cp_b : coverpoint b;
    x_ab : cross cp_a, cp_b;
  endgroup

  cg1 inst;

  initial begin
    inst.sample();                     // Table 19-5: void sample()
    inst.start();                      // Table 19-5: void start()
    inst.stop();                       // Table 19-5: void stop()
    r = inst.get_coverage();           // Table 19-5: real get_coverage()
    r = inst.get_inst_coverage();      // Table 19-5: real get_inst_coverage()
    inst.set_inst_name("top.inst");    // Table 19-5: void set_inst_name(string)
  end
endmodule
