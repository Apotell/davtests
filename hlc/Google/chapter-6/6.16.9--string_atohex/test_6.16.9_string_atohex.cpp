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

// Validates the UHDM graph for a module using string.atohex():
//   module top();
//     string a = "0xff";
//     int b = a.atohex();
//   endmodule
//
// Checked:
//   - design has module top with 2 variables (a: string, b: int) -- IEEE 1800-2023
//     6.8: declarations with no net-type keyword are variables, not nets
//   - variable 'a' typespec resolves to StringTypespec; initial value is "0xff" (vpiStringConst)
//   - variable 'b' typespec resolves to IntTypespec
//   - variable 'b' has a non-null initial value (vpiValue is set)
//   - variable 'b' initial value is a HierPath named "a.atohex()"
//   - HierPath element[0] is RefObj "a" with vpiActual resolving to Variable 'a'
//   - HierPath element[1] is FuncCall "atohex" with no arguments
//   - 'b' does NOT get a pre-evaluated constant value (e.g. 255) -- HLDB stores
//     the unevaluated HierPath expression only

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/hier_path.h>
#include <hldb/int_typespec.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class StringAtohex : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.16.9--string_atohex.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(StringAtohex, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ----
// Variable declarations -- string 'a' and int 'b'
// ----
TEST_F(StringAtohex, TwoVariablesExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 2u);
}

TEST_F(StringAtohex, NoNets) {
  // Per IEEE 1800-2023 Sec 6.7/6.8, neither 'string' nor 'int' has a
  // net-type keyword, so neither 'a' nor 'b' should be materialized as Nets.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "module should have no nets";
}

TEST_F(StringAtohex, AVariableTypespecIsString) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(StringAtohex, AVariableInitialValueIs0xff) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiStringConst);
  EXPECT_EQ(init->getDecompile(), "\"0xff\"");
}

TEST_F(StringAtohex, BVariableTypespecIsInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  EXPECT_NE(b->getTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

// ----
// HierPath -- b's initial value is the method call a.atohex()
// ----
TEST_F(StringAtohex, BVariableHasValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  EXPECT_NE(b->getValue(), nullptr) << "variable 'b' should have a vpiValue set from int b = a.atohex()";
}

TEST_F(StringAtohex, BVariableValueIsNotPreEvaluatedConstant) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getValue<hldb::Constant>(), nullptr)
      << "HLC does not pre-evaluate a.atohex() to a constant; b holds only the HierPath expression";
}

TEST_F(StringAtohex, BVariableValueIsHierPath) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::HierPath *const hp = b->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr) << "variable 'b' initial value is not a HierPath";
  EXPECT_EQ(hp->getName(), "a.atohex");
}

TEST_F(StringAtohex, HierPathReceiverIsA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::HierPath *const hp = b->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 1u);

  const hldb::RefObj *const receiver = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_EQ(receiver->getName(), "a");
  EXPECT_NE(receiver->getActual<hldb::Variable>(), nullptr);
}

TEST_F(StringAtohex, HierPathMethodIsAtohex) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::HierPath *const hp = b->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 2u);

  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "atohex");
}

TEST_F(StringAtohex, AtohexHasNoArguments) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::HierPath *const hp = b->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "atohex() takes no arguments";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
