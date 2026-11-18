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

// Tests for the IEEE 1800-2023 Clause 23 error scenarios catalogued in
// docs/error_catalog.xml (rows 862, 864, 881).
//
// Scope: this file asserts ONLY that the diagnostic each catalog row
// requires is emitted. It deliberately makes no assertion about the shape
// of the compiled model. Exactly one TEST_F per catalog row, named
// Row<N>_... after that row and carrying a "catalog row N | clause |
// category" comment; that is the link between the catalog and this file.
//
// Fixture: 23--error_rules.sv (rows 862, 864, 881), compiled in one run by
// 23--error_rules.hlc.
//
// CompMissingArgument (rows 862, 864) is reported by HLC's Linter
// (Linter::checkRefInstance). No test asserts the absence of a diagnostic,
// which would lock a gap in.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter23ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "23--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 862: ref port left unconnected (23.3.3.2) --------------------------

TEST_F(Chapter23ErrorRulesTest, Row862_RefPortCannotBeLeftUnconnected) {
  // catalog row 862 | 23.3.3.2 | LINT
  // A ref port shall be connected to an equivalent variable data type and
  // cannot be left unconnected. Instance u (line 20) leaves r862_sub's ref
  // port r unconnected.
  EXPECT_NE(findEvent(EventType::CompMissingArgument, "r", 20, 12), nullptr)
      << "a ref port cannot be left unconnected (IEEE 1800-2023 23.3.3.2)";
}

// --- row 864: interface port left unconnected (23.3.3.4) --------------------

TEST_F(Chapter23ErrorRulesTest, Row864_InterfacePortCannotBeLeftUnconnected) {
  // catalog row 864 | 23.3.3.4 | LINT
  // An interface port shall always be connected to an interface instance or
  // a higher level interface port; it cannot be left unconnected. Instance
  // u (line 29) leaves r864_sub's interface port i unconnected.
  EXPECT_NE(findEvent(EventType::CompMissingArgument, "i", 29, 12), nullptr)
      << "an interface port cannot be left unconnected (IEEE 1800-2023 23.3.3.4)";
}

// --- row 881: defparam RHS parameter must be local to the defparam's module (23.10.1)

TEST_F(Chapter23ErrorRulesTest, Row881_DefparamRhsParameterMustBeLocalToDefparamModule) {
  // catalog row 881 | 23.10.1 | LINT
  // Parameters referenced on the right-hand side of a defparam shall be
  // declared in the same module as the defparam statement; 'o.q' is declared
  // in module r881_other, not in r881_top where the defparam statement lives.
  EXPECT_NE(findEvent(EventType::CompIllegalExpressionContext, "r881_top"), nullptr)
      << "a defparam RHS parameter must be declared in the defparam's own module (IEEE 1800-2023 23.10.1)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
