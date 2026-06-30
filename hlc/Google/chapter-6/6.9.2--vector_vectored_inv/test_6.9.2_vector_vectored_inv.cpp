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

// Tests for 6.9.2--vector_vectored_inv.sv (tags: 6.9.2)
//   module top();
//     logic vectored [15:0] a = 0;
//     assign a[1] = 1;
//   endmodule
//   should_fail_because: bit selects not permitted on vectored vector nets
//
// Surelog emits 4 PA0207 syntax errors — `vectored` not recognised in
// `logic` context, so the module body cannot be parsed.
//
// Checked:
//   - no module named work@top (parse failed)
//   - design has 2 unnamed Module stubs (error-recovery artifacts), no nets,
//     no processes, no continuous assignments in either stub
//   - design has 5 Typespec nodes: 2 ModuleTypespec, 1 LogicTypespec,
//     1 IntTypespec, 1 ArrayTypespec (static, Range [15:0])
//   - ArrayTypespec range: left=15, right=0
//
// Not checked:
//   - exact PA0207 error count (4)
//   - design name field ("unnamed")

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/array_typespec.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/logic_typespec.h>
#include <uhdm/module.h>
#include <uhdm/range.h>

namespace SURELOG {

class VectorVectoredInv : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.9.2--vector_vectored_inv.hlc"});

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

// --- module-level checks ------------------------------------------------

TEST_F(VectorVectoredInv, NoModuleNamedTop) {
  // Parse failure: no properly named work@top module in the UHDM graph
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  EXPECT_EQ(top, nullptr);
}

TEST_F(VectorVectoredInv, DesignHasTwoUnnamedModuleStubs) {
  // Surelog's error recovery emits 2 partial Module nodes, both unnamed
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 2u);
  for (const uhdm::Module *const mod : *m_design->getAllModules()) {
    EXPECT_TRUE(mod->getName().empty())
        << "Expected unnamed stub but got: " << mod->getName();
  }
}

TEST_F(VectorVectoredInv, NoNetsInAnyModule) {
  // Neither module stub contains nets — `logic vectored [15:0] a` was not
  // lowered to a Net because the module body parse failed
  ASSERT_NE(m_design->getAllModules(), nullptr);
  for (const uhdm::Module *const mod : *m_design->getAllModules()) {
    EXPECT_EQ(mod->getNets(), nullptr)
        << "Unexpected nets in stub module";
  }
}

TEST_F(VectorVectoredInv, NoContAssignsInAnyModule) {
  // `assign a[1] = 1` never reached UHDM (4th PA0207 error aborts parse
  // before the assign keyword is processed)
  ASSERT_NE(m_design->getAllModules(), nullptr);
  for (const uhdm::Module *const mod : *m_design->getAllModules()) {
    EXPECT_EQ(mod->getContAssigns(), nullptr)
        << "Unexpected continuous assignments in stub module";
  }
}

TEST_F(VectorVectoredInv, NoProcessesInAnyModule) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  for (const uhdm::Module *const mod : *m_design->getAllModules()) {
    EXPECT_EQ(mod->getProcesses(), nullptr)
        << "Unexpected processes in stub module";
  }
}

// --- design-level typespec checks ----------------------------------------

TEST_F(VectorVectoredInv, DesignHasOneLogicTypespec) {
  // The `logic` keyword from `logic vectored [15:0] a = 0` was recognised and
  // deposited as a bare LogicTypespec at design scope (no ranges, no name)
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  int count = 0;
  for (const uhdm::Typespec *const ts : *m_design->getTypespecs()) {
    if (any_cast<uhdm::LogicTypespec>(ts) != nullptr) ++count;
  }
  EXPECT_EQ(count, 1);
}

TEST_F(VectorVectoredInv, DesignHasFiveTypespecs) {
  // Error recovery still deposits 5 typespec nodes at design scope:
  // 2 ModuleTypespec, 1 LogicTypespec, 1 IntTypespec, 1 ArrayTypespec
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 5u);
}

TEST_F(VectorVectoredInv, DesignHasOneArrayTypespec) {
  // The [15:0] range was parsed out as an ArrayTypespec even though the
  // surrounding module declaration failed
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  int count = 0;
  for (const uhdm::Typespec *const ts : *m_design->getTypespecs()) {
    if (any_cast<uhdm::ArrayTypespec>(ts) != nullptr) ++count;
  }
  EXPECT_EQ(count, 1);
}

TEST_F(VectorVectoredInv, ArrayTypespecIsStatic) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const uhdm::ArrayTypespec *at = nullptr;
  for (const uhdm::Typespec *const ts : *m_design->getTypespecs()) {
    at = any_cast<uhdm::ArrayTypespec>(ts);
    if (at != nullptr) break;
  }
  ASSERT_NE(at, nullptr);
  // vpiArrayType static = 1
  EXPECT_EQ(at->getArrayType(), 1);
}

TEST_F(VectorVectoredInv, ArrayTypespecHasRange) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const uhdm::ArrayTypespec *at = nullptr;
  for (const uhdm::Typespec *const ts : *m_design->getTypespecs()) {
    at = any_cast<uhdm::ArrayTypespec>(ts);
    if (at != nullptr) break;
  }
  ASSERT_NE(at, nullptr);
  EXPECT_NE(at->getRange(), nullptr);
}

TEST_F(VectorVectoredInv, ArrayTypespecRangeLeftIs15) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const uhdm::ArrayTypespec *at = nullptr;
  for (const uhdm::Typespec *const ts : *m_design->getTypespecs()) {
    at = any_cast<uhdm::ArrayTypespec>(ts);
    if (at != nullptr) break;
  }
  ASSERT_NE(at, nullptr);
  const uhdm::Range *const range = at->getRange();
  ASSERT_NE(range, nullptr);
  const uhdm::Constant *const left =
      range->getLeftExpr<uhdm::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "15");
}

TEST_F(VectorVectoredInv, ArrayTypespecRangeRightIs0) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const uhdm::ArrayTypespec *at = nullptr;
  for (const uhdm::Typespec *const ts : *m_design->getTypespecs()) {
    at = any_cast<uhdm::ArrayTypespec>(ts);
    if (at != nullptr) break;
  }
  ASSERT_NE(at, nullptr);
  const uhdm::Range *const range = at->getRange();
  ASSERT_NE(range, nullptr);
  const uhdm::Constant *const right =
      range->getRightExpr<uhdm::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

}  // namespace SURELOG
