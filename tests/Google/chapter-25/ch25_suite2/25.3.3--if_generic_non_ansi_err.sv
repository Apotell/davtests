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
:name: if_generic_non_ansi_err
:description: a generic interface port declared in a non-ANSI header
:should_fail_because: a generic interface reference can only be declared in the ANSI list of port declarations
:tags: 25.3.3
*/

module sink(p);
  interface p;
endmodule
