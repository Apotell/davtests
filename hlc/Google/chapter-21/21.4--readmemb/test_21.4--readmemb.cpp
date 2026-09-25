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

// ============================================================================
// SystemVerilog source under test:
//   tests/Google/chapter-21/21.4--readmemb.sv   (:name: readmemb_task,
//   :description: $readmemb test, :tags: 21.4, :type: parsing)
// ----------------------------------------------------------------------------
//   module top();
//
//   logic [31:0] mem0 [1023:0];
//   string fname0 = "test0.mem";
//
//   initial begin
//       $readmemb(fname0, mem0);
//   end
//
//   endmodule
// ============================================================================
//
// IEEE 1800-2023 constructs present in this file, and what each one pins down
// statically (all of this was derived by reading the .sv text above and the
// LRM; nothing here was taken from tool output):
//
//   Sec 6.8 "Variable declarations" / Sec 6.7 "Net declarations" -- "logic"
//       and "string" are data types, neither is a net_type keyword, and
//       "module top();" has an empty port list. Both "mem0" and "fname0" are
//       therefore module-level Variables and neither is a Net.
//   Sec 7.4.1 "Packed arrays" and Sec 7.4.2 "Unpacked arrays" -- "logic
//       [31:0] mem0 [1023:0];" writes dimensions on BOTH sides of the name,
//       and the side decides the kind: "[31:0]" before the name is a PACKED
//       dimension belonging to the element type, while "[1023:0]" after the
//       name is an UNPACKED dimension belonging to the object. This is the
//       classic memory declaration of Sec 21.4, and getting the two halves
//       the right way round is the structural point of the fixture: the
//       object's own typespec is an array with the 1023:0 range, and its
//       ELEMENT type is the 32-bit logic vector.
//   Sec 6.16 "String data type" -- "string" is its own data type, so "fname0"
//       carries a StringTypespec; unlike "mem0" it is written with a
//       declaration assignment, so it is the only variable here whose
//       vpiValue is populated.
//   Sec 5.9 "String literals" -- a string literal is an unsigned integer
//       constant 8 bits wide per character, so "test0.mem" is 9 * 8 = 72
//       bits.
//   Sec 9.2.2 "Initial procedures" / Sec 9.3.1 "Sequential blocks" -- the one
//       "initial begin ... end" gives exactly one Initial process whose body
//       is a Begin. Nothing is declared inside the block, so the Begin owns
//       no variables of its own and both names resolve outward to the module
//       scope. This file writes no "final" procedure.
//   Sec 21.4 "Loading memory array data from a file" -- this is the clause
//       the file is named for. $readmemb is a system TASK, not a function:
//       the clause introduces "$readmemb" and "$readmemh" as tasks and gives
//       them no return value, which puts them on the opposite side of the
//       task/function split from the Sec 21.3 file-I/O routines ($fopen,
//       $fgets, $fread, $fscanf, $ungetc ...) that all return an integer
//       code. It must therefore be a SysTaskCall and never a SysFuncCall.
//       The argument order is FILENAME FIRST, then the memory to load:
//           $readmemb(file_name, memory_name);
//           $readmemb(file_name, memory_name, start_addr);
//           $readmemb(file_name, memory_name, start_addr, finish_addr);
//       Passing exactly two arguments selects the first of those three forms,
//       with neither a start nor a finish address.
//       Two further facts come straight from the source text rather than from
//       the clause in general:
//         * $readmemb opens, reads and closes the file itself. There is no
//           descriptor anywhere in this fixture -- no $fopen, no $fclose, and
//           in fact no assignment statement at all, which is a visible
//           structural difference from every 21.3 sibling.
//         * the file name is passed as the IDENTIFIER "fname0", not as a
//           literal. The LRM's prototypes are written with a quoted name, but
//           any string expression is acceptable; because the source wrote a
//           variable, the argument must survive as a REFERENCE to that
//           declared string and must not be folded into a fresh Constant
//           carrying "test0.mem" from the declaration.
//
// ----------------------------------------------------------------------------
// WHAT IS CHECKED (every assertion below names a concrete value):
//   - module "top" exists, has no Nets, no ports and no continuous
//     assignments, and declares exactly 2 Variables, "mem0" and "fname0",
//     which are distinct objects.
//   - "mem0"'s typespec is an ArrayTypespec whose getArrayType() is
//     vpiStaticArray, carrying a range whose bounds are the Constants "1023"
//     and "0" -- the UNPACKED dimension written after the name -- and whose
//     element typespec is a LogicTypespec carrying exactly one range with
//     bounds "31" and "0" -- the PACKED dimension written before it. "mem0"
//     has no declaration initializer. Note the asymmetry in the object model
//     itself, which mirrors the language: an unpacked array object has one
//     dimension of its own and exposes it as a single getRange(), while a
//     packed type may carry several and exposes getRanges().
//   - "fname0" resolves to a StringTypespec and NOT to an ArrayTypespec, and
//     carries a declaration initializer: a Constant with constType
//     vpiStringConst, value "test0.mem" and size 72 bits.
//   - "top" has exactly one process, and it is an Initial and explicitly not
//     a FinalStmt.
//   - the Initial's body is a Begin that declares no variables of its own and
//     holds exactly ONE statement, which is not an Assignment -- $readmemb
//     needs no descriptor, so this fixture opens nothing and assigns nothing.
//   - that statement is a SysTaskCall named "$readmemb" and explicitly NOT a
//     SysFuncCall, with exactly 2 arguments -- which is what selects the
//     Sec 21.4 two-argument form over the start_addr and finish_addr forms.
//   - arg[0] is a RefObj named "fname0" bound by object identity to the
//     declared string Variable, and explicitly NOT a Constant; arg[1] is a
//     RefObj named "mem0" bound by object identity to the declared memory.
//   - the two actuals resolve to two DIFFERENT Variable objects, and arg[0]
//     resolves to the StringTypespec declaration while arg[1] resolves to the
//     ArrayTypespec one -- so the Sec 21.4 filename-then-memory order is
//     pinned by type as well as by name, and a swap cannot pass.
//   - the compiler reports zero fatal / syntax / error diagnostics: every
//     construct above is legal per the clauses cited, and the source carries
//     no ":should_fail_because:" tag.
//
// ----------------------------------------------------------------------------
// WHAT IS NOT CHECKED, AND WHY (permanently out of scope -- HLC is a static
// compiler/elaborator and never a simulator):
//   - Whether "test0.mem" exists, whether it can be opened, and what data it
//     holds. Reading a file happens while time advances. The nearest real
//     assertion is StringVariableIsInitializedToTest0Mem, which pins the name
//     that would be opened.
//   - The values loaded into "mem0", and which of its 1024 locations get
//     written. Variable::getValue() exposes only a declaration-time
//     initializer, and "logic [31:0] mem0 [1023:0];" has none, so no field
//     anywhere records a post-load value. The nearest real assertion is
//     ReadmembArgumentsAreFilenameThenMemory, which pins which declared
//     object would be loaded.
//   - That the contents of the file are interpreted as BINARY digits, which
//     is the only thing separating $readmemb from $readmemh (Sec 21.4).
//     Interpreting a digit string is work done while reading, i.e. while time
//     advances. What the source DOES determine is which of the two tasks was
//     named, and the name is what selects that interpretation; it is asserted
//     by InitialBodyStatementIsReadmembSysTaskCall.
//   - The address at which loading starts and stops. The source passes
//     neither a start_addr nor a finish_addr, and the defaults Sec 21.4 gives
//     are applied while the load runs. The nearest real assertion is the
//     argument count of exactly 2 in
//     ReadmembArgumentsAreFilenameThenMemory, which is what establishes that
//     no address was supplied.
//   - Whether "fname0" still holds "test0.mem" by the time $readmemb reads
//     it. There is no field that records a later value, and nothing in this
//     file reassigns it in any case. The nearest real assertion is
//     StringVariableIsInitializedToTest0Mem.
//   - The Begin's own name. No ": label" is written, so the block is unnamed;
//     whether the model leaves the name empty or synthesizes an implicit
//     scope name is a tool-internal convention the source does not determine.
//     The scoping fact that matters is asserted instead, by
//     InitialBodyIsBeginWithOneNonAssignmentStatement, which requires the
//     block to own no variables of its own.
//   - The design-level typespec collection's size and which scope owns the
//     shared LogicTypespec / StringTypespec nodes. Typespec sharing is
//     likewise a tool-internal convention that the source text does not
//     determine.
//   - The warning count. Whether a legal file also draws advisory warnings is
//     tool policy, not something the LRM fixes, so only the fatal/syntax/
//     error counts are asserted below.
//
// No getElaborated() gating is used anywhere in this file. That is the
// correct answer here rather than an omission: every dimension bound is
// written as a literal ("31", "0", "1023", "0"), so there is no constant
// expression to reduce, and the fixture has no parameters, no generate
// constructs and no module instantiations. Every value asserted below is
// established by parsing and name resolution alone.
// ============================================================================

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/final_stmt.h>
#include <hldb/initial.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/process_stmt.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ReadmembTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.4--readmemb.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Variable *getVariable(const char *name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, top->getVariables());
  }

  // "initial begin ... end" -- the module's only process.
  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }

  // "$readmemb(fname0, mem0);" -- the block's one statement.
  static const hldb::SysTaskCall *getReadmembCall() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::SysTaskCall>(body->getStmts()->at(0));
  }

  // Both variables are referenced by name from inside the initial block. This
  // checks that a given argument is a reference bound to the one declared
  // Variable of that name, not merely something spelled alike.
  static void checkArgumentRefersTo(const hldb::Any *argument, const char *name, const char *where) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(argument);
    ASSERT_NE(ref, nullptr) << where << " should be a RefObj";
    EXPECT_EQ(ref->getName(), name);
    EXPECT_EQ(ref->getActual<hldb::Variable>(), getVariable(name))
        << where << " must bind to the module-level Variable '" << name << "'";
  }

  // Sec 7.4.2: the unpacked dimension belongs to the object, so it lives on
  // the variable's own array typespec.
  static const hldb::ArrayTypespec *getMemArrayTypespec() {
    const hldb::Variable *const mem0 = getVariable("mem0");
    if (mem0 == nullptr || mem0->getTypespec<hldb::RefTypespec>() == nullptr) return nullptr;
    return mem0->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  }
};

