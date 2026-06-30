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
// Checked:
//   - design has module work@top with 3 nets (a: string, b: string, c: int)
//   - net 'a' initial value is "Test" (vpiStringConst); net 'b' is "TEST"
//   - net 'c' typespec resolves to IntTypespec
//   - net 'c' has a non-null initial value (vpiValue is set)
//   - net 'c' initial value is a HierPath named "a.compare(b)"
//   - HierPath element[0] is RefObj "a" with vpiActual resolving to Net 'a'
//   - HierPath element[1] is FuncCall "compare" with 1 argument
//   - compare() argument is RefObj "b" resolving to Net 'b' (not a Constant)
//
// Not checked:
//   - c does NOT get a pre-evaluated constant value — Surelog stores the
//     unevaluated HierPath expression only; runtime comparison result not known

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/func_call.h>
#include <uhdm/hier_path.h>
#include <uhdm/int_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/string_typespec.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class StringCompare : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.16.6--string_compare.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

TEST_F(StringCompare, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net declarations — string 'a', string 'b', int 'c'
// ---------------------------------------------------------------------------
TEST_F(StringCompare, ThreeNetsExist) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 3u);
}

TEST_F(StringCompare, ANetTypespecIsString) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<uhdm::StringTypespec>(), nullptr);
}

TEST_F(StringCompare, ANetInitialValueIsTest) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::Constant *const init = a->getValue<uhdm::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiStringConst);
  EXPECT_EQ(init->getDecompile(), "\"Test\"");
}

TEST_F(StringCompare, BNetTypespecIsString) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const b = uhdm::findByName<uhdm::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_NE(b->getTypespec()->getActual<uhdm::StringTypespec>(), nullptr);
}

TEST_F(StringCompare, BNetInitialValueIsTEST) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const b = uhdm::findByName<uhdm::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const uhdm::Constant *const init = b->getValue<uhdm::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiStringConst);
  EXPECT_EQ(init->getDecompile(), "\"TEST\"");
}

TEST_F(StringCompare, CNetTypespecIsInt) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const c = uhdm::findByName<uhdm::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  EXPECT_NE(c->getTypespec()->getActual<uhdm::IntTypespec>(), nullptr);
}

// ---------------------------------------------------------------------------
// HierPath — c's initial value is the method call a.compare(b)
// ---------------------------------------------------------------------------
TEST_F(StringCompare, CNetHasValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const c = uhdm::findByName<uhdm::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  EXPECT_NE(c->getValue(), nullptr)
      << "net 'c' should have a vpiValue set from int c = a.compare(b)";
}

TEST_F(StringCompare, CNetValueIsNotPreEvaluatedConstant) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const c = uhdm::findByName<uhdm::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue<uhdm::Constant>(), nullptr)
      << "Surelog does not pre-evaluate a.compare(b) to a constant; c holds only the HierPath expression";
}

TEST_F(StringCompare, CNetValueIsHierPath) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const c = uhdm::findByName<uhdm::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  const uhdm::HierPath *const hp = c->getValue<uhdm::HierPath>();
  ASSERT_NE(hp, nullptr) << "net 'c' initial value is not a HierPath";
  EXPECT_EQ(hp->getName(), "a.compare(b)");
}

TEST_F(StringCompare, HierPathReceiverIsA) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const c = uhdm::findByName<uhdm::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  const uhdm::HierPath *const hp = c->getValue<uhdm::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 1u);

  const uhdm::RefObj *const receiver =
      any_cast<uhdm::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_EQ(receiver->getName(), "a");
  EXPECT_NE(receiver->getActual<uhdm::Net>(), nullptr);
}

TEST_F(StringCompare, HierPathMethodIsCompare) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const c = uhdm::findByName<uhdm::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  const uhdm::HierPath *const hp = c->getValue<uhdm::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 2u);

  const uhdm::FuncCall *const call =
      any_cast<uhdm::FuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "compare");
}

TEST_F(StringCompare, CompareArgumentIsRefObjB) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const c = uhdm::findByName<uhdm::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  const uhdm::HierPath *const hp = c->getValue<uhdm::HierPath>();
  ASSERT_NE(hp, nullptr);
  const uhdm::FuncCall *const call =
      any_cast<uhdm::FuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);

  const uhdm::RefObj *const arg =
      any_cast<uhdm::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "compare() argument is not a RefObj";
  EXPECT_EQ(arg->getName(), "b");
  EXPECT_NE(arg->getActual<uhdm::Net>(), nullptr)
      << "compare() argument should resolve to Net b";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
