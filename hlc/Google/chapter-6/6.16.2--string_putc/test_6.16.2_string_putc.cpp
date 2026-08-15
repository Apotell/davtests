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

// Validates the UHDM graph for a module using string.putc():
//   module top();
//     string a = "Test";
//     initial a.putc(2, "B");
//   endmodule
// Key property: unlike len/getc/toupper, putc is a void call inside an
// Initial process (no result variable 'b'). The Initial stmt is a HierPath
// "a.putc(2, \"B\")" whose FuncCall carries two arguments: Constant 2 and
// Constant "B".
//
// What to check and why (IEEE 1800-2023 6.8 "Variable declarations", p.105):
// the identifiers declared in this file (string/int/byte/real -- see the
// module body above) are all 6.8 data_type keywords (string, integer_atom_type,
// non_integer_type), never IEEE 1800-2023 6.7 net_type keywords, so they must
// be Variables, not Nets, regardless of module-level scope. A prior version of
// this test used hldb::Net/getNets() throughout -- the same net/variable
// misclassification bug found and fixed across 6.5, 6.9.1, 6.12, 6.13, 6.14,
// and 6.16--string this session. This version targets hldb::Variable instead,
// and adds a CompilerReportsZeroErrors check (previously absent) since this
// file has no :should_fail_because: tag and is fully legal.
//
// Checked:
//   - design has module top with 1 variable (a: string)
//   - variable 'a' typespec resolves to StringTypespec; initial value is "Test" (vpiStringConst)
//   - top has 1 Initial process whose stmt is a HierPath named "a.putc(2, \"B\")"
//   - HierPath element[0] is RefObj "a"; element[1] is FuncCall "putc" with 2 arguments
//   - putc arguments are Constant "2" and Constant "B" (vpiStringConst)
//   - the in-place mutation of 'a' performed by putc (index 2 set to 'B') --
//     kept as a real assertion for when HLC adds compile-time evaluation of
//     string methods

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/variable.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class StringPutcTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.16.2--string_putc.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(StringPutcTest, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Variable -- only 'a' (string); putc is void so no result variable 'b'
// ---------------------------------------------------------------------------
TEST_F(StringPutcTest, OneVariableExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u) << "only variable 'a'; putc is void";
}

TEST_F(StringPutcTest, AVariableTypespecIsString) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(StringPutcTest, AVariableInitialValueIsTest) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiStringConst);
  EXPECT_EQ(init->getDecompile(), "\"Test\"");
}

// ---------------------------------------------------------------------------
// Initial process -- initial a.putc(2, "B")
// ---------------------------------------------------------------------------
TEST_F(StringPutcTest, InitialProcessExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(StringPutcTest, InitialStmtIsHierPath) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::HierPath *const hp = init->getStmt<hldb::HierPath>();
  ASSERT_NE(hp, nullptr) << "Initial stmt is not a HierPath";
  EXPECT_EQ(hp->getName(), "a.putc(2, \"B\")");
}

// ---------------------------------------------------------------------------
// HierPath -- receiver 'a' and FuncCall 'putc' with 2 arguments
// ---------------------------------------------------------------------------
TEST_F(StringPutcTest, HierPathReceiverIsA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::HierPath *const hp = init->getStmt<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 1u);

  const hldb::RefObj *const receiver = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr) << "HierPath pathElems[0] is not a RefObj";
  EXPECT_EQ(receiver->getName(), "a");
}

TEST_F(StringPutcTest, HierPathMethodIsPutc) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::HierPath *const hp = init->getStmt<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 2u);

  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "HierPath pathElems[1] is not a FuncCall";
  EXPECT_EQ(call->getName(), "putc");
}

TEST_F(StringPutcTest, PutcFirstArgumentIsTwo) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::HierPath *const hp = init->getStmt<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_GE(call->getArguments()->size(), 1u);

  const hldb::Constant *const arg0 = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg0, nullptr) << "putc first argument is not a Constant";
  EXPECT_EQ(arg0->getDecompile(), "2");
}

TEST_F(StringPutcTest, PutcSecondArgumentIsB) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::HierPath *const hp = init->getStmt<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_GE(call->getArguments()->size(), 2u);

  const hldb::Constant *const arg1 = any_cast<hldb::Constant>(call->getArguments()->at(1));
  ASSERT_NE(arg1, nullptr) << "putc second argument is not a Constant";
  EXPECT_EQ(arg1->getConstType(), vpiStringConst);
  EXPECT_EQ(arg1->getDecompile(), "\"B\"");
}

// ---------------------------------------------------------------------------
// a.putc(2, "B") runtime mutation
// ---------------------------------------------------------------------------
TEST_F(StringPutcTest, PutcMutationIsPreEvaluated) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const value = a->getValue<hldb::Constant>();
  if (m_design->getElaborated()) {
    ASSERT_NE(value, nullptr) << "variable 'a' should hold a pre-evaluated Constant reflecting the putc mutation";
    EXPECT_EQ(value->getDecompile(), "\"TeBt\"") << "a.putc(2, \"B\") should mutate 'a' to \"TeBt\"";
  }
}

TEST_F(StringPutcTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "putc"), nullptr)
      << "str.putc() must bind (IEEE 1800-2023 6.16.2)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
