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
:name: mp_err_import
:description: errors with subroutines imported through modports
:should_fail_because: an import prototype must match the subroutine declaration and calls must match the prototype
:tags: 25.7
*/

// Errors in this file:
//   ERR1: import prototype with a different argument count than the interface task
//   ERR2: import prototype with a different argument type than the interface function
//   ERR3: import prototype with different formal names than the declaration
//   ERR4: default argument used in a call when the modport imports by name only
//   ERR5: imported function called with too many arguments

interface bus_if;
  task t1(input int a);
  endtask
  function int f2(int a);
    return a;
  endfunction
  task t3(input int a);
  endtask
  task t4(input int a = 5);
  endtask
  function int f5(int a);
    return a;
  endfunction

  // ERR1
  modport mp1 (import task t1(input int a, input int b));
  // ERR2
  modport mp2 (import function int f2(string a));
  // ERR3
  modport mp3 (import task t3(input int other));
  modport mp4 (import t4);
  modport mp5 (import f5);
endinterface

// ERR4
module m4(bus_if.mp4 p);
  initial p.t4();
endmodule

// ERR5
module m5(bus_if.mp5 p);
  int r;
  initial r = p.f5(1, 2);
endmodule

module top;
  bus_if b();
  m4 u4(b);
  m5 u5(b);
endmodule
