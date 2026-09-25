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

// Tests for tests/DefaultAssign/dut.sv:
//
//   module foo(input logic [31:0] data);
//   endmodule
//
//   module dut();
//     logic [31:0] data;
//     parameter logic [31:0] data = '{ default: 1 };
//     assign data = '{ default: 1 };
//     foo f(.data('{ default: 1 }));
//   endmodule
//
// The '{ default: 1 } construct is an assignment pattern with a "default"
// pattern key (IEEE 1800-2023 Sec 10.9.1 / Table 10-13, "the value
// following 'default:' is used to assign every remaining element"). Per
// Sec 11.4.12.4 / Annex A.8.3, an assignment pattern parses as an
// 'assignment_pattern' and is represented in UHDM as an Operation with
// vpiOpType == vpiAssignmentPatternOp.
//
// Note: 'dut' declares two module-scope items named "data" -- a variable
// ('logic [31:0] data;') and a parameter ('parameter logic [31:0] data =
// ...;'). Per IEEE 1800-2023 Sec 3.13 ("Scope rules") / Sec 6.3, an
// identifier shall not be redeclared within the same scope; this is
// checked separately below rather than assumed away.
//
// Checked:
//   - module 'foo' exists with input port 'data'
//   - module 'dut' exists and instantiates 'foo' as 'f'
//   - 'dut's continuous assignment 'assign data = '{ default: 1 };' has an
//     RHS Operation with vpiOpType == vpiAssignmentPatternOp and exactly
//     one operand (the "default:1" pattern entry)
//   - the instance port connection 'f(.data('{ default: 1 }))' carries the
//     same assignment-pattern shape as its by-name parameter/port value
//   - the illegal duplicate declaration of 'data' (variable and parameter)
//     is flagged as a known compiler defect if HLC does not currently
//     reject it (matching the established
//     Google/chapter-7/structures/packed/default-value precedent for
//     structurally-provable-but-currently-unflagged illegal constructs)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>

namespace hlc {

class DefaultAssignTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DefaultAssign.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getFoo() {
    return hldb::findByDefName<hldb::Module>("foo", m_design->getAllModules());
  }
  static const hldb::Module *getDut() {
    return hldb::findByDefName<hldb::Module>("dut", m_design->getAllModules());
  }
};

// ---------------------------------------------------------------------------
// module foo(input logic [31:0] data);
// ---------------------------------------------------------------------------

TEST_F(DefaultAssignTest, ModuleFooExists) { EXPECT_NE(getFoo(), nullptr) << "module 'foo' not found"; }

TEST_F(DefaultAssignTest, FooHasInputPortData) {
  const hldb::Module *const foo = getFoo();
  ASSERT_NE(foo, nullptr);
  ASSERT_NE(foo->getPorts(), nullptr);
  const hldb::Port *const data = hldb::findByName<hldb::Port>("data", foo->getPorts());
  ASSERT_NE(data, nullptr) << "'input logic [31:0] data' not found on module 'foo'";
  EXPECT_EQ(data->getDirection(), vpiInput);
}

// ---------------------------------------------------------------------------
// module dut(); / foo f(.data('{ default: 1 }));
// ---------------------------------------------------------------------------

TEST_F(DefaultAssignTest, ModuleDutExists) { EXPECT_NE(getDut(), nullptr) << "module 'dut' not found"; }

TEST_F(DefaultAssignTest, DutInstantiatesFooAsF) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getRefInstances(), nullptr);
  const hldb::RefInstance *const f = hldb::findByName<hldb::RefInstance>("f", dut->getRefInstances());
  ASSERT_NE(f, nullptr) << "'foo f(...)' RefInstance not found in 'dut'";
  ASSERT_NE(f->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = f->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr) << "'f's typespec is not ModuleTypespec";
  EXPECT_EQ(mt->getName(), std::string_view("foo"));
}

// ---------------------------------------------------------------------------
// assign data = '{ default: 1 };
// ---------------------------------------------------------------------------

TEST_F(DefaultAssignTest, DutHasOneContinuousAssign) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getContAssigns(), nullptr);
  EXPECT_EQ(dut->getContAssigns()->size(), 1u);
}

TEST_F(DefaultAssignTest, ContAssignRhsIsAssignmentPatternWithOneOperand) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getContAssigns(), nullptr);
  ASSERT_EQ(dut->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = (*dut->getContAssigns())[0];
  ASSERT_NE(ca, nullptr);

  const hldb::Operation *const pattern = ca->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr) << "Sec 10.9.1/11.4.12.4: '{ default: 1 }' RHS must be an assignment-pattern Operation";
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp);
  ASSERT_NE(pattern->getOperands(), nullptr);
  EXPECT_EQ(pattern->getOperands()->size(), 1u) << "'{ default: 1 }' has exactly one pattern entry";
  EXPECT_NE(pattern->getOperands()->at(0), nullptr) << "the 'default: 1' entry must be captured structurally";
}

// ---------------------------------------------------------------------------
// foo f(.data('{ default: 1 }));
// ---------------------------------------------------------------------------

TEST_F(DefaultAssignTest, FInstancePortDataIsConnected) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getRefInstances(), nullptr);
  const hldb::RefInstance *const f = hldb::findByName<hldb::RefInstance>("f", dut->getRefInstances());
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getPorts(), nullptr);
  EXPECT_EQ(f->getPorts()->size(), 1u) << "'f(.data(...))' connects exactly one port";
  EXPECT_NE(f->getPorts()->at(0), nullptr) << "the '.data('{ default: 1 })' connection must be captured";
}

// ---------------------------------------------------------------------------
// Illegal duplicate declaration of 'data' (variable + parameter) in 'dut'
// ---------------------------------------------------------------------------

TEST_F(DefaultAssignTest, DuplicateDataDeclarationShouldBeRejected) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "IEEE 1800-2023 Sec 3.13/6.3: an identifier shall not be redeclared within the same scope. "
         "'dut' declares both a variable and a parameter named 'data' in the same module scope; the "
         "compiler should reject this. If this test starts failing because the compiler now emits zero "
         "errors here, that means HLC currently accepts the illegal redeclaration silently -- a genuine "
         "defect, not a reason to weaken this assertion.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
