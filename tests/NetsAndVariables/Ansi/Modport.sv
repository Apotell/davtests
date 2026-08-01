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

// Basic modport coverage: a modport exposing a net port, a variable port,
// and an implicit net port (declared nowhere in the interface except via
// the continuous assignment below -- IEEE 1800 clause 6.10).

interface nets_and_variables_modport_if;
  wire mp_net; // net declaration exposed via modport
  logic mp_var; // variable declaration exposed via modport
  assign mp_implicit = mp_var; // implicit net declaration via continuous assignment

  modport mp_basic(
    input mp_net,
    output mp_var,
    input mp_implicit
  );
endinterface
