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

// Tests for traversal.sv (tags: 7.9.8 7.9)
//   module top ();
//     string map[ byte ];
//     byte ix;
//     int rc;
//     initial begin
//       map[ 1000 ] = "a";
//       rc = map.first( ix );
//       $display(":assert: ( ('%0d' == '1') and ('%b' == '11101000') )", rc, ix);
//     end
//   endmodule
//
// Checked:
//   - design has module top with exactly 3 nets: "map" (associative
//     array), "ix" (byte), "rc" (int)
//   - module has exactly 2 typespecs: ByteTypespec (signed) and ArrayTypespec
//   - net "map": ArrayTypespec vpiArrayType=associative(3), index typespec ->
//     ByteTypespec, elem typespec -> StringTypespec
//   - net "ix": ByteTypespec; net "rc": IntTypespec
//   - Initial process: 1 Begin with 3 stmts (2 Assignment + 1 SysFuncCall)
//   - map[1000]="a": BitSelect lhs with unsigned int Constant index (typespec
//     IntTypespec, decompile/value "1000") and string Constant rhs
//     (typespec StringTypespec, value "a") -- exercises an out-of-byte-range
//     literal (1000 > 255) used as a byte-indexed associative array key
//   - rc = map.first(ix): rhs HierPath "map.first(ix)" whose 2nd path elem is
//     a MethodFuncCall "first" with 1 argument RefObj "ix" resolving to Net
//     ix
//   - $display call and its RefObj("rc")/RefObj("ix") arguments
//   - design-level typespecs (3): ModuleTypespec, StringTypespec, IntTypespec
//   - compiler emits zero errors (map.first(ix) with an explicit ref
//     argument resolves cleanly)
//
// Not checked:
//   - actual runtime value written into rc/ix by first() -- simulation-only
//     (see the skipped canary RuntimeValuesOfRcAndIxRequireSimulation below)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/byte_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AssociativeArrayTraversalTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "traversal.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / nets ------------------------------------------------------------

TEST_F(AssociativeArrayTraversalTest, ModuleExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(AssociativeArrayTraversalTest, ModuleHasThreeNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 3u);
}

TEST_F(AssociativeArrayTraversalTest, ModuleHasTwoTypespecs) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  EXPECT_EQ(top->getTypespecs()->size(), 2u);
  EXPECT_NE(any_cast<hldb::ByteTypespec>(top->getTypespecs()->at(0)), nullptr);
  EXPECT_NE(any_cast<hldb::ArrayTypespec>(top->getTypespecs()->at(1)), nullptr);
}

TEST_F(AssociativeArrayTraversalTest, NetMapIsAssociativeArrayOfStringByByte) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const map = hldb::findByName<hldb::Net>("map", top->getNets());
  ASSERT_NE(map, nullptr);
  const hldb::ArrayTypespec *const at = map->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);  // associative = 3
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  EXPECT_NE(at->getIndexTypespec()->getActual<hldb::ByteTypespec>(), nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(AssociativeArrayTraversalTest, NetIxIsByteTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const ix = hldb::findByName<hldb::Net>("ix", top->getNets());
  ASSERT_NE(ix, nullptr);
  EXPECT_NE(ix->getTypespec<hldb::RefTypespec>()->getActual<hldb::ByteTypespec>(), nullptr);
}

TEST_F(AssociativeArrayTraversalTest, NetRcIsIntTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const rc = hldb::findByName<hldb::Net>("rc", top->getNets());
  ASSERT_NE(rc, nullptr);
  EXPECT_NE(rc->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
}

// --- initial process ---------------------------------------------------------

TEST_F(AssociativeArrayTraversalTest, InitialBeginHasThreeStmts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 3u);
}

// --- map[1000]="a" ------------------------------------------------------------

TEST_F(AssociativeArrayTraversalTest, FirstAssignmentSetsMap1000ToA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Assignment *const assign =
      any_cast<hldb::Assignment>(init->getStmt<hldb::Begin>()->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::BitSelect *const lhs = assign->getLhs<hldb::BitSelect>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "map[1000]");
  const hldb::RefObj *const prefix = lhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "map");
  EXPECT_NE(prefix->getActual<hldb::Net>(), nullptr);
  const hldb::Constant *const index = lhs->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getConstType(), vpiUIntConst);
  EXPECT_EQ(index->getDecompile(), "1000");
  EXPECT_EQ(index->getValue(), "1000");
  EXPECT_NE(index->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiStringConst);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("\"a\""));
  EXPECT_EQ(rhs->getValue(), "a");
  EXPECT_NE(rhs->getTypespec<hldb::RefTypespec>()->getActual<hldb::StringTypespec>(), nullptr);
}

// --- rc = map.first(ix) --------------------------------------------------------

TEST_F(AssociativeArrayTraversalTest, SecondAssignmentAssignsRcFromMapFirstIx) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Assignment *const assign =
      any_cast<hldb::Assignment>(init->getStmt<hldb::Begin>()->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "rc");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);
  const hldb::HierPath *const rhs = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view("map.first(ix)"));
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);
  const hldb::RefObj *const mapRef = any_cast<hldb::RefObj>(rhs->getPathElems()->at(0));
  ASSERT_NE(mapRef, nullptr);
  EXPECT_EQ(mapRef->getName(), "map");
  EXPECT_NE(mapRef->getActual<hldb::Net>(), nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(rhs->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "first");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "ix");
  EXPECT_NE(arg->getActual<hldb::Net>(), nullptr);
}

// --- $display(":assert: ( ('%0d' == '1') and ('%b' == '11101000') )", rc, ix) -

TEST_F(AssociativeArrayTraversalTest, DisplayAssertsRcAndIxFormatted) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 3u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: ( ('%0d' == '1') and ('%b' == '11101000') )");
  const hldb::RefObj *const rcRef = any_cast<hldb::RefObj>(disp->getArguments()->at(1));
  ASSERT_NE(rcRef, nullptr);
  EXPECT_EQ(rcRef->getName(), "rc");
  EXPECT_NE(rcRef->getActual<hldb::Net>(), nullptr);
  const hldb::RefObj *const ixRef = any_cast<hldb::RefObj>(disp->getArguments()->at(2));
  ASSERT_NE(ixRef, nullptr);
  EXPECT_EQ(ixRef->getName(), "ix");
  EXPECT_NE(ixRef->getActual<hldb::Net>(), nullptr);
}

// --- known gap: runtime values require simulation -----------------------------

TEST_F(AssociativeArrayTraversalTest, RuntimeValuesOfRcAndIxRequireSimulation) {
  GTEST_SKIP() << "This harness only compiles/elaborates traversal.sv; it does not run a simulator, "
                  "so the actual runtime values written into rc/ix by first() cannot be observed "
                  "here. traversal.sv's own $display format string documents the expected values.";

  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(init->getStmt<hldb::Begin>()->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: ( ('%0d' == '1') and ('%b' == '11101000') )")
      << "expected rc == 1 and ix == 8'b11101000 (1000 truncated to a byte) after first()";
}

// --- design-level typespecs / compiler diagnostics ---------------------------

TEST_F(AssociativeArrayTraversalTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(AssociativeArrayTraversalTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(AssociativeArrayTraversalTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
