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

// Validates the UHDM graph for a module using string.atoreal():
//   module top();
//     string a = "4.76";
//     real b = a.atoreal();
//   endmodule
// Per IEEE 1800-2023 6.8/6.16: neither 'string' nor 'real' has an explicit
// net-type keyword, so both 'a' and 'b' are variable_declarations.
//
// Checked:
//   - design has module top with 2 variables (a: string, b: real)
//   - variable 'a' typespec resolves to StringTypespec; initial value is "4.76" (vpiStringConst)
//   - variable 'b' typespec resolves to RealTypespec (not IntTypespec -- key distinction vs atoi)
//   - variable 'b' has a non-null initial value (vpiValue is set)
//   - variable 'b' initial value is a HierPath named "a.atoreal()"
//   - HierPath element[0] is RefObj "a" with vpiActual resolving to Variable 'a'
//   - HierPath element[1] is FuncCall "atoreal" with no arguments
//   - 'b' does NOT get a pre-evaluated constant value (e.g. 4.76) -- HLDB stores
//     the unevaluated HierPath expression only; compile-time evaluation of
//     string method return values is not performed

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/hier_path.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class StringAtoreal : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.16.10--string_atoreal.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(StringAtoreal, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(StringAtoreal, TwoVariablesExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 2u);
}

TEST_F(StringAtoreal, NoNets) {
  // Per IEEE 1800-2023 Sec 6.7/6.8, neither 'string' nor 'real' has a
  // net-type keyword, so neither 'a' nor 'b' should be materialized as Nets.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "module should have no nets";
}

TEST_F(StringAtoreal, AVariableTypespecIsString) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(StringAtoreal, AVariableInitialValueIs4dot76) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiStringConst);
  EXPECT_EQ(init->getDecompile(), "\"4.76\"");
}

TEST_F(StringAtoreal, BVariableTypespecIsReal) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::RefTypespec *const rts = b->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr)
      << "variable 'b' typespec should resolve to RealTypespec (not IntTypespec)";
}

TEST_F(StringAtoreal, BVariableHasValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  EXPECT_NE(b->getValue(), nullptr) << "variable 'b' should have a vpiValue set from real b = a.atoreal()";
}

TEST_F(StringAtoreal, BVariableValueIsNotPreEvaluatedConstant) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getValue<hldb::Constant>(), nullptr)
      << "HLC does not pre-evaluate a.atoreal() to a constant; b holds only the HierPath expression";
}

TEST_F(StringAtoreal, BVariableValueIsHierPath) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::HierPath *const hp = b->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  EXPECT_EQ(hp->getName(), "a.atoreal");
}

TEST_F(StringAtoreal, HierPathReceiverIsA) {
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

TEST_F(StringAtoreal, HierPathMethodIsAtoreal) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::HierPath *const hp = b->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 2u);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "atoreal");
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "atoreal() takes no arguments";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
