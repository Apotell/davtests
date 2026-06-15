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

// Validates the UHDM graph for a module using string.icompare():
//   module top();
//     string a = "Test";
//     string b = "TEST";
//     int c = a.icompare(b);
//   endmodule
//
// Checked:
//   - design has module work@top with 3 nets (a: string, b: string, c: int)
//   - net 'a' initial value is "Test" (vpiStringConst); net 'b' is "TEST"
//   - net 'c' typespec resolves to IntTypespec
//   - net 'c' has a non-null initial value (vpiValue is set)
//   - net 'c' initial value is a HierPath named "a.icompare(b)"
//   - HierPath element[1] is FuncCall "icompare" (case-insensitive, not "compare")
//   - icompare() argument is RefObj "b" resolving to Net 'b'
//
// Not checked:
//   - c does NOT get a pre-evaluated constant value — Surelog stores the
//     unevaluated HierPath expression only
//   - icompare returns 0 for equal (case-insensitive "Test"=="TEST") but this
//     is a runtime result invisible to UHDM

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

class StringIcompare : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.16.7--string_icompare.hlc"});

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

TEST_F(StringIcompare, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net declarations — string 'a', string 'b', int 'c'
// ---------------------------------------------------------------------------
TEST_F(StringIcompare, ThreeNetsExist) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 3u);
}

TEST_F(StringIcompare, ANetInitialValueIsTest) {
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

TEST_F(StringIcompare, BNetInitialValueIsTEST) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const b = uhdm::findByName<uhdm::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const uhdm::Constant *const init = b->getValue<uhdm::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "\"TEST\"");
}

TEST_F(StringIcompare, CNetTypespecIsInt) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const c = uhdm::findByName<uhdm::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  EXPECT_NE(c->getTypespec()->getActual<uhdm::IntTypespec>(), nullptr);
}

// ---------------------------------------------------------------------------
// HierPath — c's initial value is the method call a.icompare(b)
// ---------------------------------------------------------------------------
TEST_F(StringIcompare, CNetHasValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const c = uhdm::findByName<uhdm::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  EXPECT_NE(c->getValue(), nullptr)
      << "net 'c' should have a vpiValue set from int c = a.icompare(b)";
}

TEST_F(StringIcompare, CNetValueIsNotPreEvaluatedConstant) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const c = uhdm::findByName<uhdm::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue<uhdm::Constant>(), nullptr)
      << "Surelog does not pre-evaluate a.icompare(b) to a constant; c holds only the HierPath expression";
}

TEST_F(StringIcompare, CNetValueIsHierPath) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const c = uhdm::findByName<uhdm::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  const uhdm::HierPath *const hp = c->getValue<uhdm::HierPath>();
  ASSERT_NE(hp, nullptr) << "net 'c' initial value is not a HierPath";
  EXPECT_EQ(hp->getName(), "a.icompare(b)");
}

TEST_F(StringIcompare, HierPathMethodIsIcompare) {
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
  EXPECT_EQ(call->getName(), "icompare");
}

TEST_F(StringIcompare, IcompareArgumentIsRefObjB) {
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
  ASSERT_NE(arg, nullptr) << "icompare() argument is not a RefObj";
  EXPECT_EQ(arg->getName(), "b");
  EXPECT_NE(arg->getActual<uhdm::Net>(), nullptr);
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
