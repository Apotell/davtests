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

// Validates the HLDB model built for tests/ParamScope/dut.sv:
//
//   package foo_pkg;
//     parameter int VALUE_TEMP = '{default: 1};
//   endpackage
//
//   module bottom ();
//     parameter int VALUE = 8;
//   endmodule
//
//   module lower ();
//     import foo_pkg::*;
//     parameter int VALUE = VALUE_TEMP + 2;
//     bottom #(
//       .VALUE(VALUE)
//     ) bottom_u();
//   endmodule
//
//   module upper ();
//     parameter int VALUE = 7;
//     lower lower_u ();
//   endmodule
//
//   module top ();
//     parameter int VALUE = 5;
//     upper upper_u() ;
//   endmodule
//
// The point of the fixture is name resolution of parameters across scopes.
// Every module of the hierarchy top -> upper -> lower -> bottom declares a
// parameter with the same name, VALUE, and 23.9 lists modules under "The
// following elements define a new scope in SystemVerilog". In lower,
// '.VALUE(VALUE)' names bottom's parameter before the parentheses, since
// 23.10.2.2 says "The name of the parameter shall be the name specified in
// the instantiated module", and lower's own parameter inside them, since
// that expression is written in lower's scope, where lower declares VALUE.
// lower's default reaches into
// foo_pkg through a wildcard import; 26.3: "A wildcard import allows all
// identifiers declared within a package to be imported provided the
// identifier is not otherwise defined in the importing scope". The
// regression this file exists to catch is HLC binding a VALUE to the
// parameter of the wrong scope, or letting the values of upper (7) or top
// (5) leak downward into instances they do not override.
//
// Values: VALUE_TEMP is an int, which 6.11 (Table 6-8) makes a 2-state,
// 32-bit signed integer, written as the assignment pattern '{default: 1}.
// 7.4.1 lets an int be treated as a packed array: "Although an integer type
// with a predefined width n is not a packed array, it matches (see 6.22), and
// can be selected from as if it were, a packed array type with a single
// [n-1:0] dimension", and 10.9 shows an assignment pattern on an integer type
// keyed by bit index, "var integer i = '{31:1, 23:1, 15:1, 8:1, default:0};".
// By the default rule of 10.9.1, each element that "is not an array or
// structure type" is set to the value, so '{default: 1} sets all 32 bits of
// the signed int to 1, and VALUE_TEMP = -1. lower's VALUE = VALUE_TEMP + 2 =
// 1, bottom_u's VALUE = 1, upper_u's VALUE keeps its default 7 and top's
// VALUE keeps 5.
//
// What is checked, and why:
//   Package foo_pkg
//     - exactly 1 parameter, VALUE_TEMP, local because "All param_assignments
//       appearing within a generate block, package, or compilation-unit scope
//       shall become localparam declarations" (6.20.1), of the signed type
//       int (6.11)
//     - its unreduced value is an assignment pattern (10.9) with exactly 1
//       item, keyed by default, whose value is the literal 1, at least 32
//       bits wide (5.7.1); the key 'default' binds to no declaration
//   Modules bottom, lower, upper and top
//     - each '()' declares one null port (37.14 detail 10, "module M();"),
//       with port index 0 (37.14 detail 9)
//     - each declares exactly 1 parameter, VALUE, of the signed type int,
//       overridable because none has a parameter port list (6.20.1)
//     - the defaults are 8 (bottom), 7 (upper) and 5 (top), each assigned to
//       the module's own VALUE; lower's is the Operation vpiAddOp (11.4.3)
//       over the RefObj VALUE_TEMP and the Constant 2, in that order, with
//       VALUE_TEMP bound through the import to foo_pkg's parameter (26.3)
//   Module lower
//     - records exactly 1 import, the wildcard import of foo_pkg (26.3)
//     - holds exactly 1 instance, bottom_u of bottom, whose only override is
//       '.VALUE(VALUE)', by name (23.10.2.2); the name before the parentheses
//       is bound to bottom's VALUE and the name inside them to lower's own
//       VALUE, not to the VALUE of bottom, upper or top
//   Modules upper and top
//     - each holds exactly 1 instance, lower_u of lower and upper_u of upper,
//       with no parameter override
//   Instances
//     - bottom_u, lower_u and upper_u write an empty connection list
//   Elaboration
//     - foo_pkg's VALUE_TEMP is -1; top's VALUE is 5; upper_u's VALUE is 7;
//       lower_u's VALUE is 1; bottom_u's VALUE is 1
//     - top is the only top-level instance (23.3.1)
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the reduced values are checked only when the
// design is elaborated, on Design::getTopPackages() and
// Design::getTopModules(); the unreduced values and bindings are asserted on
// the definitions in every run.
//
// Model convention: the key 'default' of the assignment pattern is held as
// the tag of a TaggedPattern, as a RefObj named "default"; the pattern of the
// TaggedPattern is the value the key selects. The tests read the key there.
//
// Model convention: an unelaborated instance refers, through its
// RefTypespec, to a ModuleTypespec that names the instantiated module and
// holds the instance's parameter overrides; a package import is held as an
// ImportTypespec among the importing module's typespecs. The tests read the
// overrides and the import there.
//
// What is NOT checked, and why:
//   - The vpiConstType of the literals 1, 2, 5, 7 and 8. The VPI header in
//     Annex K lists both vpiDecConst ("decimal integer") and vpiIntConst
//     ("integer constant (1364-2001)") and the standard does not say which
//     an unsized decimal literal takes, so only its text and minimum width
//     are asserted.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/import_typespec.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/tagged_pattern.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

class ParamScopeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamScope.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("foo_pkg", m_design->getAllPackages());
  }

  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Parameter *getParam(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, scope->getParameters());
  }

  // The ParamAssign in 'scope' whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr || scope->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
      const hldb::Parameter *const p = pa->getLhs<hldb::Parameter>();
      if ((p != nullptr) && (p->getName() == name)) return pa;
    }
    return nullptr;
  }

  static const hldb::RefInstance *getInstance(std::string_view module, std::string_view name) {
    const hldb::Module *const m = getModule(module);
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>(name, m->getRefInstances());
  }

  // The ModuleTypespec the unelaborated instance refers to.
  static const hldb::ModuleTypespec *getInstanceType(const hldb::RefInstance *inst) {
    if (inst == nullptr || inst->getTypespec() == nullptr) return nullptr;
    return inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  }

  // The elaborated instance at 'path' below the elaborated top.
  static const hldb::Module *getElabInstance(const std::vector<std::string_view> &path) {
    const hldb::Module *m = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    for (std::string_view name : path) {
      if (m == nullptr) return nullptr;
      m = hldb::findByName<hldb::Module>(name, m->getModules());
    }
    return m;
  }

  // Integer value of a Constant, from its decompiled text or from a sized or
  // based literal. The radix HLC picks for a folded value is a tool
  // convention, so every integer form is accepted. A leading '-' is honored.
  static bool getIntValue(const hldb::Constant *c, int64_t *value) {
    std::string text;
    for (char ch : c->getDecompile()) {
      if ((ch != '_') && (ch != ' ')) text.push_back(ch);
    }
    bool negative = false;
    if (!text.empty() && (text[0] == '-')) {
      negative = true;
      text = text.substr(1);
    }
    int base = 10;
    const std::string::size_type tick = text.find('\'');
    if (tick != std::string::npos) {
      std::string::size_type pos = tick + 1;
      if ((pos < text.size()) && ((text[pos] == 's') || (text[pos] == 'S'))) ++pos;
      if (pos >= text.size()) return false;
      switch (text[pos]) {
        case 'h':
        case 'H': base = 16; break;
        case 'd':
        case 'D': base = 10; break;
        case 'o':
        case 'O': base = 8; break;
        case 'b':
        case 'B': base = 2; break;
        default: return false;
      }
      text = text.substr(pos + 1);
    }
    if (text.empty()) return false;
    uint64_t magnitude = 0;
    const char *const last = text.data() + text.size();
    const std::from_chars_result res = std::from_chars(text.data(), last, magnitude, base);
    if ((res.ec != std::errc()) || (res.ptr != last)) return false;
    *value = negative ? -static_cast<int64_t>(magnitude) : static_cast<int64_t>(magnitude);
    return true;
  }

  // Verifies the parameter VALUE of the elaborated 'scope' holds 'expected'
  // as a 32-bit signed int.
  static void ExpectElabValue(const hldb::Scope *scope, std::string_view name, int32_t expected, std::string_view why) {
    ASSERT_NE(scope, nullptr) << "the elaborated scope holding '" << name << "' not found";
    const hldb::ParamAssign *const pa = getParamAssign(scope, name);
    ASSERT_NE(pa, nullptr) << "'" << name << "' has no ParamAssign";
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "6.20.2: '" << name << "' should be reduced to a Constant";
    int64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(v))), expected) << why;
  }
};

// ---------------------------------------------------------------------------
// package foo_pkg
// ---------------------------------------------------------------------------