// --- module scope and its two declarations -----------------------------------

TEST_F(ReadmembTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr); }

// Sec 6.7 lists the net_type keywords; neither "logic" nor "string" is one of
// them, and "module top();" has an empty port list, so nothing here declares
// a net or drives one continuously.
TEST_F(ReadmembTest, ModuleHasNoNetsNoPortsAndNoContinuousAssignments) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'logic' and 'string' are not net-type keywords (IEEE 1800-2023 Sec 6.7)";
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty()) << "'module top();' has an empty port list";
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "the file contains no 'assign' statement";
}

TEST_F(ReadmembTest, ModuleHasExactlyTwoDistinctVariablesMem0AndFname0) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr) << "the memory and the file name are both Variables (Sec 6.8)";
  ASSERT_EQ(top->getVariables()->size(), 2u);

  const hldb::Variable *const mem0 = getVariable("mem0");
  const hldb::Variable *const fname0 = getVariable("fname0");
  ASSERT_NE(mem0, nullptr);
  ASSERT_NE(fname0, nullptr);
  EXPECT_NE(mem0, fname0) << "the memory and the file name are two separate declarations and must be two "
                             "separate Variable objects";
}

// THE STRUCTURAL POINT OF THE DECLARATION. Sec 7.4.1 / Sec 7.4.2: the
// dimension written AFTER the name is unpacked and belongs to the object,
// while the one written BEFORE it is packed and belongs to the element type.
// Asserting the bounds on each side separately is what proves the two halves
// were not swapped.
TEST_F(ReadmembTest, Mem0IsAThousandTwentyFourDeepArrayOfThirtyTwoBitLogic) {
  const hldb::ArrayTypespec *const arr = getMemArrayTypespec();
  ASSERT_NE(arr, nullptr) << "Sec 7.4.2: 'mem0 [1023:0]' writes an unpacked dimension, so the object's own "
                             "type is an array";
  EXPECT_EQ(arr->getArrayType(), vpiStaticArray)
      << "the dimension is written with explicit bounds, so this is a fixed-size array, not a dynamic, "
         "queue or associative one";

  const hldb::Range *const unpacked = arr->getRange();
  ASSERT_NE(unpacked, nullptr) << "'[1023:0]' is the unpacked dimension, carried as the array's own range";
  const hldb::Constant *const unpackedLeft = unpacked->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const unpackedRight = unpacked->getRightExpr<hldb::Constant>();
  ASSERT_NE(unpackedLeft, nullptr) << "both unpacked bounds are written as literals";
  ASSERT_NE(unpackedRight, nullptr);
  EXPECT_EQ(unpackedLeft->getDecompile(), "1023");
  EXPECT_EQ(unpackedRight->getDecompile(), "0");

  ASSERT_NE(arr->getElemTypespec(), nullptr) << "the array's element type is the packed logic vector";
  const hldb::LogicTypespec *const elem = arr->getElemTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(elem, nullptr) << "Sec 7.4.1: 'logic [31:0]' before the name is the element type";
  ASSERT_NE(elem->getRanges(), nullptr) << "'[31:0]' is the packed dimension";
  ASSERT_EQ(elem->getRanges()->size(), 1u) << "exactly one packed dimension is written";
  const hldb::Range *const packed = elem->getRanges()->at(0);
  ASSERT_NE(packed, nullptr);
  const hldb::Constant *const packedLeft = packed->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const packedRight = packed->getRightExpr<hldb::Constant>();
  ASSERT_NE(packedLeft, nullptr) << "both packed bounds are written as literals";
  ASSERT_NE(packedRight, nullptr);
  EXPECT_EQ(packedLeft->getDecompile(), "31");
  EXPECT_EQ(packedRight->getDecompile(), "0");

  const hldb::Variable *const mem0 = getVariable("mem0");
  ASSERT_NE(mem0, nullptr);
  EXPECT_EQ(mem0->getValue(), nullptr) << "'logic [31:0] mem0 [1023:0];' is declared with no '=' initializer";
}

