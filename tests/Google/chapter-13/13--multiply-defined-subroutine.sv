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

// Tasks and functions declared twice in one scope (IEEE 1800-2023 Sec 3.13, 13).

package p;
  task pt; endtask
  task pt; endtask
  function int pf(); return 0; endfunction
  function int pf(); return 1; endfunction
endpackage
class C;
  task ct; endtask
  task ct; endtask
  extern function void e();
  function void ok(); endfunction
endclass
function void C::e(); endfunction
interface I;
  task it; endtask
  task it; endtask
endinterface
module top;
  task t; endtask
  task t; endtask
  task u; endtask
  function void v(); endfunction
endmodule
