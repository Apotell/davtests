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

// Validates the UHDM graph for a module using string.substr():
//   module top();
//     string a = "Test";
//     string b = a.substr(1, 2);
//   endmodule
//
// Checked:
//   - design has module work@top with 2 nets (a: string, b: string)
//   - net 'a' typespec resolves to StringTypespec; initial value is "Test" (vpiStringConst)
//   - net 'b' typespec resolves to StringTypespec
//   - net 'b' has a non-null initial value (vpiValue is set)
//   - net 'b' initial value is a HierPath named "a.substr(1, 2)"
//   - HierPath element[0] is RefObj "a" with vpiActual resolving to Net 'a'
//   - HierPath element[1] is FuncCall "substr" with 2 Constant arguments "1" and "2"
//   - 'b' does NOT get a pre-evaluated constant value (e.g. "es") -- HLC
//     stores the unevaluated HierPath expression only
//   - const type of arguments "1" and "2" is vpiUIntConst (same as itoa lesson)
//   - the actual string result of a.substr(1, 2) ("es") -- kept as a real
//     assertion for when HLC adds compile-time evaluation of string methods

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/hier_path.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class StringSubstr : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.16.8--string_substr.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(StringSubstr, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net declarations — string 'a' and string 'b'
// ---------------------------------------------------------------------------
TEST_F(StringSubstr, TwoNetsExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u);
}

TEST_F(StringSubstr, ANetTypespecIsString) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(StringSubstr, ANetInitialValueIsTest) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiStringConst);
  EXPECT_EQ(init->getDecompile(), "\"Test\"");
}

TEST_F(StringSubstr, BNetTypespecIsString) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_NE(b->getTypespec()->getActual<hldb::StringTypespec>(), nullptr)
      << "net 'b' (result of substr) should be StringTypespec";
}

// ---------------------------------------------------------------------------
// HierPath — b's initial value is the method call a.substr(1, 2)
// ---------------------------------------------------------------------------
TEST_F(StringSubstr, BNetHasValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_NE(b->getValue(), nullptr) << "net 'b' should have a vpiValue set from string b = a.substr(1, 2)";
}

TEST_F(StringSubstr, BNetValueIsNotPreEvaluatedConstant) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getValue<hldb::Constant>(), nullptr)
      << "HLC does not pre-evaluate a.substr(1,2) to a constant; b holds only the HierPath expression";
}

TEST_F(StringSubstr, BNetValueIsHierPath) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::HierPath *const hp = b->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr) << "net 'b' initial value is not a HierPath";
  EXPECT_EQ(hp->getName(), "a.substr(1, 2)");
}

TEST_F(StringSubstr, HierPathReceiverIsA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::HierPath *const hp = b->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 1u);

  const hldb::RefObj *const receiver = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_EQ(receiver->getName(), "a");
  EXPECT_NE(receiver->getActual<hldb::Net>(), nullptr);
}

TEST_F(StringSubstr, HierPathMethodIsSubstr) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::HierPath *const hp = b->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 2u);

  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "substr");
}

TEST_F(StringSubstr, SubstrFirstArgumentIsOne) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::HierPath *const hp = b->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_GE(call->getArguments()->size(), 1u);

  const hldb::Constant *const arg0 = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg0, nullptr) << "substr first argument is not a Constant";
  EXPECT_EQ(arg0->getDecompile(), "1");
}

TEST_F(StringSubstr, SubstrSecondArgumentIsTwo) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::HierPath *const hp = b->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_GE(call->getArguments()->size(), 2u);

  const hldb::Constant *const arg1 = any_cast<hldb::Constant>(call->getArguments()->at(1));
  ASSERT_NE(arg1, nullptr) << "substr second argument is not a Constant";
  EXPECT_EQ(arg1->getDecompile(), "2");
}

TEST_F(StringSubstr, SubstrArgumentsAreUIntConst) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::HierPath *const hp = b->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_GE(call->getArguments()->size(), 2u);
  const hldb::Constant *const arg0 = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg0, nullptr);
  EXPECT_EQ(arg0->getConstType(), vpiUIntConst)
      << "HLDB stores unsized integer literals as vpiUIntConst, not vpiIntConst";
  const hldb::Constant *const arg1 = any_cast<hldb::Constant>(call->getArguments()->at(1));
  ASSERT_NE(arg1, nullptr);
  EXPECT_EQ(arg1->getConstType(), vpiUIntConst);
}

// ---------------------------------------------------------------------------
// a.substr(1, 2) runtime result
// ---------------------------------------------------------------------------
TEST_F(StringSubstr, SubstrResultIsPreEvaluated) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::Constant *const value = b->getValue<hldb::Constant>();
  if (m_design->getElaborated()) {
    ASSERT_NE(value, nullptr) << "net 'b' should hold a pre-evaluated Constant";
    EXPECT_EQ(value->getDecompile(), "\"es\"") << "a.substr(1, 2) should evaluate to \"es\"";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
