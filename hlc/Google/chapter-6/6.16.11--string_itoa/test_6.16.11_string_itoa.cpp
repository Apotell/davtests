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

// Validates the UHDM graph for a module using string.itoa():
//   module top();
//     string a;
//     initial
//       a.itoa(12);
//   endmodule
//
// Checked:
//   - design has module work@top with 1 net (a: string, uninitialized)
//   - net 'a' typespec resolves to StringTypespec
//   - net 'a' has no compile-time initial value (itoa writes at runtime)
//   - work@top has 1 Initial process
//   - Initial stmt is a HierPath named "a.itoa(12)"
//   - HierPath element[0] is RefObj "a" with vpiActual resolving to Net 'a'
//   - HierPath element[1] is FuncCall "itoa" with 1 argument
//   - argument to itoa is Constant "12" stored as vpiUIntConst (Surelog stores
//     unsized integer literals as unsigned; vpiIntConst is NOT used here)
//
// Not checked:
//   - itoa() is void — it has no return value; there is no net 'b' to inspect
//   - a's value after a.itoa(12) is not visible in UHDM because it is set
//     at simulation runtime, not at compile time

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class StringItoa : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.16.11--string_itoa.hlc"});

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

TEST_F(StringItoa, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net — only 'a' (string, uninitialized); itoa writes into it
// ---------------------------------------------------------------------------
TEST_F(StringItoa, OneNetExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u) << "only net 'a'; itoa is void";
}

TEST_F(StringItoa, ANetTypespecIsString) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(StringItoa, ANetHasNoInitialValue) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue<hldb::Any>(), nullptr)
      << "net 'a' is declared without an initial value";
}

// ---------------------------------------------------------------------------
// Initial process — initial a.itoa(12)
// ---------------------------------------------------------------------------
TEST_F(StringItoa, InitialProcessExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(StringItoa, InitialStmtIsHierPath) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init =
      dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::HierPath *const hp = init->getStmt<hldb::HierPath>();
  ASSERT_NE(hp, nullptr) << "Initial stmt is not a HierPath";
  EXPECT_EQ(hp->getName(), "a.itoa(12)");
}

// ---------------------------------------------------------------------------
// HierPath — receiver 'a' and FuncCall 'itoa' with 1 argument
// ---------------------------------------------------------------------------
TEST_F(StringItoa, HierPathReceiverIsA) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init =
      dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::HierPath *const hp = init->getStmt<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 1u);
  const hldb::RefObj *const receiver =
      any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_EQ(receiver->getName(), "a");
  EXPECT_NE(receiver->getActual<hldb::Net>(), nullptr);
}

TEST_F(StringItoa, HierPathMethodIsItoa) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init =
      dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::HierPath *const hp = init->getStmt<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 2u);
  const hldb::MethodFuncCall *const call =
      any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "itoa");
}

TEST_F(StringItoa, ItoaArgumentIs12) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init =
      dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::HierPath *const hp = init->getStmt<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  const hldb::MethodFuncCall *const call =
      any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Constant *const arg =
      any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "itoa argument is not a Constant";
  EXPECT_EQ(arg->getDecompile(), "12");
}

TEST_F(StringItoa, ItoaArgumentIs12AsIntegerConst) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init =
      dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::HierPath *const hp = init->getStmt<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  const hldb::MethodFuncCall *const call =
      any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  const hldb::Constant *const arg =
      any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getConstType(), vpiUIntConst)
      << "Surelog stores unsized integer literals as vpiUIntConst, not vpiIntConst";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
