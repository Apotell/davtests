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

// Validates the UHDM graph for a module using string.hextoa():
//   module top();
//     string a;
//     initial
//       a.hextoa(12);
//   endmodule
//
// Checked:
//   - design has module work@top with 1 net (a: string, uninitialized)
//   - net 'a' has no compile-time initial value (hextoa writes at runtime)
//   - work@top has 1 Initial process
//   - Initial stmt is a HierPath named "a.hextoa(12)"
//   - HierPath element[0] is RefObj "a" with vpiActual resolving to Net 'a'
//   - HierPath element[1] is FuncCall "hextoa" with 1 argument (Constant "12")
//   - argument to hextoa is stored as vpiUIntConst (unsized integer literals
//     are unsigned in Surelog — same as established in the itoa test)
//
// Not checked:
//   - hextoa() is void — no return value; there is no net to capture a result
//   - a's value after a.hextoa(12) is set at simulation runtime only

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/func_call.h>
#include <uhdm/hier_path.h>
#include <uhdm/initial.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/string_typespec.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class StringHextoa : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.16.12--string_hextoa.hlc"});

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

TEST_F(StringHextoa, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net — only 'a' (string, uninitialized)
// ---------------------------------------------------------------------------
TEST_F(StringHextoa, OneNetExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(StringHextoa, ANetTypespecIsString) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<uhdm::StringTypespec>(), nullptr);
}

TEST_F(StringHextoa, ANetHasNoInitialValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue<uhdm::Any>(), nullptr);
}

// ---------------------------------------------------------------------------
// Initial process — initial a.hextoa(12)
// ---------------------------------------------------------------------------
TEST_F(StringHextoa, InitialProcessExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(StringHextoa, InitialStmtIsHierPath) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::HierPath *const hp = init->getStmt<uhdm::HierPath>();
  ASSERT_NE(hp, nullptr) << "Initial stmt is not a HierPath";
  EXPECT_EQ(hp->getName(), "a.hextoa(12)");
}

// ---------------------------------------------------------------------------
// HierPath — receiver 'a' and FuncCall 'hextoa' with 1 argument
// ---------------------------------------------------------------------------
TEST_F(StringHextoa, HierPathReceiverIsA) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::HierPath *const hp = init->getStmt<uhdm::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 1u);
  const uhdm::RefObj *const receiver =
      any_cast<uhdm::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_EQ(receiver->getName(), "a");
  EXPECT_NE(receiver->getActual<uhdm::Net>(), nullptr);
}

TEST_F(StringHextoa, HierPathMethodIsHextoa) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::HierPath *const hp = init->getStmt<uhdm::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 2u);
  const uhdm::FuncCall *const call =
      any_cast<uhdm::FuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "hextoa");
}

TEST_F(StringHextoa, HextoaArgumentIs12) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::HierPath *const hp = init->getStmt<uhdm::HierPath>();
  ASSERT_NE(hp, nullptr);
  const uhdm::FuncCall *const call =
      any_cast<uhdm::FuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const uhdm::Constant *const arg =
      any_cast<uhdm::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "hextoa argument is not a Constant";
  EXPECT_EQ(arg->getDecompile(), "12");
}

TEST_F(StringHextoa, HextoaArgumentIs12AsUIntConst) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::HierPath *const hp = init->getStmt<uhdm::HierPath>();
  ASSERT_NE(hp, nullptr);
  const uhdm::FuncCall *const call =
      any_cast<uhdm::FuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  const uhdm::Constant *const arg =
      any_cast<uhdm::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getConstType(), vpiUIntConst)
      << "Surelog stores unsized integer literals as vpiUIntConst, not vpiIntConst";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
