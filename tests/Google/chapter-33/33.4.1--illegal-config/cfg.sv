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

// Illegal configuration rules (IEEE 1800-2023 Sec 33.4.1.3, 33.4.1.4).

config cfg;
  design lib1.top;
  default liblist lib1 lib2;
  cell lib2.m liblist lib1;
  cell m liblist lib2;
  instance other.u1 use lib2.m;
  instance top.u2 use lib2.m;
endconfig