// Sec 6.16 for the type, Sec 6.8 for the declaration assignment and Sec 5.9
// for the literal's width. The negative check against ArrayTypespec is the
// mirror image of the memory's own type and proves the two declarations were
// not transposed.
TEST_F(ReadmembTest, StringVariableIsInitializedToTest0Mem) {
  const hldb::Variable *const fname0 = getVariable("fname0");
  ASSERT_NE(fname0, nullptr);
  ASSERT_NE(fname0->getTypespec<hldb::RefTypespec>(), nullptr);

  EXPECT_NE(fname0->getTypespec<hldb::RefTypespec>()->getActual<hldb::StringTypespec>(), nullptr)
      << "Sec 6.16: 'string fname0' must resolve to a StringTypespec";
  EXPECT_EQ(fname0->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>(), nullptr)
      << "'fname0' is the string declaration, not the memory -- the two must not be transposed";

  const hldb::Constant *const init = fname0->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'string fname0 = \"test0.mem\";' carries a declaration-time initializer";
  EXPECT_EQ(init->getConstType(), vpiStringConst);
  EXPECT_EQ(init->getValue(), "test0.mem");
  EXPECT_EQ(init->getSize(), 72) << "Sec 5.9: \"test0.mem\" = 9 chars x 8 bits";
}

