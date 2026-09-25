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

// Tests for tests/EmptyAssign/dut.sv:
//
//   module dut #(parameter a = 1, b = 2, c = 3) ();
//   initial begin
//     $display("%0d %0d %0d", a, b, c);
//     if (b === 2)
//       $display("PASSED");
//     else
//       $display("FAILED");
//   end
//   endmodule
//
//   module top();
//   dut #(.a(4), .b(), .c(5)) dut();
//   endmodule // top
//
// The instantiation "dut #(.a(4), .b(), .c(5)) dut();" uses named
// parameter value assignment (IEEE 1800-2023 Annex A.2.3:
// "named_parameter_assignment ::= . parameter_identifier ( [ param_expression ] )")
// -- the expression inside the parens is explicitly optional. Sec 23.10
// "Parameterized modules" states that when the expression is omitted the
// parameter retains its previous (default) value -- an empty "()" is not
// the same as omitting the whole named assignment; it is an explicit
// no-op override that keeps parameter "b" at its default value of 2. The
// dut's own "if (b === 2) $display(\"PASSED\")" check is exactly this
// behavior made observable at simulation time.
//
// Checked:
//   - module "dut" exists and declares parameters a, b, c (non-localparam)
//     with default ParamAssign RHS Constants "1", "2", "3"
//   - module "top" exists and instantiates "dut" via RefInstance "dut"
//   - the instance carries a by-name, overriding ParamAssign for "a" with
//     RHS Constant "4"
//   - the instance carries a by-name, overriding ParamAssign for "c" with
//     RHS Constant "5"
//   - the instance carries a by-name ParamAssign for "b" whose RHS is
//     absent (no param_expression given) and which is not marked as
//     overriding the default (Sec 23.10: omitted expression keeps default)
//   - compiler reports zero errors

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class EmptyAssignTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "EmptyAssign.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() { return hldb::findByDefName<hldb::Module>("dut", m_design->getAllModules()); }

  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  template <typename ScopeT>
  static const hldb::ParamAssign *findParamAssign(const ScopeT *scope, std::string_view name) {
    return (scope == nullptr) ? nullptr : hldb::findByName(name, hldb::getParamAssigns(scope));
  }
};

// ---------------------------------------------------------------------------
// Modules
// ---------------------------------------------------------------------------

TEST_F(EmptyAssignTest, ModuleDutExists) { EXPECT_NE(getDut(), nullptr) << "module 'dut' not found"; }

TEST_F(EmptyAssignTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr) << "module 'top' not found"; }

// ---------------------------------------------------------------------------
// module dut #(parameter a = 1, b = 2, c = 3) ();
// ---------------------------------------------------------------------------

TEST_F(EmptyAssignTest, DutHasThreeNonLocalParameters) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getParameters(), nullptr);
  EXPECT_EQ(dut->getParameters()->size(), 3u);
  for (std::string_view name : {"a", "b", "c"}) {
    const hldb::Parameter *param = nullptr;
    for (const hldb::Any *const p : *dut->getParameters()) {
      const hldb::Parameter *const candidate = any_cast<hldb::Parameter>(p);
      if (candidate != nullptr && candidate->getName() == name) {
        param = candidate;
        break;
      }
    }
    ASSERT_NE(param, nullptr) << "parameter '" << name << "' not found on module 'dut'";
    EXPECT_FALSE(param->getLocalParam()) << "'" << name << "' is a module parameter, not a localparam";
  }
}

TEST_F(EmptyAssignTest, DutDefaultParamAssignsAreOneTwoThree) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(dut, "a");
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'a' not found";
  const hldb::Constant *const rhsA = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhsA, nullptr) << "'parameter a = 1': default RHS must be a Constant";
  EXPECT_EQ(std::string(rhsA->getDecompile()), "1");

  const hldb::ParamAssign *const pb = findParamAssign(dut, "b");
  ASSERT_NE(pb, nullptr) << "default ParamAssign for 'b' not found";
  const hldb::Constant *const rhsB = pb->getRhs<hldb::Constant>();
  ASSERT_NE(rhsB, nullptr) << "'parameter b = 2': default RHS must be a Constant";
  EXPECT_EQ(std::string(rhsB->getDecompile()), "2");

  const hldb::ParamAssign *const pc = findParamAssign(dut, "c");
  ASSERT_NE(pc, nullptr) << "default ParamAssign for 'c' not found";
  const hldb::Constant *const rhsC = pc->getRhs<hldb::Constant>();
  ASSERT_NE(rhsC, nullptr) << "'parameter c = 3': default RHS must be a Constant";
  EXPECT_EQ(std::string(rhsC->getDecompile()), "3");
}

// ---------------------------------------------------------------------------
// dut #(.a(4), .b(), .c(5)) dut();
// ---------------------------------------------------------------------------

TEST_F(EmptyAssignTest, TopInstantiatesDutAsDut) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("dut", top->getRefInstances());
  ASSERT_NE(inst, nullptr) << "'dut #(...) dut(...)' RefInstance not found in 'top'";
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr) << "instance's typespec is not ModuleTypespec";
  EXPECT_EQ(mt->getName(), std::string_view("dut"));
}

TEST_F(EmptyAssignTest, InstanceOverridesAWithFour) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("dut", top->getRefInstances());
  ASSERT_NE(inst, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(inst, "a");
  ASSERT_NE(pa, nullptr) << "'.a(4)' override not found on the 'dut' instance";
  EXPECT_TRUE(pa->getConnByName()) << "'.a(4)' is a by-name parameter connection (Sec 23.3)";
  EXPECT_TRUE(pa->getOverridden()) << "an explicit instance-level override must be marked as overriding the default";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'.a(4)' RHS must be a Constant";
  EXPECT_EQ(std::string(rhs->getDecompile()), "4");
}

TEST_F(EmptyAssignTest, InstanceOverridesCWithFive) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("dut", top->getRefInstances());
  ASSERT_NE(inst, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(inst, "c");
  ASSERT_NE(pa, nullptr) << "'.c(5)' override not found on the 'dut' instance";
  EXPECT_TRUE(pa->getConnByName()) << "'.c(5)' is a by-name parameter connection (Sec 23.3)";
  EXPECT_TRUE(pa->getOverridden()) << "an explicit instance-level override must be marked as overriding the default";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'.c(5)' RHS must be a Constant";
  EXPECT_EQ(std::string(rhs->getDecompile()), "5");
}

TEST_F(EmptyAssignTest, InstanceEmptyBOverrideKeepsDefault) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("dut", top->getRefInstances());
  ASSERT_NE(inst, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(inst, "b");
  ASSERT_NE(pa, nullptr) << "'.b()' named parameter assignment (with omitted expression) not found on the "
                            "'dut' instance -- Sec 23.10 still requires a named parameter assignment entry "
                            "to be recorded, even with no expression";
  EXPECT_TRUE(pa->getConnByName()) << "'.b()' is still a by-name parameter connection (Sec 23.3)";
  EXPECT_FALSE(pa->getOverridden())
      << "Sec 23.10: '.b()' omits the param_expression, so parameter 'b' keeps its default value of 2 and "
         "must not be marked as an overriding assignment";
  EXPECT_EQ(pa->getRhs(), nullptr) << "'.b()' provides no param_expression, so its RHS must be absent";
}

// ---------------------------------------------------------------------------
// Compiler diagnostics
// ---------------------------------------------------------------------------

TEST_F(EmptyAssignTest, CompilerReportsZeroErrors) {
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
