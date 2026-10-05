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

// Tests for tests/NonAnsiPort/dut.sv (compiled with -fileunit):
//
//   package pkg1;
//       typedef struct packed { logic [7:0] first; } struct1;
//   endpackage
//   package pkg2;
//       typedef struct packed { logic [6:0] second; } struct1;
//   endpackage
//   module dut(var1, var2, var3);
//       typedef struct packed { logic [5:0] third; } struct2;
//       output pkg1::struct1 var1;
//       output pkg2::struct1 var2;
//       output struct2 var3;
//       assign var1.first = 255;
//       assign var2.second = 127;
//       assign var3.third = 63;
//   endmodule;
//
// What to check and why (IEEE 1800-2023):
//   - Sec 23.2.2.1 / 23.2.2.3: "If the direction, port kind, and data type
//     are all omitted for the first port in the port list, then all ports
//     shall be assumed to be non-ANSI style, and port direction and optional
//     type declarations shall be declared after the port list." dut has 3
//     ports var1, var2, var3 (in that order), each declared 'output' in the
//     body (Syntax 23-3 output_declaration).
//   - Sec 26.3: "pkg1::struct1" and "pkg2::struct1" are explicit
//     package-scope references; although both typedefs are named 'struct1',
//     each port's data type must resolve to the typedef of the package
//     named in its declaration -- var1 to pkg1's {logic [7:0] first},
//     var2 to pkg2's {logic [6:0] second}. var3 uses the module-local
//     typedef struct2 {logic [5:0] third} (Sec 6.18).
//   - Sec 7.2.1: packed structures (all three typedefs use 'packed').
//   - Sec 7.2 / 10.3.2: "assign varN.member = ..." continuous assignments
//     whose lvalue is a member select that resolves to the member of the
//     correct structure.
//   - Sec A.1.11 / Syntax: the trailing ';' after endmodule is legal (an
//     empty package item in the compilation unit) so no syntax error.
//
// What is NOT checked and why:
//   - Port kind (net vs variable) for the non-ANSI output ports: the
//     defaulting rules of Sec 23.2.2.3 are stated for ANSI-style lists, and
//     for non-ANSI output_declaration both net_port_type and
//     variable_port_type alternatives apply; the standard text does not
//     unambiguously pin the kind, so it is not asserted.
//   - Timescale warnings: tool-specific.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NonAnsiPortTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NonAnsiPort.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() { return hldb::findByName<hldb::Module>("dut", m_design->getAllModules()); }

  static const hldb::Port *getPort(std::string_view name) {
    const hldb::Module *const dut = getDut();
    if (dut == nullptr || dut->getPorts() == nullptr) return nullptr;
    return hldb::findByName<hldb::Port>(name, dut->getPorts());
  }

  // Follows TypedefTypespec aliases until a StructTypespec is reached.
  static const hldb::Struct *resolveStruct(const hldb::Typespec *ts) {
    for (int i = 0; (ts != nullptr) && (i < 8); ++i) {
      if (const hldb::StructTypespec *const st = any_cast<hldb::StructTypespec>(ts)) return st->getStruct();
      const hldb::TypedefTypespec *const tts = any_cast<hldb::TypedefTypespec>(ts);
      if (tts == nullptr || tts->getTypedef() == nullptr || tts->getTypedef()->getAlias() == nullptr) return nullptr;
      ts = tts->getTypedef()->getAlias()->getActual();
    }
    return nullptr;
  }

  // Returns the name of the nearest enclosing Package or Module of 'any'.
  static std::string_view enclosingScopeName(const hldb::Any *any) {
    for (const hldb::Any *p = any; p != nullptr; p = p->getParent()) {
      if (const hldb::Package *const pkg = any_cast<hldb::Package>(p)) return pkg->getName();
      if (const hldb::Module *const mod = any_cast<hldb::Module>(p)) return mod->getName();
    }
    return std::string_view();
  }

  // Asserts port 'name' is typed by a packed struct with a single logic
  // member 'member' of range [msb:0], declared in scope 'scope'.
  static void checkPortType(std::string_view name, std::string_view scope, std::string_view member,
                            std::string_view msb) {
    const hldb::Port *const p = getPort(name);
    ASSERT_NE(p, nullptr) << name;
    ASSERT_NE(p->getTypespec(), nullptr) << name;
    const hldb::Typespec *const ts = p->getTypespec()->getActual();
    ASSERT_NE(ts, nullptr) << name << ": port type must resolve";
    EXPECT_EQ(enclosingScopeName(ts), scope) << name << ": type must come from '" << scope << "'";
    const hldb::Struct *const s = resolveStruct(ts);
    ASSERT_NE(s, nullptr) << name;
    EXPECT_TRUE(s->getPacked()) << name;
    ASSERT_NE(s->getMembers(), nullptr) << name;
    ASSERT_EQ(s->getMembers()->size(), 1u) << name;
    const hldb::TypespecMember *const m = s->getMembers()->at(0);
    EXPECT_EQ(m->getName(), member) << name;
    ASSERT_NE(m->getTypespec(), nullptr) << name;
    const hldb::LogicTypespec *const lt = m->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << name;
    ASSERT_NE(lt->getRanges(), nullptr) << name;
    ASSERT_EQ(lt->getRanges()->size(), 1u) << name;
    const hldb::Constant *const left = any_cast<hldb::Constant>(lt->getRanges()->at(0)->getLeftExpr());
    ASSERT_NE(left, nullptr) << name;
    EXPECT_EQ(left->getDecompile(), msb) << name;
  }

  // Asserts cont assign 'idx' is "assign <port>.<member> = <value>;".
  static void checkContAssign(size_t idx, std::string_view port, std::string_view member, std::string_view value) {
    const hldb::Module *const dut = getDut();
    ASSERT_NE(dut, nullptr);
    ASSERT_NE(dut->getContAssigns(), nullptr);
    ASSERT_GT(dut->getContAssigns()->size(), idx);
    const hldb::ContAssign *const ca = dut->getContAssigns()->at(idx);
    const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(ca->getLhs());
    ASSERT_NE(lhs, nullptr);
    ASSERT_NE(lhs->getPathElems(), nullptr);
    ASSERT_EQ(lhs->getPathElems()->size(), 2u);
    const hldb::RefObj *const base = any_cast<hldb::RefObj>(lhs->getPathElems()->at(0));
    ASSERT_NE(base, nullptr);
    EXPECT_EQ(base->getName(), port);
    EXPECT_NE(base->getActual(), nullptr) << port << " must bind to its declaration";
    const hldb::RefObj *const mem = any_cast<hldb::RefObj>(lhs->getPathElems()->at(1));
    ASSERT_NE(mem, nullptr);
    EXPECT_EQ(mem->getName(), member);
    ASSERT_NE(mem->getActual(), nullptr) << member << " must bind to the struct member";
    const hldb::TypespecMember *const tm = any_cast<hldb::TypespecMember>(mem->getActual());
    ASSERT_NE(tm, nullptr);
    EXPECT_EQ(tm->getName(), member);
    const hldb::Constant *const rhs = any_cast<hldb::Constant>(ca->getRhs());
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getDecompile(), value);
  }
};