TEST_F(ParamScopeTest, ValueTempIsLocalSignedInt) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'foo_pkg' not found";
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getParam(pkg, "VALUE_TEMP");
  ASSERT_NE(p, nullptr) << "parameter 'VALUE_TEMP' not found";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.1: a param_assignment in a package becomes a localparam";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = p->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "'VALUE_TEMP' is declared 'int'";
  EXPECT_TRUE(it->getSigned()) << "6.11: int is signed";
}

TEST_F(ParamScopeTest, ValueTempIsDefaultKeyedAssignmentPatternOfOne) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg(), "VALUE_TEMP");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const pattern = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr) << "unreduced, the value is an Operation";
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9: '{...} is an assignment pattern";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 1u) << "'{default: 1}' writes exactly one item";
  const hldb::TaggedPattern *const item = any_cast<hldb::TaggedPattern>(pattern->getOperands()->at(0));
  ASSERT_NE(item, nullptr) << "the item is keyed";
  const hldb::RefObj *const key = item->getTag<hldb::RefObj>();
  ASSERT_NE(key, nullptr) << "10.9: the key is an assignment_pattern_key";
  EXPECT_EQ(key->getName(), "default") << "10.9: assignment_pattern_key ::= simple_type | default";
  EXPECT_EQ(key->getActual(), nullptr) << "'default' is a keyword, not the name of a declaration";
  const hldb::Constant *const one = item->getPattern<hldb::Constant>();
  ASSERT_NE(one, nullptr) << "the value of the default key is a literal";
  EXPECT_EQ(one->getDecompile(), "1");
  EXPECT_GE(one->getSize(), 32) << "5.7.1: an unsized number is at least 32 bits";
}

// ---------------------------------------------------------------------------
// Ports and parameters of the four modules
// ---------------------------------------------------------------------------

TEST_F(ParamScopeTest, EmptyPortListsDeclareOneNullPort) {
  for (std::string_view name : {"bottom", "lower", "upper", "top"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: " << name << "'s '()' declares a null port";
    ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: " << name << "'s '()' declares exactly one null port";
    EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
    EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
    EXPECT_EQ(m->getPorts()->at(0)->getPortIndex(), 0) << "37.14 detail 9: the first port has a port index of zero";
  }
}

TEST_F(ParamScopeTest, EachModuleDeclaresOneOverridableIntValue) {
  for (std::string_view name : {"bottom", "lower", "upper", "top"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getParameters(), nullptr) << name;
    EXPECT_EQ(m->getParameters()->size(), 1u) << name << " declares exactly one parameter";
    const hldb::Parameter *const p = getParam(m, "VALUE");
    ASSERT_NE(p, nullptr) << "parameter 'VALUE' not found in " << name;
    EXPECT_FALSE(p->getLocalParam()) << "6.20.1: " << name << " has no parameter port list, so VALUE is overridable";
    ASSERT_NE(p->getTypespec(), nullptr) << name;
    const hldb::IntTypespec *const it = p->getTypespec()->getActual<hldb::IntTypespec>();
    ASSERT_NE(it, nullptr) << name << "'s VALUE is declared 'int'";
    EXPECT_TRUE(it->getSigned()) << "6.11: int is signed";
  }
}

TEST_F(ParamScopeTest, LiteralDefaultsAreEightSevenAndFive) {
  struct LiteralDefault {
    std::string_view m_module;
    std::string_view m_value;
  };
  const LiteralDefault defaults[] = {{"bottom", "8"}, {"upper", "7"}, {"top", "5"}};
  for (const LiteralDefault &d : defaults) {
    const hldb::ParamAssign *const pa = getParamAssign(getModule(d.m_module), "VALUE");
    ASSERT_NE(pa, nullptr) << d.m_module << "'s VALUE has no ParamAssign";
    ASSERT_NE(pa->getLhs<hldb::RefObj>(), nullptr) << d.m_module;
    EXPECT_EQ(pa->getLhs<hldb::RefObj>()->getActual(), getParam(getModule(d.m_module), "VALUE"))
        << d.m_module << "'s ParamAssign assigns its own VALUE";
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << d.m_module;
    EXPECT_EQ(c->getDecompile(), d.m_value) << d.m_module;
    EXPECT_GE(c->getSize(), 32) << d.m_module << ": 5.7.1, an unsized number is at least 32 bits";
  }
}

TEST_F(ParamScopeTest, LowerDefaultIsValueTempPlusTwo) {
  const hldb::Module *const lower = getModule("lower");
  ASSERT_NE(lower, nullptr) << "module 'lower' not found";
  const hldb::ParamAssign *const pa = getParamAssign(lower, "VALUE");
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getLhs<hldb::RefObj>(), nullptr);
  EXPECT_EQ(pa->getLhs<hldb::RefObj>()->getActual(), getParam(lower, "VALUE"));
  const hldb::Operation *const add = pa->getRhs<hldb::Operation>();
  ASSERT_NE(add, nullptr) << "unreduced, the default is an Operation";
  EXPECT_EQ(add->getOpType(), vpiAddOp) << "11.4.3";
  ASSERT_NE(add->getOperands(), nullptr);
  ASSERT_EQ(add->getOperands()->size(), 2u);
  const hldb::RefObj *const temp = any_cast<hldb::RefObj>(add->getOperands()->at(0));
  ASSERT_NE(temp, nullptr) << "the first operand is the name 'VALUE_TEMP'";
  EXPECT_EQ(temp->getName(), "VALUE_TEMP");
  ASSERT_NE(getParam(getPkg(), "VALUE_TEMP"), nullptr);
  EXPECT_EQ(temp->getActual(), getParam(getPkg(), "VALUE_TEMP")) << "26.3: imported from foo_pkg";
  const hldb::Constant *const two = any_cast<hldb::Constant>(add->getOperands()->at(1));
  ASSERT_NE(two, nullptr) << "the second operand is the literal 2";
  EXPECT_EQ(two->getDecompile(), "2");
  EXPECT_GE(two->getSize(), 32) << "5.7.1: an unsized number is at least 32 bits";
}

