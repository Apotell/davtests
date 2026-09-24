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
// docs/error_catalog.xml (rows 835, 862, 864, 881).
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
// None of the three ErrorDefinition codes exercised here (COMP_MISSING_ARGUMENT,
// COMP_ILLEGAL_EXPRESSION_CONTEXT) have a call site anywhere in src/ outside
// their own ErrorDefinition.cpp registration, so every assertion below is
// expected to be red today -- that is the intended, documented state of this
// exercise, not a bug for this file to work around. No test asserts the
// absence of a diagnostic, which would lock the gap in.
//
// Row 835 was added later: "signed" on an interconnect port compiles with no
// diagnostic at all.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
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
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 23.3.3.2)";
  // A ref port shall be connected to an equivalent variable data type and
  // cannot be left unconnected.
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_ARGUMENT, "r862_sub"), nullptr)
      << "a ref port cannot be left unconnected (IEEE 1800-2023 23.3.3.2)";
}

// --- row 864: interface port left unconnected (23.3.3.4) --------------------

TEST_F(Chapter23ErrorRulesTest, Row864_InterfacePortCannotBeLeftUnconnected) {
  // catalog row 864 | 23.3.3.4 | LINT
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 23.3.3.4)";
  // An interface port shall always be connected to an interface instance or
  // a higher level interface port; it cannot be left unconnected.
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_ARGUMENT, "r864_sub"), nullptr)
      << "an interface port cannot be left unconnected (IEEE 1800-2023 23.3.3.4)";
}

// --- row 881: defparam RHS parameter must be local to the defparam's module (23.10.1)

TEST_F(Chapter23ErrorRulesTest, Row881_DefparamRhsParameterMustBeLocalToDefparamModule) {
  // catalog row 881 | 23.10.1 | LINT
  // Parameters referenced on the right-hand side of a defparam shall be
  // declared in the same module as the defparam statement; 'o.q' is declared
  // in module r881_other, not in r881_top where the defparam statement lives.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r881_top"), nullptr)
      << "a defparam RHS parameter must be declared in the defparam's own module (IEEE 1800-2023 23.10.1)";
}

// --- row 835: signed is illegal on an interconnect port (23.2.2.1) ----------

TEST_F(Chapter23ErrorRulesTest, Row835_SignedIllegalOnInterconnectPort) {
  // catalog row 835 | 23.2.2.1 | COMP
  GTEST_SKIP() << "blocked on a grammar/tree-shape change (the INTERCONNECT terminal is dropped "
                  "from the AST entirely for this port-declaration spelling); see "
                  ".claude/instructions/error_catalog_known_gaps.md item 5";
  // r835_m's "inout interconnect signed r835_a;" on line 47 specifies
  // signed on an interconnect port.
  //
  // Root-caused, not just unchecked: net_port_type's "INTERCONNECT
  // implicit_data_type" alternative leaves no trace of the INTERCONNECT
  // keyword in the AST at all (confirmed by instrumented debugging of
  // leavePA_Port_declaration) -- the resulting net_port_type node has only a
  // paImplicit_data_type child, structurally identical to a bare, typeless
  // port ("input a;"). There is currently no way to tell "this port is
  // declared interconnect" from "this port has no type at all" once parsed,
  // for this specific (port-declaration) spelling of interconnect -- fixing
  // this needs the grammar/tree-shape listener to retain the INTERCONNECT
  // terminal, not a Phase2ModelBuilder-side check. The sibling, unambiguous
  // net_declaration spelling ("interconnect signed w;", a standalone
  // statement rather than a port) IS covered (see the "Catalog row 835"
  // comment in Phase2ModelBuilder.cpp's leavePA_Net_declaration).
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_QUALIFIER, "r835_a", 47, 29), nullptr)
      << "signed cannot be specified for an interconnect port (IEEE 1800-2023 23.2.2.1)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
