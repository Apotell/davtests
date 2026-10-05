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

// Tests for tests/NonSynthError/dut.sv (compiled with HLC's -synth option):
//
//   module dut ();
//   parameter S = $size(int);
//   initial begin
//     $display();
//   end
//   class A;
//   endclass
//   endmodule
//
// What to check and why (IEEE 1800-2023):
//   - Sec 6.20.1: dut has no parameter_port_list, so "parameter S" declared
//     in the module body is an ordinary (overridable) parameter, not a
//     localparam.
//   - Sec 20.7 (Syntax 20-9): "array_dimension_function ( data_type [ ,
//     dimension_expression ] )" -- $size may take a data type argument such
//     as 'int'; the initializer of S is a call to the $size system function
//     whose argument is the int data type.
//   - Sec 9.2.1: an initial procedure; Sec 21.2.1: "$display();" with no
//     arguments is legal (prints a newline).
//   - Sec 8.3 / 23.2.4 (module_item may include class_declaration): class A
//     is declared within module dut and must be modeled as a class
//     definition owned by dut.
//   - The source is legal SystemVerilog: no syntax errors and no binding
//     errors.
//
// What is NOT checked and why:
//   - Whether -synth reports these constructs as non-synthesizable: the
//     synthesizable subset is a tool policy, not defined by IEEE 1800-2023,
//     so no assertion is made either way about HLDB_NON_SYNTHESIZABLE.
//   - The value of S ($size(int) == 32 per Sec 20.7/6.11.1): the .hlc does
//     not request elaboration, so the parameter is not reduced.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NonSynthErrorTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NonSynthError.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() { return hldb::findByName<hldb::Module>("dut", m_design->getAllModules()); }
};

TEST_F(NonSynthErrorTest, ModuleDutExists) { ASSERT_NE(getDut(), nullptr) << "module 'dut' not found"; }

// ---------------------------------------------------------------------------
// parameter S = $size(int);
// ---------------------------------------------------------------------------

TEST_F(NonSynthErrorTest, SIsOverridableParameter) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getParameters(), nullptr);
  const hldb::Parameter *const s = hldb::findByName<hldb::Parameter>("S", dut->getParameters());
  ASSERT_NE(s, nullptr);
  EXPECT_FALSE(s->getLocalParam()) << "Sec 6.20.1: no parameter_port_list -> body 'parameter' is overridable";
}

TEST_F(NonSynthErrorTest, SInitializerIsSizeOfInt) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getParamAssigns(), nullptr);
  ASSERT_EQ(dut->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = dut->getParamAssigns()->at(0);
  const hldb::SysFuncCall *const call = any_cast<hldb::SysFuncCall>(pa->getRhs());
  ASSERT_NE(call, nullptr) << "Sec 20.7: $size(...) is a system function call";
  EXPECT_EQ(call->getName(), std::string_view("$size"));

  // The data type argument 'int' must be reachable from the call, either as
  // an argument or as the call's attached data-type operand.
  bool foundInt = false;
  if (call->getArguments() != nullptr) {
    for (const hldb::Any *const arg : *call->getArguments()) {
      if (const hldb::RefTypespec *const rt = any_cast<hldb::RefTypespec>(arg)) {
        if (rt->getActual<hldb::IntTypespec>() != nullptr) foundInt = true;
      }
    }
  }
  if (!foundInt && (call->getTypespec() != nullptr)) {
    foundInt = (call->getTypespec()->getActual<hldb::IntTypespec>() != nullptr);
  }
  EXPECT_TRUE(foundInt) << "$size's data_type argument 'int' not found";
}

// ---------------------------------------------------------------------------
// initial begin $display(); end
// ---------------------------------------------------------------------------

TEST_F(NonSynthErrorTest, InitialWithDisplayNoArgs) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getProcesses(), nullptr);
  ASSERT_EQ(dut->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(dut->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const b = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getStmts(), nullptr);
  ASSERT_EQ(b->getStmts()->size(), 1u);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(b->getStmts()->at(0));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), std::string_view("$display"));
  const size_t nbArgs = (disp->getArguments() == nullptr) ? 0u : disp->getArguments()->size();
  EXPECT_EQ(nbArgs, 0u);
}

// ---------------------------------------------------------------------------
// class A; endclass  (inside module dut)
// ---------------------------------------------------------------------------

TEST_F(NonSynthErrorTest, ClassADeclaredInDut) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getClassDefns(), nullptr);
  ASSERT_EQ(dut->getClassDefns()->size(), 1u);
  const hldb::ClassDefn *const a = dut->getClassDefns()->at(0);
  EXPECT_EQ(a->getName(), std::string_view("A"));
  EXPECT_EQ(a->getParent(), dut);
}

// ---------------------------------------------------------------------------
// Diagnostics: legal source
// ---------------------------------------------------------------------------

TEST_F(NonSynthErrorTest, NoSyntaxOrBindingErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_SYSTEM_FUNCTION, "$size"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
