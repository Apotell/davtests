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
:name: ic_err_variable_connections
:description: variables connected to interconnect ports or interconnect nets
:should_fail_because: it shall be illegal to connect a port variable to an interconnect port or interconnect net
:tags: 23.3.3
*/

// Errors in this file:
//   ERR1: logic variable connected to an input interconnect port
//   ERR2: int variable connected to an inout interconnect port
//   ERR3: interconnect net connected to an output logic variable port
//   ERR4: interconnect net connected to an output var logic port
//   ERR5: interconnect net connected to a ref port

module ic_in(input interconnect p);
endmodule

module ic_io(inout interconnect p);
endmodule

module var_out(output logic o);
  always_comb o = 1'b0;
endmodule

module var_out2(output var logic o);
endmodule

module ref_port(ref logic r);
endmodule

module top;
  logic v;
  int   iv;
  interconnect n3, n4;
  interconnect n5;
  // ERR1
  ic_in u1(v);
  // ERR2
  ic_io u2(iv);
  // ERR3
  var_out u3(n3);
  // ERR4
  var_out2 u4(n4);
  // ERR5
  ref_port u5(n5);
endmodule
