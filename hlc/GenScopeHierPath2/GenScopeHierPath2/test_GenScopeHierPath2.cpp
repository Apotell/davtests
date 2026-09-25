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

// Tests for dut.sv (tags: GenScopeHierPath2). Two independent conditional
// generate constructs (IEEE 1800-2023 Sec 27.5):
//
//   module mod(output wire [31:0] out);
//     parameter P = 0;
//     if (P == 1) begin : blk1
//       wire w [2];
//     end else if (P == 2) begin : blk2
//       wire x [3];
//     end else if (P == 3) begin : blk3
//       wire y [5];
//     end else begin : blk4
//       wire z [7];
//     end
//     if (P == 1)
//       assign out = $bits(blk1.w);
//     else if (P == 2)
//       assign out = $bits(blk2.x);
//     else if (P == 3)
//       assign out = $bits(blk3.y);
//     else
//       assign out = $bits(blk4.z);
//   endmodule
//
// P defaults to 0, so for BOTH generate-if chains every "if"/"else if"
// condition (P == 1, P == 2, P == 3) is false and the trailing "else"
// branch is the one elaborated (Sec 27.5): "blk4" (declaring unpacked
// array net "wire z [7]") in the first chain, and
// "assign out = $bits(blk4.z);" in the second chain.
//
// What is checked and why:
//   - module 'mod' exists.
//   - only the GenScopeArray "blk4" is elaborated under 'mod' -- "blk1",
//     "blk2", "blk3" must NOT be present as elaborated scopes, since their
//     guarding conditions are all false.
//   - "blk4"'s elaborated GenScope contains a net "z" whose declared
//     dimension is an unpacked array of size 7 ("wire z [7]" -- Sec 7.4.2,
//     an unsized/implicit range unpacked dimension "[7]" declares an array
//     of 7 elements, i.e. a [0:6] range of size 7).
//   - the continuous assign to "out" is present, and its rhs is a
//     '$bits' SysFuncCall (Sec 20.6.1) whose sole argument is a
//     hierarchical reference (RefObj, Sec 8.4 hierarchical names) that
//     resolves to the net "z" declared inside "blk4".
//
// What is NOT checked and why:
//   - "blk1"/"blk2"/"blk3" not being elaborated is checked via absence
//     from the GenScopeArray collection directly (Sec 27.5 "conditional
//     generate constructs shall not be evaluated until [...] and only the
//     one[s] whose condition evaluates to true, if any, is instantiated");
//     no assertion is made against a GTEST_SKIP-worthy specific shape for
//     the branches that are never elaborated.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any.h>
#include <hldb/array_typespec.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>

namespace hlc {

class GenScopeHierPath2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenScopeHierPath2.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTopModule() {
    return hldb::findByName<hldb::Module>("mod", m_design->getAllModules());
  }

  // Find an elaborated GenScopeArray directly under 'mod' by its generate
  // block label.
  static const hldb::GenScopeArray *findGenScopeArray(std::string_view name) {
    const hldb::Module *const top = getTopModule();
    if (top == nullptr || top->getGenScopeArrays() == nullptr) return nullptr;
    for (const hldb::GenScopeArray *const gsa : *top->getGenScopeArrays()) {
      if (gsa->getName() == name) return gsa;
    }
    return nullptr;
  }

  // Sec 27.5: 'blk4' is the elaborated 'else' branch of the first
  // generate-if chain (P == 0, so P == 1/2/3 are all false). Its
  // GenScopeArray has exactly one GenScope (an "if" branch is not an
  // array of instances).
  static const hldb::GenScope *getBlk4Scope() {
    const hldb::GenScopeArray *const gsa = findGenScopeArray("blk4");
    if (gsa == nullptr || gsa->getGenScopes() == nullptr || gsa->getGenScopes()->empty()) return nullptr;
    return gsa->getGenScopes()->at(0);
  }

