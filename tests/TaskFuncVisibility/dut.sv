// IEEE 1800-2023 Sec 37.41 detail 4: vpiVisibility of a task/function is
// vpiPublicVis for a non-member and for a member that is not local/protected.

package pkg;
  task pkg_task();
  endtask

  class C;
    function new();
    endfunction

    task public_task();
    endtask

    local task local_task();
    endtask

    protected task protected_task();
    endtask
  endclass

  class D;
    local function new();
    endfunction
  endclass

  class E;
    protected function new();
    endfunction
  endclass
endpackage

interface ifc;
  task ifc_task();
  endtask
endinterface

module top;
  task mod_task();
  endtask

  task automatic mod_auto_task(input int a);
  endtask
endmodule
