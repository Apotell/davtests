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

#pragma once
// Pull in the artifact's own Surelog/Tests/Test.h (skip this shim directory).
#include_next <Surelog/Tests/Test.h>

// New-style artifacts (force-include hlc/config.h, not Surelog/config.h) put
// Test in namespace hlc. SURELOG_PATHID_DEBUG_ENABLED is defined only by the
// old-style Surelog/config.h force-include, so its absence means we are on a
// new-style artifact and need to bridge Test into namespace SURELOG so that
// old-style test files ("namespace SURELOG { class Foo : public Test }") work.
#ifndef SURELOG_PATHID_DEBUG_ENABLED
namespace SURELOG {
using ::hlc::Test;
}
#endif
