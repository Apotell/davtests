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
:name: mp_err_virtual
:description: illegal assignments between virtual interfaces with modports
:should_fail_because: virtual interface modports are part of the type and must match on assignment and access
:tags: 25.9.2
*/

// Errors in this file:
//   ERR1: virtual with modport TB assigned from a virtual with modport DUT
//   ERR2: virtual interface declared with a modport that does not exist
//   ERR3: virtual with modport assigned to a virtual without modport
//   ERR4: virtual with modport of interface A assigned a modport of interface B
//   ERR5: access through a virtual modport to a signal that the modport hides

interface A_Bus;
  logic req, gnt, hidden;
  modport DUT (input req, output gnt);
  modport TB (output req, input gnt);
endinterface

interface B_Bus;
  logic req, gnt;
  modport TB (output req, input gnt);
endinterface

module top;
  A_Bus a();
  B_Bus b();
  virtual A_Bus.TB  v_tb;
  virtual A_Bus.DUT v_dut;
  virtual A_Bus     v_plain;
  // ERR2
  virtual A_Bus.NOPE v_bad;
  logic y;
  initial begin
    v_dut = a;
    // ERR1
    v_tb = v_dut;
    // ERR3
    v_plain = v_tb;
    // ERR4
    v_tb = b.TB;
    // ERR5
    y = v_tb.hidden;
  end
endmodule
