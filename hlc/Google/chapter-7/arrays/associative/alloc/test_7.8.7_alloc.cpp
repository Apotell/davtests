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

// Tests for alloc.sv (tags: 7.8.7 7.8 7.9.1)
//   module top ();
//     int arr [ int ];
//     initial begin
//       $display(":assert: (%d == 0)", arr.size);
//       arr[10] = 10;
//       $display(":assert: (%d == 1)", arr.size);
//     end
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has exactly 1 net: 'arr' (associative ArrayTypespec, idx=IntTypespec, elem=IntTypespec)
//   - 1 Initial process; body is a Begin with 3 stmts
//   - stmt[0]: $display(":assert: (%d == 0)", arr.size) — HierPath with 2 RefObj elems
//   - stmt[1]: blocking Assignment arr[10]=10 (BitSelect lhs, Constant rhs)
//   - stmt[2]: $display(":assert: (%d == 1)", arr.size)
//   - work@top has no continuous assignments
//
// Not checked:
//   - arr.size produces EL0535 errors (illegal implicit net "size") but appears in UHDM as HierPath
//   - 'arr' net has no initial value
//   - runtime behavior (arr.size returns 0 then 1 after allocation)

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/array_typespec.h>
#include <uhdm/assignment.h>
#include <uhdm/begin.h>
#include <uhdm/bit_select.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/hier_path.h>
#include <uhdm/initial.h>
#include <uhdm/int_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/sys_func_call.h>

namespace SURELOG {

class Alloc : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "alloc.hlc"});

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

// --- module and net -------------------------------------------------------

TEST_F(Alloc, ModuleExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(Alloc, ModuleHasOneNet) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(Alloc, NetNameIsArr) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->at(0)->getName(), "arr");
}

TEST_F(Alloc, NetHasAssociativeArrayTypespec) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::RefTypespec *const rt = net->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const uhdm::ArrayTypespec *const at =
      rt->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  // vpiArrayType associative = 3
  EXPECT_EQ(at->getArrayType(), 3);
}

TEST_F(Alloc, AssocArrayKeyTypeIsInt) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::RefTypespec *const rt = net->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const uhdm::ArrayTypespec *const at =
      rt->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  const uhdm::RefTypespec *const idxRt = at->getIndexTypespec();
  ASSERT_NE(idxRt, nullptr);
  EXPECT_NE(idxRt->getActual<uhdm::IntTypespec>(), nullptr);
}

TEST_F(Alloc, AssocArrayValueTypeIsInt) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::RefTypespec *const rt = net->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const uhdm::ArrayTypespec *const at =
      rt->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  const uhdm::RefTypespec *const elemRt = at->getElemTypespec();
  ASSERT_NE(elemRt, nullptr);
  EXPECT_NE(elemRt->getActual<uhdm::IntTypespec>(), nullptr);
}

// --- initial process ------------------------------------------------------

TEST_F(Alloc, ModuleHasOneInitialProcess) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  EXPECT_NE(init, nullptr);
}

TEST_F(Alloc, InitialBodyIsBeginWith3Stmts) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 3u);
}

// --- $display(":assert: (%d == 0)", arr.size) -----------------------------

TEST_F(Alloc, FirstStmtIsDisplayWithAssertZero) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::SysFuncCall *const sc =
      any_cast<uhdm::SysFuncCall>(blk->getStmts()->at(0));
  ASSERT_NE(sc, nullptr);
  EXPECT_EQ(sc->getName(), "$display");
  ASSERT_NE(sc->getArguments(), nullptr);
  const uhdm::Constant *const fmt =
      any_cast<uhdm::Constant>(sc->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(), "\":assert: (%d == 0)\"");
}

TEST_F(Alloc, FirstDisplaySecondArgIsArrSizeHierPath) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::SysFuncCall *const sc =
      any_cast<uhdm::SysFuncCall>(blk->getStmts()->at(0));
  ASSERT_NE(sc, nullptr);
  ASSERT_NE(sc->getArguments(), nullptr);
  const uhdm::HierPath *const hp =
      any_cast<uhdm::HierPath>(sc->getArguments()->at(1));
  ASSERT_NE(hp, nullptr);
  EXPECT_EQ(hp->getName(), "arr.size");
}

TEST_F(Alloc, ArrSizeHierPathHasTwoElems) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::SysFuncCall *const sc =
      any_cast<uhdm::SysFuncCall>(blk->getStmts()->at(0));
  ASSERT_NE(sc, nullptr);
  const uhdm::HierPath *const hp =
      any_cast<uhdm::HierPath>(sc->getArguments()->at(1));
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  EXPECT_EQ(hp->getPathElems()->size(), 2u);
  const uhdm::RefObj *const ro0 =
      any_cast<uhdm::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(ro0, nullptr);
  EXPECT_EQ(ro0->getName(), "arr");
  const uhdm::RefObj *const ro1 =
      any_cast<uhdm::RefObj>(hp->getPathElems()->at(1));
  ASSERT_NE(ro1, nullptr);
  EXPECT_EQ(ro1->getName(), "size");
}

// --- arr[10] = 10 ---------------------------------------------------------

TEST_F(Alloc, SecondStmtIsBlockingAssignment) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(Alloc, AssignmentLhsIsArrAt10) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  const uhdm::BitSelect *const bs = assign->getLhs<uhdm::BitSelect>();
  ASSERT_NE(bs, nullptr);
  const uhdm::RefObj *const prefix = bs->getPrefix<uhdm::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "arr");
  const uhdm::Constant *const idx = bs->getIndex<uhdm::Constant>();
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->getDecompile(), "10");
}

TEST_F(Alloc, AssignmentRhsIs10) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  const uhdm::Constant *const rhs = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "10");
}

// --- $display(":assert: (%d == 1)", arr.size) -----------------------------

TEST_F(Alloc, ThirdStmtIsDisplayWithAssertOne) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      any_cast<uhdm::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::SysFuncCall *const sc =
      any_cast<uhdm::SysFuncCall>(blk->getStmts()->at(2));
  ASSERT_NE(sc, nullptr);
  EXPECT_EQ(sc->getName(), "$display");
  ASSERT_NE(sc->getArguments(), nullptr);
  const uhdm::Constant *const fmt =
      any_cast<uhdm::Constant>(sc->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(), "\":assert: (%d == 1)\"");
}

// --- structural completeness -----------------------------------------------

TEST_F(Alloc, ArrNetHasNoInitialValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->at(0)->getValue<uhdm::Any>(), nullptr)
      << "int arr[int] is declared without an initializer";
}

TEST_F(Alloc, NoContAssigns) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace SURELOG