// ---------------------------------------------------------------------------
// Existence
// ---------------------------------------------------------------------------

TEST_F(NonAnsiPortTest, PackagesExist) {
  EXPECT_NE(hldb::findByName<hldb::Package>("pkg1", m_design->getAllPackages()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Package>("pkg2", m_design->getAllPackages()), nullptr);
}

TEST_F(NonAnsiPortTest, EachPackageDeclaresStruct1) {
  for (const char *const name : {"pkg1", "pkg2"}) {
    const hldb::Package *const pkg = hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
    ASSERT_NE(pkg, nullptr) << name;
    ASSERT_NE(pkg->getTypedefs(), nullptr) << name;
    EXPECT_NE(hldb::findByName<hldb::Typedef>("struct1", pkg->getTypedefs()), nullptr) << name;
  }
}

TEST_F(NonAnsiPortTest, ModuleDutExists) { ASSERT_NE(getDut(), nullptr) << "module 'dut' not found"; }

// ---------------------------------------------------------------------------
// Sec 23.2.2.1: non-ANSI port list (var1, var2, var3), all output
// ---------------------------------------------------------------------------

TEST_F(NonAnsiPortTest, DutHasThreePortsInOrder) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getPorts(), nullptr);
  ASSERT_EQ(dut->getPorts()->size(), 3u);
  EXPECT_EQ(dut->getPorts()->at(0)->getName(), std::string_view("var1"));
  EXPECT_EQ(dut->getPorts()->at(1)->getName(), std::string_view("var2"));
  EXPECT_EQ(dut->getPorts()->at(2)->getName(), std::string_view("var3"));
}

TEST_F(NonAnsiPortTest, AllPortsAreOutput) {
  for (const char *const name : {"var1", "var2", "var3"}) {
    const hldb::Port *const p = getPort(name);
    ASSERT_NE(p, nullptr) << name;
    EXPECT_EQ(p->getDirection(), vpiOutput) << name;
  }
}

TEST_F(NonAnsiPortTest, PortsLowConnBindToDeclarations) {
  for (const char *const name : {"var1", "var2", "var3"}) {
    const hldb::Port *const p = getPort(name);
    ASSERT_NE(p, nullptr) << name;
    const hldb::RefObj *const lc = any_cast<hldb::RefObj>(p->getLowConn());
    ASSERT_NE(lc, nullptr) << name;
    EXPECT_EQ(lc->getName(), std::string_view(name));
    EXPECT_NE(lc->getActual(), nullptr) << name << ": port must connect to its internal declaration";
  }
}

// ---------------------------------------------------------------------------
// Sec 26.3: each port type resolves to the right package's struct1
// ---------------------------------------------------------------------------

TEST_F(NonAnsiPortTest, Var1TypeIsPkg1Struct1) { checkPortType("var1", "pkg1", "first", "7"); }

TEST_F(NonAnsiPortTest, Var2TypeIsPkg2Struct1) { checkPortType("var2", "pkg2", "second", "6"); }

TEST_F(NonAnsiPortTest, Var3TypeIsLocalStruct2) { checkPortType("var3", "dut", "third", "5"); }

// ---------------------------------------------------------------------------
// Continuous assignments to members
// ---------------------------------------------------------------------------

TEST_F(NonAnsiPortTest, DutHasThreeContAssigns) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getContAssigns(), nullptr);
  EXPECT_EQ(dut->getContAssigns()->size(), 3u);
}

TEST_F(NonAnsiPortTest, AssignVar1First) { checkContAssign(0, "var1", "first", "255"); }

TEST_F(NonAnsiPortTest, AssignVar2Second) { checkContAssign(1, "var2", "second", "127"); }

TEST_F(NonAnsiPortTest, AssignVar3Third) { checkContAssign(2, "var3", "third", "63"); }

// ---------------------------------------------------------------------------
// Diagnostics: the source is legal
// ---------------------------------------------------------------------------

TEST_F(NonAnsiPortTest, NoBindingOrSyntaxErrors) {
  for (const char *const sym : {"var1", "var2", "var3", "first", "second", "third", "struct1", "struct2"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, sym), nullptr) << sym;
  }
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbFatal, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