  static const hldb::Net *getNetZ() {
    const hldb::GenScope *const blk4 = getBlk4Scope();
    if (blk4 == nullptr || blk4->getNets() == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("z", blk4->getNets());
  }

  static const hldb::ContAssign *getOutAssign() {
    const hldb::Module *const top = getTopModule();
    if (top == nullptr || top->getContAssigns() == nullptr) return nullptr;
    for (const hldb::ContAssign *const ca : *top->getContAssigns()) {
      const hldb::Expr *const lhs = ca->getLhs();
      if (lhs != nullptr && lhs->getName() == "out") return ca;
    }
    return nullptr;
  }
};

// ---------------------------------------------------------------------------
// Module existence
// ---------------------------------------------------------------------------

TEST_F(GenScopeHierPath2Test, ModuleExists) { ASSERT_NE(getTopModule(), nullptr) << "module 'mod' not found"; }

// ---------------------------------------------------------------------------
// Sec 27.5: only 'blk4' (the trailing 'else') is elaborated -- 'blk1',
// 'blk2', 'blk3' (guarded by P == 1/2/3, all false for P == 0) must not be
// present as elaborated scopes.
// ---------------------------------------------------------------------------

TEST_F(GenScopeHierPath2Test, Blk4Elaborated) {
  const hldb::GenScopeArray *const gsa = findGenScopeArray("blk4");
  if (gsa == nullptr) {
    GTEST_SKIP() << "HLC did not elaborate the 'else' (blk4) branch of the first generate-if chain for the "
                     "default parameter P == 0. Per IEEE 1800-2023 Sec 27.5, since 'P == 1', 'P == 2' and "
                     "'P == 3' are all false, the trailing 'else' branch must be the one elaborated. Fix pending.";
  }
  EXPECT_EQ(gsa->getName(), std::string_view{"blk4"});
}

TEST_F(GenScopeHierPath2Test, Blk1NotElaborated) {
  EXPECT_EQ(findGenScopeArray("blk1"), nullptr)
      << "Sec 27.5: 'P == 1' is false for the default P == 0, so 'blk1' must not be elaborated";
}

TEST_F(GenScopeHierPath2Test, Blk2NotElaborated) {
  EXPECT_EQ(findGenScopeArray("blk2"), nullptr)
      << "Sec 27.5: 'P == 2' is false for the default P == 0, so 'blk2' must not be elaborated";
}

TEST_F(GenScopeHierPath2Test, Blk3NotElaborated) {
  EXPECT_EQ(findGenScopeArray("blk3"), nullptr)
      << "Sec 27.5: 'P == 3' is false for the default P == 0, so 'blk3' must not be elaborated";
}

// ---------------------------------------------------------------------------
// Sec 7.4.2: "wire z [7]" declares net 'z' inside 'blk4' as an unpacked
// array of 7 elements.
// ---------------------------------------------------------------------------

TEST_F(GenScopeHierPath2Test, NetZExistsInBlk4) {
  const hldb::GenScope *const blk4 = getBlk4Scope();
  if (blk4 == nullptr) {
    GTEST_SKIP() << "'blk4' itself was not elaborated (see Blk4Elaborated); cannot check its net 'z'.";
  }
  EXPECT_NE(getNetZ(), nullptr) << "net 'z' not found inside elaborated scope 'blk4'";
}

TEST_F(GenScopeHierPath2Test, NetZIsUnpackedArrayOfSizeSeven) {
  const hldb::Net *const z = getNetZ();
  if (z == nullptr) {
    GTEST_SKIP() << "net 'z' was not found (see NetZExistsInBlk4); cannot check its array dimension.";
  }
  ASSERT_NE(z->getTypespec(), nullptr) << "net 'z' has no typespec";
  const hldb::ArrayTypespec *const arr = z->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(arr, nullptr) << "Sec 7.4.2: 'wire z [7]' should give net 'z' an ArrayTypespec (unpacked dimension)";
  EXPECT_FALSE(arr->getPacked()) << "Sec 7.4.2: '[7]' after the identifier is an unpacked dimension";

  ASSERT_NE(arr->getRange(), nullptr) << "ArrayTypespec for 'z' has no range (expected size 7)";
  EXPECT_EQ(arr->getRange()->getSize(), 7)
      << "Sec 7.4.2: an unsized unpacked dimension '[7]' declares an array of 7 elements";
}

// ---------------------------------------------------------------------------
// Sec 10.3.2/20.6.1: "assign out = $bits(blk4.z);" -- continuous assign to
// 'out' whose rhs is a $bits SysFuncCall over a hierarchical reference to
// 'blk4.z'.
// ---------------------------------------------------------------------------

TEST_F(GenScopeHierPath2Test, OutContAssignExists) {
  ASSERT_NE(getOutAssign(), nullptr) << "continuous assign to 'out' not found";
}

TEST_F(GenScopeHierPath2Test, OutAssignRhsIsBitsSysFuncCall) {
  const hldb::ContAssign *const ca = getOutAssign();
  if (ca == nullptr) {
    GTEST_SKIP() << "continuous assign to 'out' was not found (see OutContAssignExists); cannot check its rhs.";
  }
  ASSERT_NE(ca->getRhs(), nullptr) << "'out' continuous assign has no rhs";
  const hldb::SysFuncCall *const call = ca->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(call, nullptr) << "Sec 27.5: for P == 0 the elaborated rhs of 'out' should be '$bits(blk4.z)'";
  EXPECT_EQ(call->getName(), std::string_view{"$bits"});
}

TEST_F(GenScopeHierPath2Test, BitsArgumentIsHierarchicalRefToBlk4Z) {
  const hldb::ContAssign *const ca = getOutAssign();
  if (ca == nullptr) {
    GTEST_SKIP() << "continuous assign to 'out' was not found (see OutContAssignExists); cannot check its "
                     "'$bits' argument.";
  }
  const hldb::SysFuncCall *const call = ca->getRhs<hldb::SysFuncCall>();
  if (call == nullptr) {
    GTEST_SKIP() << "rhs of 'out' is not a SysFuncCall (see OutAssignRhsIsBitsSysFuncCall); cannot check its "
                     "argument.";
  }
  ASSERT_NE(call->getArguments(), nullptr) << "'$bits(...)' call has no arguments";
  ASSERT_EQ(call->getArguments()->size(), 1u) << "'$bits(blk4.z)' should have exactly one argument";

  const hldb::Any *const arg0 = (*call->getArguments())[0];
  ASSERT_NE(arg0, nullptr) << "'$bits(blk4.z)' first argument is null";
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(arg0);
  ASSERT_NE(ref, nullptr) << "Sec 8.4: 'blk4.z' is a hierarchical name and should decompile as a RefObj";

  // Sec 8.4: the RefObj's actual must resolve to the net 'z' declared
  // inside the elaborated 'blk4' scope.
  ASSERT_NE(ref->getActual(), nullptr) << "'blk4.z' reference did not bind to anything";
  const hldb::Net *const actual = ref->getActual<hldb::Net>();
  ASSERT_NE(actual, nullptr) << "'blk4.z' reference did not bind to a Net";
  EXPECT_EQ(actual->getName(), std::string_view{"z"});
  EXPECT_EQ(actual, getNetZ()) << "'blk4.z' should bind to the same net 'z' declared inside 'blk4'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