// ---------------------------------------------------------------------------
// module lower
// ---------------------------------------------------------------------------

TEST_F(ParamScopeTest, LowerImportsAllOfFooPkg) {
  const hldb::Module *const m = getModule("lower");
  ASSERT_NE(m, nullptr);
  std::vector<const hldb::ImportTypespec *> imports;
  if (m->getTypespecs() != nullptr) {
    for (const hldb::Typespec *const ts : *m->getTypespecs()) {
      if (const hldb::ImportTypespec *const it = any_cast<hldb::ImportTypespec>(ts)) imports.emplace_back(it);
    }
  }
  ASSERT_EQ(imports.size(), 1u) << "lower writes exactly one import";
  EXPECT_EQ(imports[0]->getName(), "foo_pkg");
  ASSERT_NE(imports[0]->getItem(), nullptr);
  EXPECT_EQ(imports[0]->getItem()->getDecompile(), "*") << "26.3: a wildcard import";
}

TEST_F(ParamScopeTest, LowerInstantiatesBottomOnce) {
  const hldb::Module *const lower = getModule("lower");
  ASSERT_NE(lower, nullptr);
  ASSERT_NE(lower->getRefInstances(), nullptr);
  EXPECT_EQ(lower->getRefInstances()->size(), 1u);
  const hldb::ModuleTypespec *const mt = getInstanceType(getInstance("lower", "bottom_u"));
  ASSERT_NE(mt, nullptr) << "instance 'bottom_u' not found";
  ASSERT_NE(getModule("bottom"), nullptr);
  EXPECT_EQ(mt->getModule(), getModule("bottom")) << "bottom_u instantiates bottom";
}

TEST_F(ParamScopeTest, BottomUOverridesValueByName) {
  const hldb::ModuleTypespec *const mt = getInstanceType(getInstance("lower", "bottom_u"));
  ASSERT_NE(mt, nullptr);
  ASSERT_NE(mt->getParamAssigns(), nullptr);
  ASSERT_EQ(mt->getParamAssigns()->size(), 1u) << "'#(.VALUE(VALUE))' writes exactly one override";
  const hldb::ParamAssign *const pa = mt->getParamAssigns()->at(0);
  EXPECT_TRUE(pa->getConnByName()) << "23.10.2.2: '.VALUE(...)' is a named parameter assignment";
  EXPECT_TRUE(pa->getOverridden()) << "23.10.2: bottom_u overrides VALUE";
  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "VALUE");
  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "unreduced, the override is the name 'VALUE'";
  EXPECT_EQ(rhs->getName(), "VALUE");
}

TEST_F(ParamScopeTest, OverrideNameBindsToBottomsValue) {
  const hldb::ModuleTypespec *const mt = getInstanceType(getInstance("lower", "bottom_u"));
  ASSERT_NE(mt, nullptr);
  ASSERT_NE(mt->getParamAssigns(), nullptr);
  ASSERT_EQ(mt->getParamAssigns()->size(), 1u);
  const hldb::RefObj *const lhs = mt->getParamAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  const hldb::Parameter *const bottomValue = getParam(getModule("bottom"), "VALUE");
  ASSERT_NE(bottomValue, nullptr);
  EXPECT_EQ(lhs->getActual(), bottomValue)
      << "23.10.2.2: the name before the parentheses is the instantiated module's parameter";
}

