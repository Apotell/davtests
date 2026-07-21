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

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>

namespace hlc {

class Alloc : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "alloc.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module and net -------------------------------------------------------

TEST_F(Alloc, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(Alloc, ModuleHasOneNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(Alloc, NetNameIsArr) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->at(0)->getName(), "arr");
}

TEST_F(Alloc, NetHasAssociativeArrayTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::ArrayTypespec *const at = rt->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  // vpiArrayType associative = 3
  EXPECT_EQ(at->getArrayType(), 3);
}

TEST_F(Alloc, AssocArrayKeyTypeIsInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::ArrayTypespec *const at = rt->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  const hldb::RefTypespec *const idxRt = at->getIndexTypespec();
  ASSERT_NE(idxRt, nullptr);
  EXPECT_NE(idxRt->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(Alloc, AssocArrayValueTypeIsInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::ArrayTypespec *const at = rt->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  const hldb::RefTypespec *const elemRt = at->getElemTypespec();
  ASSERT_NE(elemRt, nullptr);
  EXPECT_NE(elemRt->getActual<hldb::IntTypespec>(), nullptr);
}

// --- initial process ------------------------------------------------------

TEST_F(Alloc, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  EXPECT_NE(init, nullptr);
}

TEST_F(Alloc, InitialBodyIsBeginWith3Stmts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 3u);
}

// --- $display(":assert: (%d == 0)", arr.size) -----------------------------

TEST_F(Alloc, FirstStmtIsDisplayWithAssertZero) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::SysTaskCall *const sc = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(0));
  ASSERT_NE(sc, nullptr);
  EXPECT_EQ(sc->getName(), "$display");
  ASSERT_NE(sc->getArguments(), nullptr);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(sc->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(), "\":assert: (%d == 0)\"");
}

TEST_F(Alloc, FirstDisplaySecondArgIsArrSizeHierPath) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::SysTaskCall *const sc = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(0));
  ASSERT_NE(sc, nullptr);
  ASSERT_NE(sc->getArguments(), nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(sc->getArguments()->at(1));
  ASSERT_NE(hp, nullptr);
  EXPECT_EQ(hp->getName(), "arr.size");
}

TEST_F(Alloc, ArrSizeHierPathHasTwoElems) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::SysTaskCall *const sc = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(0));
  ASSERT_NE(sc, nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(sc->getArguments()->at(1));
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  EXPECT_EQ(hp->getPathElems()->size(), 2u);
  const hldb::RefObj *const ro0 = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(ro0, nullptr);
  EXPECT_EQ(ro0->getName(), "arr");
  const hldb::MethodFuncCall *const ro1 = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(ro1, nullptr);
  EXPECT_EQ(ro1->getName(), "size");
}

// --- arr[10] = 10 ---------------------------------------------------------

TEST_F(Alloc, SecondStmtIsBlockingAssignment) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(Alloc, AssignmentLhsIsArrAt10) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  const hldb::BitSelect *const bs = assign->getLhs<hldb::BitSelect>();
  ASSERT_NE(bs, nullptr);
  const hldb::RefObj *const prefix = bs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "arr");
  const hldb::Constant *const idx = bs->getIndex<hldb::Constant>();
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->getDecompile(), "10");
}

TEST_F(Alloc, AssignmentRhsIs10) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "10");
}

// --- $display(":assert: (%d == 1)", arr.size) -----------------------------

TEST_F(Alloc, ThirdStmtIsDisplayWithAssertOne) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::SysTaskCall *const sc = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(2));
  ASSERT_NE(sc, nullptr);
  EXPECT_EQ(sc->getName(), "$display");
  ASSERT_NE(sc->getArguments(), nullptr);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(sc->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(), "\":assert: (%d == 1)\"");
}

// --- structural completeness -----------------------------------------------

TEST_F(Alloc, ArrNetHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->at(0)->getValue<hldb::Any>(), nullptr) << "int arr[int] is declared without an initializer";
}

TEST_F(Alloc, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
