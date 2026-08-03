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

// Tests for 6.9.1--logic_vector.sv (tags: 6.9.1)
//   module top();
//     logic [15:0] a;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.8 "Variable declarations",
// p.105, checked before any test code was written):
//   "data_declaration ::= [const] [var] [lifetime] data_type_or_implicit
//   list_of_variable_decl_assignments ;" with "integer_vector_type ::=
//   bit | logic | reg". "logic" is explicitly one of the keywords that
//   produces a variable_decl_assignment (a VARIABLE), and it never
//   appears in IEEE 1800-2023 6.7's net_type list (wire, tri, triand,
//   trior, trireg, tri0, tri1, uwire, wand, wor, supply0, supply1).
//   "logic [15:0] a;" declared directly in a module body (not as a port,
//   not given a net-type keyword) must therefore be a Variable, per this
//   grammar text, regardless of the fact that it is declared at module
//   scope.
//
//   A prior version of this test used hldb::Net/getNets() for "a"
//   throughout, and additionally asserted getNetType() == 0 and
//   getExplicitScalared() == false as if those were meaningful facts --
//   both are Net-only VPI properties (scalared/vectored per IEEE
//   1800-2023 6.9.2 only applies to nets) that only appeared not-set
//   because "a" should never have been a Net in the first place. This
//   matches this session's confirmed hldb bug: classifying a declared
//   signal as Net vs Variable by declaration SCOPE (module level -> Net)
//   rather than by the actual keyword. This version targets
//   hldb::Variable for "a" (real bug if it still resolves to Net).
//
// What is checked:
//   - module top exists, has exactly 1 Variable (not Net) named "a"
//   - "a" has no declaration-time value (plain declaration, no
//     initializer)
//   - "a" has a LogicTypespec (via RefTypespec), vpiVector=true (a range
//     was declared), and exactly 1 Range: left=15, right=0
//   - top has no processes, no continuous assignments
//   - compiler reports zero errors (this file is fully legal per 6.8)
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LogicVectorTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.9.1--logic_vector.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(LogicVectorTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(LogicVectorTest, ModuleHasNoNetsAndOneVariableA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'logic [15:0] a' declares no net-type keyword (IEEE 1800-2023 6.7) anywhere in this file";
  ASSERT_NE(top->getVariables(), nullptr)
      << "'logic [15:0] a' should be a Variable (IEEE 1800-2023 6.8: 'logic' is an "
         "integer_vector_type keyword); if this is null, hldb likely misclassified it as a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr) << "Variable 'a' not found";
}

TEST_F(LogicVectorTest, VariableHasNoInitialValue) {
  // 'logic [15:0] a' -- no initializer, vpiValue is absent
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue(), nullptr);
}

TEST_F(LogicVectorTest, VariableHasLogicTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rt = a->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(LogicVectorTest, LogicTypespecIsVector) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rt = a->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls = rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
}

TEST_F(LogicVectorTest, LogicTypespecHasOneRange) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rt = a->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls = rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  EXPECT_EQ(ls->getRanges()->size(), 1u);
}

TEST_F(LogicVectorTest, RangeLeftIs15) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rt = a->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls = rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  const hldb::Range *const range = ls->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const left = range->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "15");
}

TEST_F(LogicVectorTest, RangeRightIs0) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rt = a->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls = rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  const hldb::Range *const range = ls->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const right = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(LogicVectorTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(LogicVectorTest, NoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(LogicVectorTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
