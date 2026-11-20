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

// 23.3.1: a local path takes precedence over a top-level path with the same
// names; "$root allows explicit access to the top of the instantiation tree".
// Here A is both a top-level instance and a local instance inside top.
module leaf;
  logic x;
endmodule

module B;
  leaf C ();
endmodule

module A;
  B B ();
endmodule

module A2;
  B B ();
endmodule

module top;
  logic l, r;
  A2 A ();
  assign l = A.B.C.x;
  assign r = $root.A.B.C.x;
endmodule
