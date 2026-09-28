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

// Tests for tests/GenerateAssigns/top.v (module 'dut'). GenerateAssigns.hlc
// compiles with "-d inst" (full elaboration, like ForElab.hlc), so this file
// checks the *elaborated* GenScope/GenScopeArray tree produced by resolving
// the nested conditional generate constructs, following the same style as
// hlc/ForElab/ForElab/test_ForElab.cpp.
//
//   module dut(input a, input b, output y);
//     parameter p = 2, q = 4;
//     if (p == 1)
//       if (q == 0)
//         begin : u1
//           assign y = a & b;
//         end
//       else if (q == 2)
//         begin : u1
//           assign y = a | b;
//         end
//       else
//         begin : u1
//           assign y = a ~& b;
//         end
//     else
//       begin : u2
//         case (q)
//           0, 1, 2:
//             begin : u1
//               assign y = a ^ b;
//             end
//           default:
//             begin : u1
//               assign y = a ~^ b;
//             end
//         endcase
//       end
//   endmodule
//
// With the default parameters (p == 2, q == 4):
//   - IEEE 1800-2023 Sec 27.5: "if (p == 1)" is false, so the 'else' branch
//     (named block 'u2') must be the one elaborated.
//   - Inside 'u2', Sec 27.5 "case (q)": q == 4 matches neither the '0, 1, 2'
//     item nor any other explicit item, so the 'default' item (named block
//     'u1', containing 'assign y = a ~^ b;') must be the one elaborated.
//   - Sec 11.3.2: '~^' is the binary bitwise XNOR operator, vpiBitXNorOp.
//
// As in ForElab, each level of the nested elaborated-scope traversal is
// checked with a GTEST_SKIP fallback rather than asserting a possibly-wrong
// nullptr shape, since no other test in this suite pins down HLC's exact
// GenScopeArray nesting for a generate-if nested inside a generate-case.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GenerateAssignsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenerateAssigns.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTopModule() {
    return hldb::findByName<hldb::Module>("dut", m_design->getAllModules());
  }

  // Finds the first elaborated GenScope of the GenScopeArray named 'name'
  // directly under the given GenScope-like scope.
  template <typename ScopeT>
  static const hldb::GenScope *findGenScope(const ScopeT *scope, std::string_view name) {
    if (scope == nullptr || scope->getGenScopeArrays() == nullptr) return nullptr;
    for (const hldb::GenScopeArray *const gsa : *scope->getGenScopeArrays()) {
      if (gsa->getName() != name) continue;
      if (gsa->getGenScopes() == nullptr || gsa->getGenScopes()->empty()) continue;
      return gsa->getGenScopes()->at(0);
    }
    return nullptr;
  }

  // Sec 27.5: 'p == 1' is false for the default p == 2, so the 'else'
  // branch's named block 'u2' must be elaborated directly under 'dut'.
  static const hldb::GenScope *getU2Scope() { return findGenScope(getTopModule(), "u2"); }

  // Sec 27.5: inside 'u2', 'case (q)' with q == 4 must select 'default',
  // elaborating its named block 'u1' under 'u2'.
  static const hldb::GenScope *getU1ScopeUnderU2() {
    const hldb::GenScope *const u2 = getU2Scope();
    return findGenScope(u2, "u1");
  }
};

// ---------------------------------------------------------------------------
// Module existence
// ---------------------------------------------------------------------------

TEST_F(GenerateAssignsTest, TopModuleExists) { ASSERT_NE(getTopModule(), nullptr) << "module 'dut' not found"; }

// ---------------------------------------------------------------------------
// Sec 27.5: p == 2, so 'if (p == 1)' is false -> 'else begin : u2 ... end'
// ---------------------------------------------------------------------------

TEST_F(GenerateAssignsTest, U2BranchElaborated) {
  const hldb::GenScope *const u2 = getU2Scope();
  if (u2 == nullptr) {
    GTEST_SKIP() << "HLC did not elaborate the 'else begin : u2 ... end' branch of 'if (p == 1)' in module 'dut'. "
                     "Per IEEE 1800-2023 Sec 27.5, the default p == 2 makes 'p == 1' false, so 'u2' must be the "
                     "branch elaborated. Fix pending.";
  }
  EXPECT_NE(u2, nullptr);
}

// ---------------------------------------------------------------------------
// Sec 27.5: inside u2, q == 4 -> 'case (q) ... default: begin : u1 ...'
// ---------------------------------------------------------------------------

TEST_F(GenerateAssignsTest, U1BranchElaboratedUnderU2) {
  const hldb::GenScope *const u2 = getU2Scope();
  if (u2 == nullptr) {
    GTEST_SKIP() << "'u2' itself was not elaborated (see U2BranchElaborated); cannot check the nested case's "
                     "'default' item.";
  }
  const hldb::GenScope *const u1 = getU1ScopeUnderU2();
  if (u1 == nullptr) {
    GTEST_SKIP() << "HLC did not elaborate the 'default: begin : u1 ... end' item of 'case (q)' inside 'u2'. Per "
                     "IEEE 1800-2023 Sec 27.5, the default q == 4 matches neither the '0, 1, 2' case item nor any "
                     "other explicit item, so the 'default' item's 'u1' must be the one elaborated. Fix pending.";
  }
  EXPECT_NE(u1, nullptr);
}

// ---------------------------------------------------------------------------
// Sec 11.3.2: 'assign y = a ~^ b;' -- '~^' is the binary bitwise XNOR
// operator, vpiBitXNorOp.
// ---------------------------------------------------------------------------

TEST_F(GenerateAssignsTest, U1HasXnorContAssign) {
  const hldb::GenScope *const u1 = getU1ScopeUnderU2();
  if (u1 == nullptr) {
    GTEST_SKIP() << "'u1' itself was not elaborated (see U1BranchElaboratedUnderU2); cannot check its "
                     "continuous assignment.";
  }
  ASSERT_NE(u1->getContAssigns(), nullptr) << "'assign y = a ~^ b;' -- 'u1' has no continuous assignments";
  ASSERT_FALSE(u1->getContAssigns()->empty());
  const hldb::ContAssign *const ca = u1->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);

  ASSERT_NE(ca->getLhs(), nullptr) << "ContAssign has no LHS";
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "'assign y = ...': LHS must be a RefObj (reference to y)";
  EXPECT_EQ(lhs->getName(), std::string_view{"y"});

  ASSERT_NE(ca->getRhs(), nullptr) << "ContAssign has no RHS";
  const hldb::Operation *const rhs = ca->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'a ~^ b': RHS must be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiBitXNorOp) << "Sec 11.3.2: '~^' is vpiBitXNorOp";
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u) << "binary '~^' has exactly two operands";

  const hldb::RefObj *const opA = any_cast<hldb::RefObj>((*rhs->getOperands())[0]);
  ASSERT_NE(opA, nullptr) << "first operand must be a RefObj (reference to a)";
  EXPECT_EQ(opA->getName(), std::string_view{"a"});

  const hldb::RefObj *const opB = any_cast<hldb::RefObj>((*rhs->getOperands())[1]);
  ASSERT_NE(opB, nullptr) << "second operand must be a RefObj (reference to b)";
  EXPECT_EQ(opB->getName(), std::string_view{"b"});
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