TEST_F(ParamScopeTest, OverrideValueBindsToLowersValue) {
  const hldb::ModuleTypespec *const mt = getInstanceType(getInstance("lower", "bottom_u"));
  ASSERT_NE(mt, nullptr);
  ASSERT_NE(mt->getParamAssigns(), nullptr);
  ASSERT_EQ(mt->getParamAssigns()->size(), 1u);
  const hldb::RefObj *const rhs = mt->getParamAssigns()->at(0)->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  const hldb::Parameter *const lowerValue = getParam(getModule("lower"), "VALUE");
  ASSERT_NE(lowerValue, nullptr);
  EXPECT_EQ(rhs->getActual(), lowerValue) << "the expression inside the parentheses is resolved in lower";
  for (std::string_view other : {"bottom", "upper", "top"}) {
    EXPECT_NE(rhs->getActual(), getParam(getModule(other), "VALUE"))
        << "'VALUE' inside '.VALUE(...)' is not " << other << "'s VALUE";
  }
}

// ---------------------------------------------------------------------------
// modules upper and top
// ---------------------------------------------------------------------------

TEST_F(ParamScopeTest, UpperAndTopInstantiateWithoutOverride) {
  struct Instantiation {
    std::string_view m_module;
    std::string_view m_instance;
    std::string_view m_target;
  };
  const Instantiation insts[] = {{"upper", "lower_u", "lower"}, {"top", "upper_u", "upper"}};
  for (const Instantiation &i : insts) {
    const hldb::Module *const m = getModule(i.m_module);
    ASSERT_NE(m, nullptr) << "module '" << i.m_module << "' not found";
    ASSERT_NE(m->getRefInstances(), nullptr) << i.m_module;
    EXPECT_EQ(m->getRefInstances()->size(), 1u) << i.m_module << " holds exactly one instance";
    const hldb::ModuleTypespec *const mt = getInstanceType(getInstance(i.m_module, i.m_instance));
    ASSERT_NE(mt, nullptr) << "instance '" << i.m_instance << "' not found";
    ASSERT_NE(getModule(i.m_target), nullptr);
    EXPECT_EQ(mt->getModule(), getModule(i.m_target)) << i.m_instance << " instantiates " << i.m_target;
    EXPECT_TRUE(mt->getParamAssigns() == nullptr || mt->getParamAssigns()->empty())
        << i.m_instance << " writes no parameter_value_assignment";
  }
}

TEST_F(ParamScopeTest, InstancesConnectNoPort) {
  struct InstanceName {
    std::string_view m_module;
    std::string_view m_instance;
  };
  const InstanceName insts[] = {{"lower", "bottom_u"}, {"upper", "lower_u"}, {"top", "upper_u"}};
  for (const InstanceName &i : insts) {
    const hldb::RefInstance *const inst = getInstance(i.m_module, i.m_instance);
    ASSERT_NE(inst, nullptr) << "instance '" << i.m_instance << "' not found";
    EXPECT_TRUE(inst->getArguments() == nullptr || inst->getArguments()->empty())
        << "'" << i.m_instance << " ()' writes an empty connection list";
  }
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamScopeTest, ValueTempReducesToMinusOne) {
  if (m_design->getElaborated()) {
    const hldb::Package *const pkg = hldb::findByName<hldb::Package>("foo_pkg", m_design->getTopPackages());
    ExpectElabValue(pkg, "VALUE_TEMP", -1,
                    "10.9, 7.4.1: '{default: 1} sets all 32 bits of the signed int VALUE_TEMP to 1");
  }
}

TEST_F(ParamScopeTest, EachInstanceReceivesItsOwnScopesValue) {
  if (m_design->getElaborated()) {
    ExpectElabValue(getElabInstance({}), "VALUE", 5, "top keeps its default 5");
    ExpectElabValue(getElabInstance({"upper_u"}), "VALUE", 7, "upper_u is not overridden and keeps 7");
    ExpectElabValue(getElabInstance({"upper_u", "lower_u"}), "VALUE", 1,
                    "lower_u is not overridden: VALUE_TEMP + 2 = -1 + 2 = 1");
    ExpectElabValue(getElabInstance({"upper_u", "lower_u", "bottom_u"}), "VALUE", 1,
                    "23.10.2.2: '.VALUE(VALUE)' passes lower's VALUE, 1, not 8, 7 or 5");
  }
}

TEST_F(ParamScopeTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: upper, lower and bottom are instantiated";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "top");
  }
}

TEST_F(ParamScopeTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
