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
:name: if_item_signals
:description: data and net declarations inside an interface
:tags: 25.3
*/

// Checks in this file:
//   CHK1: several variables on one line: logic a, b, c;
//   CHK2: each variable on its own line
//   CHK3: packed vectors, unpacked arrays and a queue
//   CHK4: wire and tri nets with a continuous assign
//   CHK5: typedef struct declared in the interface and used for a signal

// CHK1
interface c1_if;
  logic a, b, c;
endinterface

// CHK2
interface c2_if;
  logic a;
  logic b;
  logic c;
endinterface

// CHK3
interface c3_if;
  logic [7:0] addr, data;
  logic [3:0][7:0] packed2d;
  logic [7:0] mem [0:15];
  int queue_q[$];
endinterface

// CHK4
interface c4_if;
  wire w1, w2;
  tri [3:0] t;
  assign w1 = 1'b1;
endinterface

// CHK5
interface c5_if;
  typedef struct packed {
    logic [7:0] addr;
    logic       wr;
  } req_t;
  req_t req;
endinterface

module top;
  c1_if u1();
  c2_if u2();
  c3_if u3();
  c4_if u4();
  c5_if u5();
  assign u1.a = u1.b & u1.c;
  assign u2.a = u2.b | u2.c;
  assign u3.addr = u3.mem[3];
  assign u5.req.wr = 1'b0;
endmodule