// --- the single procedure -----------------------------------------------------

// Sec 9.2.2: one "initial" keyword, one Initial process -- and nothing else.
TEST_F(ReadmembTest, ModuleHasExactlyOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u) << "the file writes exactly one procedure";
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr) << "that procedure is an 'initial'";
  EXPECT_EQ(any_cast<hldb::FinalStmt>(top->getProcesses()->at(0)), nullptr)
      << "no 'final' keyword is written in this file";
}

// Sec 9.3.1 for the block, and Sec 21.4 for the consequence: $readmemb opens
// and closes the file itself, so unlike every 21.3 sibling this fixture has no
// descriptor to assign and therefore no assignment statement at all.
TEST_F(ReadmembTest, InitialBodyIsBeginWithOneNonAssignmentStatement) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr) << "'initial begin ... end' body should be a Begin";
  EXPECT_TRUE(body->getVariables() == nullptr || body->getVariables()->empty())
      << "no block_item_declaration is written inside the begin-end (Sec 9.3.1); both names resolve "
         "outward to the module scope";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u) << "the block holds the single '$readmemb(fname0, mem0);'";
  EXPECT_EQ(any_cast<hldb::Assignment>(body->getStmts()->at(0)), nullptr)
      << "Sec 21.4: $readmemb needs no file descriptor, so this fixture opens nothing and contains no "
         "assignment at all";
}

