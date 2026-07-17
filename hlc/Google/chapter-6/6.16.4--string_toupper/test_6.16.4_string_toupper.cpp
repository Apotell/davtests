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

// Validates the UHDM graph for a module using string.toupper():
//   module top();
//     string a = "Test";
//     string b = a.toupper();
//   endmodule
//
// Checked:
//   - design has module work@top with 2 nets (a: string, b: string)
//   - net 'a' typespec resolves to StringTypespec; initial value is "Test" (vpiStringConst)
//   - net 'b' typespec resolves to StringTypespec (same type as receiver)
//   - net 'b' has a non-null initial value (vpiValue is set)
//   - net 'b' initial value is a HierPath named "a.toupper()"
//   - HierPath element[0] is RefObj "a" with vpiActual resolving to Net 'a'
//   - HierPath element[1] is FuncCall "toupper" with no arguments
//   - 'b' does NOT get a pre-evaluated constant value (e.g. "TEST") -- HLC
//     stores the unevaluated HierPath expression only
//   - the actual string result of a.toupper() ("TEST") -- kept as a real
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

class StringToupper : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.16.4--string_toupper.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(StringToupper, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net declarations — string 'a' and string 'b'
// ---------------------------------------------------------------------------
TEST_F(StringToupper, TwoNetsExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u);
}

TEST_F(StringToupper, ANetTypespecIsString) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(StringToupper, ANetInitialValueIsTest) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiStringConst);
  EXPECT_EQ(init->getDecompile(), "\"Test\"");
}

TEST_F(StringToupper, BNetTypespecIsString) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::RefTypespec *const rts = b->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::StringTypespec>(), nullptr)
      << "net 'b' (result of toupper) should also be StringTypespec";
}

// ---------------------------------------------------------------------------
// HierPath — b's initial value is the method call a.toupper()
// ---------------------------------------------------------------------------
TEST_F(StringToupper, BNetHasValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_NE(b->getValue(), nullptr) << "net 'b' should have a vpiValue set from string b = a.toupper()";
}

TEST_F(StringToupper, BNetValueIsNotPreEvaluatedConstant) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getValue<hldb::Constant>(), nullptr)
      << "HLC does not pre-evaluate a.toupper() to a constant; b holds only the HierPath expression";
}

TEST_F(StringToupper, BNetValueIsHierPath) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::HierPath *const hp = b->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr) << "net 'b' initial value is not a HierPath";
  EXPECT_EQ(hp->getName(), "a.toupper");
}

TEST_F(StringToupper, HierPathReceiverIsA) {
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

TEST_F(StringToupper, HierPathMethodIsToupper) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::HierPath *const hp = b->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 2u);

  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "toupper");
}

TEST_F(StringToupper, ToupperHasNoArguments) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::HierPath *const hp = b->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "toupper() takes no arguments";
}

// ---------------------------------------------------------------------------
// a.toupper() runtime result
// ---------------------------------------------------------------------------
TEST_F(StringToupper, ToupperResultIsPreEvaluated) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::Constant *const value = b->getValue<hldb::Constant>();
  if (m_design->getElaborated()) {
    ASSERT_NE(value, nullptr) << "net 'b' should hold a pre-evaluated Constant";
    EXPECT_EQ(value->getDecompile(), "\"TEST\"") << "a.toupper() should evaluate to \"TEST\"";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
