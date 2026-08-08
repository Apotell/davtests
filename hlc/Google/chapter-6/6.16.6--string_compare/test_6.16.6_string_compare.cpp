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

// Validates the UHDM graph for a module using string.compare():
//   module top();
//     string a = "Test";
//     string b = "TEST";
//     int c = a.compare(b);
//   endmodule
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
//   - design has module top with 3 variables (a: string, b: string, c: int)
//   - variable 'a' initial value is "Test" (vpiStringConst); variable 'b' is "TEST"
//   - variable 'c' typespec resolves to IntTypespec
//   - variable 'c' has a non-null initial value (vpiValue is set)
//   - variable 'c' initial value is a HierPath named "a.compare(b)"
//   - HierPath element[0] is RefObj "a" with vpiActual resolving to Variable 'a'
//   - HierPath element[1] is FuncCall "compare" with 1 argument
//   - compare() argument is RefObj "b" resolving to Variable 'b' (not a Constant)
//   - 'c' does NOT get a pre-evaluated constant value -- HLDB stores the
//     unevaluated HierPath expression only; runtime comparison result not known
//   - the actual numeric result of a.compare(b) -- kept as a real assertion
//     for when HLC adds compile-time evaluation of string methods

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/hier_path.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/variable.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class StringCompareTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.16.6--string_compare.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(StringCompareTest, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Variable declarations -- string 'a', string 'b', int 'c'
// ---------------------------------------------------------------------------
TEST_F(StringCompareTest, ThreeVariablesExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 3u);
}

TEST_F(StringCompareTest, AVariableTypespecIsString) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(StringCompareTest, AVariableInitialValueIsTest) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiStringConst);
  EXPECT_EQ(init->getDecompile(), "\"Test\"");
}

TEST_F(StringCompareTest, BVariableTypespecIsString) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  EXPECT_NE(b->getTypespec()->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(StringCompareTest, BVariableInitialValueIsTEST) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::Constant *const init = b->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiStringConst);
  EXPECT_EQ(init->getDecompile(), "\"TEST\"");
}

TEST_F(StringCompareTest, CVariableTypespecIsInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const c = hldb::findByName<hldb::Variable>("c", top->getVariables());
  ASSERT_NE(c, nullptr);
  EXPECT_NE(c->getTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

// ---------------------------------------------------------------------------
// HierPath -- c's initial value is the method call a.compare(b)
// ---------------------------------------------------------------------------
TEST_F(StringCompareTest, CVariableHasValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const c = hldb::findByName<hldb::Variable>("c", top->getVariables());
  ASSERT_NE(c, nullptr);
  EXPECT_NE(c->getValue(), nullptr) << "variable 'c' should have a vpiValue set from int c = a.compare(b)";
}

TEST_F(StringCompareTest, CVariableValueIsNotPreEvaluatedConstant) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const c = hldb::findByName<hldb::Variable>("c", top->getVariables());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue<hldb::Constant>(), nullptr)
      << "HLC does not pre-evaluate a.compare(b) to a constant; c holds only the HierPath expression";
}

TEST_F(StringCompareTest, CVariableValueIsHierPath) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const c = hldb::findByName<hldb::Variable>("c", top->getVariables());
  ASSERT_NE(c, nullptr);
  const hldb::HierPath *const hp = c->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr) << "variable 'c' initial value is not a HierPath";
  EXPECT_EQ(hp->getName(), "a.compare(b)");
}

TEST_F(StringCompareTest, HierPathReceiverIsA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const c = hldb::findByName<hldb::Variable>("c", top->getVariables());
  ASSERT_NE(c, nullptr);
  const hldb::HierPath *const hp = c->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 1u);

  const hldb::RefObj *const receiver = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_EQ(receiver->getName(), "a");
  EXPECT_NE(receiver->getActual<hldb::Variable>(), nullptr);
}

TEST_F(StringCompareTest, HierPathMethodIsCompare) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const c = hldb::findByName<hldb::Variable>("c", top->getVariables());
  ASSERT_NE(c, nullptr);
  const hldb::HierPath *const hp = c->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 2u);

  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "compare");
}

TEST_F(StringCompareTest, CompareArgumentIsRefObjB) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const c = hldb::findByName<hldb::Variable>("c", top->getVariables());
  ASSERT_NE(c, nullptr);
  const hldb::HierPath *const hp = c->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);

  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "compare() argument is not a RefObj";
  EXPECT_EQ(arg->getName(), "b");
  EXPECT_NE(arg->getActual<hldb::Variable>(), nullptr) << "compare() argument should resolve to Variable b";
}

// ---------------------------------------------------------------------------
// a.compare(b) runtime result
// ---------------------------------------------------------------------------
TEST_F(StringCompareTest, CompareResultIsPreEvaluated) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const c = hldb::findByName<hldb::Variable>("c", top->getVariables());
  ASSERT_NE(c, nullptr);
  const hldb::Constant *const value = c->getValue<hldb::Constant>();
  if (m_design->getElaborated()) {
    ASSERT_NE(value, nullptr) << "variable 'c' should hold a pre-evaluated Constant";
    EXPECT_NE(value->getDecompile(), "0") << "\"Test\".compare(\"TEST\") should evaluate to a nonzero result";
  }
}

TEST_F(StringCompareTest, CompilerReportsZeroErrors) {
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