// --- the $readmemb call --------------------------------------------------------

// Sec 21.4 introduces $readmemb as a system TASK, with no return value -- the
// opposite side of the task/function split from the Sec 21.3 routines, which
// all return an integer code.
TEST_F(ReadmembTest, InitialBodyStatementIsReadmembSysTaskCall) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u);

  EXPECT_EQ(any_cast<hldb::SysFuncCall>(body->getStmts()->at(0)), nullptr)
      << "IEEE 1800-2023 Sec 21.4: $readmemb returns nothing, so it is a system task, not a system "
         "function";
  const hldb::SysTaskCall *const call = getReadmembCall();
  ASSERT_NE(call, nullptr) << "the block's one statement should be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$readmemb")
      << "the name must be '$readmemb', not '$readmemh' -- the two differ only in the radix they read "
         "the file's digits in, and the name is what selects between them";
}

// Sec 21.4 fixes the order as (file_name, memory_name, ...). Exactly two
// arguments selects the shortest of the clause's three forms.
TEST_F(ReadmembTest, ReadmembArgumentsAreFilenameThenMemory) {
  const hldb::SysTaskCall *const call = getReadmembCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u)
      << "'$readmemb(fname0, mem0)' passes the file name and the memory and nothing else, which is the "
         "Sec 21.4 form with neither a start_addr nor a finish_addr";

  // The file name is passed as the identifier "fname0", so it must survive as
  // a reference and must not be folded into a Constant carrying "test0.mem".
  EXPECT_EQ(any_cast<hldb::Constant>(call->getArguments()->at(0)), nullptr)
      << "the source wrote the identifier 'fname0', so arg[0] must be a reference, not a Constant "
         "carrying an inlined copy of its initializer";
  checkArgumentRefersTo(call->getArguments()->at(0), "fname0", "the $readmemb file-name argument");
  checkArgumentRefersTo(call->getArguments()->at(1), "mem0", "the $readmemb memory argument");
}

// Because "fname0" and "mem0" differ in TYPE as well as in name, a swap of the
// file name and the memory is detectable here by the typespec each argument's
// actual resolves to, not only by the name -- so this pins the Sec 21.4 order
// from both directions.
TEST_F(ReadmembTest, ReadmembFilenameAndMemoryAreNotSwapped) {
  const hldb::SysTaskCall *const call = getReadmembCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::RefObj *const first = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  const hldb::RefObj *const second = any_cast<hldb::RefObj>(call->getArguments()->at(1));
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);

  const hldb::Variable *const fileName = first->getActual<hldb::Variable>();
  const hldb::Variable *const memory = second->getActual<hldb::Variable>();
  ASSERT_NE(fileName, nullptr);
  ASSERT_NE(memory, nullptr);
  EXPECT_NE(fileName, memory) << "the file name and the memory are distinct declared variables";

  ASSERT_NE(fileName->getTypespec<hldb::RefTypespec>(), nullptr);
  EXPECT_NE(fileName->getTypespec<hldb::RefTypespec>()->getActual<hldb::StringTypespec>(), nullptr)
      << "Sec 21.4: arg[0] is the file name, so it must resolve to the string declaration";
  ASSERT_NE(memory->getTypespec<hldb::RefTypespec>(), nullptr);
  EXPECT_NE(memory->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>(), nullptr)
      << "Sec 21.4: arg[1] is the memory being loaded, so it must resolve to the array declaration";
}

// --- compiler diagnostics -----------------------------------------------------

// The source carries no ":should_fail_because:" tag and every construct in it
// is legal per the clauses cited above, so nothing may be reported as an
// error. (The warning count is deliberately not asserted -- see the header.)
TEST_F(ReadmembTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
