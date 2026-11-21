/*
 Copyright 2020 Apotell

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

/*
:name: if_tasks_functions
:description: tasks and functions declared in an interface
:tags: 25.7
*/

// Checks in this file:
//   CHK1: function called through the instance name in a continuous assign
//   CHK2: task called through a module interface port
//   CHK3: void function called as a statement
//   CHK4: function called through a generic interface port

interface bus_if(input logic clk);
  logic [7:0] data;
  logic req;
  int count;
  // CHK1 + CHK4
  function automatic logic [7:0] inc(logic [7:0] v);
    return v + 1;
  endfunction
  // CHK2
  task automatic drive(input logic v);
    @(posedge clk) req <= v;
  endtask
  // CHK3
  function automatic void bump();
    count++;
  endfunction
endinterface

module drv(bus_if b);
  initial b.drive(1'b1);
endmodule

module gen(interface g);
  logic [7:0] r;
  assign r = g.inc(8'd4);
endmodule

module top;
  logic clk;
  bus_if b(clk);
  logic [7:0] r;
  assign r = b.inc(b.data);
  drv u_drv(.b(b));
  initial b.bump();
  gen u_gen(.g(b));
endmodule
