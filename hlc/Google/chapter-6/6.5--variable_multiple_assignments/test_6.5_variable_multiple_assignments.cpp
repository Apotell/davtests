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

// Tests for 6.5--variable_multiple_assignments.sv (tags: 6.5)
//   :should_fail_because: it shall be an error to have multiple continuous assignments
//   module top();
//     int v;
//     assign v = 12;
//     assign v = 13;
//   endmodule
//
// What to check and why (IEEE 1800-2023 10.3.2, p.248-249, checked before
// any test code was written):
//   "Nets can be driven by multiple continuous assignments ... Variables
//   can only be driven by ONE continuous assignment or by one primitive
//   output or module output."
//   "v" is declared "int" (a variable-type keyword, not in IEEE
//   1800-2023 6.7's net_type list) and receives TWO continuous
//   assignments -- legal for a net, illegal for a variable. This matches
//   the file's own :should_fail_because: tag exactly.
//
//   A prior version of this test used hldb::Net/getNets() for "v" and
//   had a Compiler_NoErrorsReported test asserting nbError == 0,
//   documented as "HLC does not reject two continuous assignments...".
//   Both encoded suspected/confirmed bugs as correct behavior. This
//   version targets hldb::Variable for "v" (real bug if it resolves to
//   Net instead), and asserts an error IS reported (real bug, currently
//   failing).
//
// What is checked:
//   - module top has zero Nets and exactly 1 Variable, "v" (int, no
//     declaration-time initializer)
//   - exactly 2 ContAssigns, both with LHS RefObj "v" resolving via
//     getActual<hldb::Variable>() (not Net) to that same Variable;
//     first RHS Constant "12", second RHS Constant "13"
//   - both RHS constants have vpiConstType unsigned int (unsized integer
//     literals)
//   - top has no processes
//   - THE POINT OF THIS FILE: the compiler should report at least one
//     error for the two continuous assignments driving the same
//     variable, per IEEE 1800-2023 10.3.2 quoted above -- a real,
//     non-skipped, currently-failing assertion
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
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class VariableMultipleAssignmentsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.5--variable_multiple_assignments.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- existence: module, and "v" as a Variable (not a Net) -----------------

TEST_F(VariableMultipleAssignmentsTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(VariableMultipleAssignmentsTest, ModuleHasNoNetsAndOneVariableV) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'int v' declares no net-type keyword anywhere in this file";
  ASSERT_NE(top->getVariables(), nullptr)
      << "'int v' should be a Variable; if this is null, hldb likely misclassified it as a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", top->getVariables());
  ASSERT_NE(v, nullptr);
  EXPECT_NE(v->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  EXPECT_EQ(v->getValue<hldb::Constant>(), nullptr) << "'int v;' has no inline initializer";
}

// --- shape: both continuous assignments target the same Variable ----------

TEST_F(VariableMultipleAssignmentsTest, TwoContAssignsExist) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 2u);
}

TEST_F(VariableMultipleAssignmentsTest, BothContAssignsResolveToVariableVNotNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 2u);
  for (const hldb::ContAssign *const ca : *top->getContAssigns()) {
    const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), "v");
    EXPECT_EQ(lhs->getActual<hldb::Net>(), nullptr) << "'v' must not resolve to a Net";
    EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr) << "'v' should resolve to the Variable";
  }
}

TEST_F(VariableMultipleAssignmentsTest, FirstContAssignRhsIsConstantTwelve) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Constant *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "12");
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst);
}

TEST_F(VariableMultipleAssignmentsTest, SecondContAssignRhsIsConstantThirteen) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Constant *const rhs = top->getContAssigns()->at(1)->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "13");
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst);
}

// --- absence: no procedural driver at all ----------------------------------

TEST_F(VariableMultipleAssignmentsTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// --- the actual point of the file: two cont-assigns to a var is illegal ---

TEST_F(VariableMultipleAssignmentsTest, CompilerShouldRejectTwoContinuousAssignmentsToOneVariableButDoesNot) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "IEEE 1800-2023 10.3.2: 'variables can only be driven by one continuous assignment' -- "
         "'v' has two here, matching this file's own :should_fail_because: tag -- HLC currently "
         "accepts it with zero diagnostics";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
