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

// Validates the HLDB model built for
// tests/Google/generic/iface/iface_class_test_7.sv. Lines 1-5 are the block
// comment carrying the suite's :name:/:description:/:tags: metadata, and
// the rest is:
//
//    6  interface class base_ic;
//    7  typedef struct { int i; bool b; } mystruct;
//    8  typedef enum { RED, GREEN, BLUE } colors;
//    9  typedef virtual blah_if harness_if;
//   10  typedef some_class#(3, 2, 1) car_type;
//   11  endclass
//
// Line 11 has no line terminator; the file ends right after 'endclass'.
// There is no module.
//
// The fixture is tagged 8.3 (class declarations) and 8.26 (interface
// classes).
//
// interface class base_ic; ... endclass (8.26, A.1.9)
//   interface_class_item ::= type_declaration
//     | { attribute_instance } interface_class_method
//     | local_parameter_declaration ; | parameter_declaration ; | ;
//   - An interface class named base_ic, declared once, as a
//     compilation-unit item (3.12.1); user-defined (vpiClassType
//     vpiUserDefinedClass, 37.29) and an interface class (8.26).
//   - No parameter port list, no 'extends', no 'implements' (8.26.2), no
//     end label.
//   - Its four interface_class_items are all type_declarations (6.18),
//     which 8.26 allows, in this order:
//       7  mystruct    an unpacked struct (7.2) with members i (int) and
//                      b (bool), in that order
//       8  colors      an enum (6.19) with constants RED, GREEN, BLUE; with
//                      no explicit values they are 0, 1, 2 ("the first name
//                      ... is assigned 0 ... the next ... the previous value
//                      plus 1", 6.19)
//       9  harness_if  a virtual interface type (25.9) of interface blah_if
//      10  car_type    the class type some_class specialized with the
//                      parameter values 3, 2, 1 (8.25)
//     There are no methods, no properties (an interface class has none,
//     8.26) and no nested classes.
//
// Three of the four typedefs name something that is declared nowhere: the
// file declares no type 'bool' (bool is not a SystemVerilog built-in type;
// the built-in integer types are listed in 6.11), no interface blah_if and
// no class some_class, and the .hlc compiles no other source. Each such
// name resolves to no data type, which is an error:
//   - line 7, member type bool: HLC's diagnostic for a type name that
//     resolves to no type is COMP_UNDEFINED_TYPE ("Undefined type").
//   - line 9, 'virtual blah_if': a virtual interface shall name an
//     interface (25.9); HLC's dedicated diagnostic is
//     COMP_UNDEFINED_INTERFACE ("Undefined interface"); COMP_UNDEFINED_TYPE
//     is also accepted.
//   - line 10, 'some_class#(3, 2, 1)': a specialization shall name a
//     parameterized class (8.25); HLC's dedicated diagnostic is
//     COMP_UNDEFINED_CLASS ("Undefined class"); COMP_UNDEFINED_TYPE is also
//     accepted.
// The syntax of every line is legal, so there is no syntax error. The enum
// typedef of line 8 is fully legal.
//
// The file declares no module, so there is no top-level instance (23.3.1),
// and no other design element or compilation-unit item: the four typedefs
// and the enum constants are members of base_ic, not of the compilation
// unit.
//
// The .hlc also puts UVM 1800.2-2017 on the include path ('-I'), but adds no
// UVM source file, and passes -nobuiltin, so nothing else is compiled.
//
// What is checked
//   - Each undefined name is reported on its own line: bool (line 7),
//     blah_if (line 9), some_class (line 10); at least one error overall.
//   - No fatal and no syntax diagnostic.
//   - Exactly one SourceFile for the .sv, with no includes, no generic
//     directives, no macro definitions and no macro instances.
//   - Exactly one class named base_ic: user-defined, an interface class,
//     with no parameters, base classes, implemented classes, methods,
//     properties, constraints, nested classes or end label.
//   - base_ic declares exactly four typedefs, in source order mystruct,
//     colors, harness_if, car_type.
//   - mystruct aliases an unpacked struct with members i then b, i of type
//     int.
//   - colors aliases an enum with constants RED, GREEN, BLUE, in order;
//     any value HLDB stores for them is 0, 1, 2 respectively.
//   - harness_if aliases a type naming blah_if, bound to no interface
//     definition (and, if modeled as an interface typespec, a virtual one).
//   - car_type aliases a type naming some_class, bound to no class
//     definition.
//   - No interface blah_if and no class some_class exists in the design.
//   - No other design element, no compilation-unit declaration besides
//     base_ic, and no top-level module instance.
//
// KNOWN FAILURE (undefined type names misreported), not a defect in this
// test: HLC 577 models the four typedefs (names, order, struct members,
// enum constants) and leaves bool, blah_if and some_class unresolved, but
// reports only one diagnostic, an HLDB_UNSUPPORTED_TYPESPEC (2029,
// "Unsupported typespec") on line 10 for car_type. It reports nothing for
// bool (line 7) or blah_if (line 9), and nothing names some_class as
// undefined; the construct on line 10 is supported, the class is merely
// undeclared. BoolIsReportedUndefined, BlahIfIsReportedUndefined and
// SomeClassIsReportedUndefined assert the LRM behavior and are expected to
// fail until HLC is fixed; they are intentionally not skipped or relaxed.
//
// What is NOT checked, and why
//   - Whether harness_if is modeled as a virtual interface typespec. HLC
//     577 keeps only an unresolved reference named blah_if (no typespec at
//     all), so the 'virtual' keyword leaves no trace. The representation
//     of a type that failed to resolve is a tool convention; the missing
//     error for it is what BlahIfIsReportedUndefined asserts. Nearest
//     assertion: HarnessIfNamesUnresolvedBlahIf (checks virtual-ness when an
//     interface typespec exists).
//   - The values of RED, GREEN and BLUE when HLDB stores none: HLC does not
//     materialize implicit enum values, which this suite accepts as a model
//     convention (chapter-6 6.19--enum_anon, 6.19.1--enum_typedef). Stored
//     values are asserted. Nearest assertion: ColorsIsEnumRedGreenBlue.
//   - The data type HLDB records for member b. bool resolves to no type, so
//     the representation of the failed member type is a tool convention;
//     the member's name and position are asserted. Nearest assertions:
//     BoolIsReportedUndefined and MystructIsUnpackedStructOfIAndB.
//   - The base type of colors. None is written, so it is int (6.19), but
//     whether HLDB stores an explicit base typespec for the default is a
//     tool convention (see chapter-6 6.19--enum_anon). Nearest assertion:
//     ColorsIsEnumRedGreenBlue (constant names and order).
//   - The parameter values 3, 2, 1 of car_type. some_class is undeclared,
//     so there is no parameter of it that could receive them; how HLDB keeps
//     the arguments of an unresolved specialization is a tool convention.
//     Nearest assertion: CarTypeNamesUnresolvedSomeClass.
//   - The exact spelling of the names of the virtual-interface and class
//     types ("blah_if", "virtual blah_if", "some_class#(3, 2, 1)", ...): a
//     tool convention; each is checked to contain its interface or class
//     name.
//   - How the enum values are encoded (vpiConstType, size): only the value
//     is asserted.
//   - ClassDefn::getVirtual() for base_ic: the 'virtual' keyword is not
//     written; whether the flag reflects abstract-ness is a model
//     convention.
//   - The errors' columns. The LRM numbers lines (22.13) but not columns.
//   - The metadata block comment (5.4) and the missing final newline (end
//     of file is white space, 5.3).
//   - The SourceFile time unit and precision: there is no `timescale and the
//     default is implementation-specific (3.14.2.3).
//   - The warning count. The LRM neither requires nor forbids warnings here.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/checker_decl.h>
#include <hldb/class_defn.h>
#include <hldb/config_decl.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/directive.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/include_stmt.h>
#include <hldb/int_typespec.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/let_decl.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/program.h>
#include <hldb/ref_typespec.h>
#include <hldb/source_file.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task_func.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/udp_defn.h>
#include <hldb/variable.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "iface_class_test_7.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

bool contains(std::string_view text, std::string_view part) { return text.find(part) != std::string_view::npos; }

// Size of an HLDB collection; a collection that was never created is empty.
template <typename T>
size_t sizeOf(const std::vector<T *> *collection) {
  if (collection == nullptr) {
    return 0;
  }
  return collection->size();
}

// Number of objects in 'collection' whose source location is the file under test.
template <typename T>
size_t countFromSourceFile(const std::vector<T *> *collection) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName)) ++count;
    }
  }
  return count;
}

// Number of objects in 'collection' whose name or definition name is 'name'.
template <typename T>
size_t countNamed(const std::vector<T *> *collection, std::string_view name) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if ((object->getName() == name) || (object->getDefName() == name)) ++count;
    }
  }
  return count;
}

// The literal of 'constant' as written: its vpiDecompile text (37.59), or,
// when that is empty, its value text after any "<format>:" prefix.
std::string_view literalOf(const hldb::Constant *constant) {
  const std::string_view decompile = constant->getDecompile();
  if (!decompile.empty()) {
    return decompile;
  }
  const std::string_view value = constant->getValue();
  const size_t colon = value.find(':');
  if (colon == std::string_view::npos) {
    return value;
  }
  return value.substr(colon + 1);
}

// The line a diagnostic points at, or 0 when it carries no location.
uint32_t lineOf(const Error &error) {
  if (error.getLocations().empty()) {
    return 0;
  }
  return error.getLocations().front().m_line;
}

// Every diagnostic of 'severity' as " [<code> line <n>: <text>]", for failure
// messages, so a red run shows what was reported and where. <text> is HLC's
// message template, so its placeholders are not filled in.
std::string describeErrors(const ErrorContainer *errorContainer, ErrorDefinition::ErrorSeverity severity) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  std::string description;
  for (const Error &error : errorContainer->getErrors()) {
    const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
    if ((errorInfo == errorInfoMap.end()) || (errorInfo->second.m_severity != severity)) continue;
    description += " [" + std::to_string(static_cast<int32_t>(error.getType()));
    description += " line " + std::to_string(lineOf(error));
    description += ": " + std::string(errorInfo->second.m_errorText) + "]";
  }
  return description;
}
}  // namespace

class IfaceClassTest7Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "iface_class_test_7.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // True when one of 'types' is reported naming 'symbol' on 'line'.
  bool reportsAnyOf(std::initializer_list<ErrorDefinition::ErrorType> types, std::string_view symbol, uint32_t line) {
    for (ErrorDefinition::ErrorType type : types) {
      if (findError(type, symbol, line) != nullptr) {
        return true;
      }
    }
    return false;
  }

  // The ERROR-severity diagnostics, for failure messages.
  static std::string describeReportedErrors() {
    if ((m_session == nullptr) || (m_session->getErrorContainer() == nullptr)) {
      return "";
    }
    return describeErrors(m_session->getErrorContainer(), ErrorDefinition::ERROR);
  }

  // All SourceFile nodes recorded for the file under test.
  static std::vector<const hldb::SourceFile *> findSourceFiles() {
    std::vector<const hldb::SourceFile *> sourceFiles;
    if (m_design == nullptr) {
      return sourceFiles;
    }
    const hldb::SourceFileCollection *const collection = m_design->getSourceFiles();
    if (collection == nullptr) {
      return sourceFiles;
    }
    for (const hldb::SourceFile *sourceFile : *collection) {
      if (endsWith(sourceFile->getName(), kSourceFileName)) sourceFiles.emplace_back(sourceFile);
    }
    return sourceFiles;
  }

  // The SourceFile node for the file under test, or nullptr unless there is exactly one.
  static const hldb::SourceFile *getSourceFile() {
    const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
    if (sourceFiles.size() != 1) {
      return nullptr;
    }
    return sourceFiles.front();
  }

  // The interface class base_ic, or nullptr.
  static const hldb::ClassDefn *getBaseIc() {
    if (m_design == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::ClassDefn>("base_ic", m_design->getAllClasses());
  }

  // The names of the typedefs declared in base_ic, in collection order.
  static std::vector<std::string_view> getTypedefNames() {
    std::vector<std::string_view> names;
    const hldb::ClassDefn *const baseIc = getBaseIc();
    if ((baseIc == nullptr) || (baseIc->getTypespecs() == nullptr)) {
      return names;
    }
    for (const hldb::Typespec *typespec : *baseIc->getTypespecs()) {
      const hldb::TypedefTypespec *const typedefTypespec = ::any_cast<hldb::TypedefTypespec>(typespec);
      if (typedefTypespec != nullptr) names.emplace_back(typedefTypespec->getName());
    }
    return names;
  }

  // The data type aliased by base_ic's typedef 'name', or nullptr.
  static const hldb::RefTypespec *getAliasOf(std::string_view name) {
    const hldb::ClassDefn *const baseIc = getBaseIc();
    if (baseIc == nullptr) {
      return nullptr;
    }
    const hldb::TypedefTypespec *const typedefTypespec =
        hldb::findByName<hldb::TypedefTypespec>(name, baseIc->getTypespecs());
    if ((typedefTypespec == nullptr) || (typedefTypespec->getTypedef() == nullptr)) {
      return nullptr;
    }
    return typedefTypespec->getTypedef()->getAlias();
  }
};

// Line 7: member b is declared 'bool', which is no built-in type (6.11) and
// is declared nowhere, so it resolves to no data type: an error.
TEST_F(IfaceClassTest7Test, BoolIsReportedUndefined) {
  ASSERT_NE(m_session, nullptr);
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  EXPECT_GE(m_session->getErrorContainer()->getErrorStats().nbError, 1) << "undeclared type names are errors";
  EXPECT_TRUE(reportsAnyOf({ErrorDefinition::COMP_UNDEFINED_TYPE}, "bool", 7))
      << "bool (line 7) resolves to no data type;" << describeReportedErrors();
}

// Line 9: 'virtual blah_if' shall name an interface (25.9); blah_if is
// declared nowhere: an error.
TEST_F(IfaceClassTest7Test, BlahIfIsReportedUndefined) {
  EXPECT_TRUE(
      reportsAnyOf({ErrorDefinition::COMP_UNDEFINED_INTERFACE, ErrorDefinition::COMP_UNDEFINED_TYPE}, "blah_if", 9))
      << "blah_if (line 9) names no interface;" << describeReportedErrors();
}

// Line 10: 'some_class#(3, 2, 1)' shall name a parameterized class (8.25);
// some_class is declared nowhere: an error.
TEST_F(IfaceClassTest7Test, SomeClassIsReportedUndefined) {
  EXPECT_TRUE(
      reportsAnyOf({ErrorDefinition::COMP_UNDEFINED_CLASS, ErrorDefinition::COMP_UNDEFINED_TYPE}, "some_class", 10))
      << "some_class (line 10) names no class;" << describeReportedErrors();
}

// Every line matches the grammar (A.1.9, A.2.1.3, A.2.2.1), so the errors
// above are semantic: nothing is fatal or a syntax error.
TEST_F(IfaceClassTest7Test, ReportsNoFatalOrSyntaxDiagnostics) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  const ErrorContainer::Stats stats = errorContainer->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0) << describeErrors(errorContainer, ErrorDefinition::FATAL);
  EXPECT_EQ(stats.nbSyntax, 0) << describeErrors(errorContainer, ErrorDefinition::SYNTAX);
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_SYNTAX_ERROR), nullptr);
}

TEST_F(IfaceClassTest7Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

// The file holds no compiler directive, no `define and no macro usage.
TEST_F(IfaceClassTest7Test, SourceFileHasNoIncludesDirectivesOrMacros) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getIncludeStmts()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Line 6: 'interface class base_ic;' declares the class once. It is written
// in the source, so it is user-defined (37.29 vpiClassType), and the
// 'interface' keyword makes it an interface class (8.26).
TEST_F(IfaceClassTest7Test, BaseIcIsUserDefinedInterfaceClass) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countNamed(m_design->getAllClasses(), "base_ic"), 1u) << "base_ic is declared exactly once";

  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);
  EXPECT_EQ(baseIc->getClassType(), vpiUserDefinedClass) << "8.3: base_ic is declared in the source text";
  EXPECT_TRUE(baseIc->getIsInterfaceClass()) << "8.26: base_ic is declared 'interface class'";
}

// Line 6 has no parameter port list (8.25), no 'extends' and no
// 'implements' (8.26.2); lines 7-10 are type declarations only, so base_ic
// has no method, property (8.26), constraint or nested class; line 11 is
// 'endclass' without ': base_ic'.
TEST_F(IfaceClassTest7Test, BaseIcHasOnlyTypeDeclarations) {
  const hldb::ClassDefn *const baseIc = getBaseIc();
  ASSERT_NE(baseIc, nullptr);

  EXPECT_EQ(sizeOf(baseIc->getParameters()), 0u) << "8.25: no parameter port list is written";
  EXPECT_EQ(baseIc->getExtends(), nullptr) << "8.26.3: no 'extends' list is written";
  EXPECT_EQ(baseIc->getImplements(), nullptr) << "8.26.2: an interface class implements nothing";
  EXPECT_EQ(sizeOf(baseIc->getMethods()), 0u) << "no pure virtual method is declared";
  EXPECT_EQ(sizeOf(baseIc->getTaskFuncDecls()), 0u) << "no method prototype is declared";
  EXPECT_EQ(sizeOf(baseIc->getVariables()), 0u) << "8.26: an interface class has no properties";
  EXPECT_EQ(sizeOf(baseIc->getConstraints()), 0u) << "no constraint is declared";
  EXPECT_EQ(sizeOf(baseIc->getClassDefns()), 0u) << "no nested class is declared";
  EXPECT_EQ(baseIc->getEndLabelObj(), nullptr) << "'endclass' is written without ': base_ic'";
}

// Lines 7-10 are four type_declarations (6.18), in this order.
TEST_F(IfaceClassTest7Test, BaseIcDeclaresFourTypedefsInOrder) {
  const std::vector<std::string_view> expected = {"mystruct", "colors", "harness_if", "car_type"};
  EXPECT_EQ(getTypedefNames(), expected) << "lines 7-10 declare mystruct, colors, harness_if, car_type";
}

// Line 7: 'struct { int i; bool b; }' is an unpacked struct ('packed' is not
// written, 7.2) whose members are i then b, in declaration order; i is of
// type int (6.11).
TEST_F(IfaceClassTest7Test, MystructIsUnpackedStructOfIAndB) {
  const hldb::RefTypespec *const alias = getAliasOf("mystruct");
  ASSERT_NE(alias, nullptr) << "mystruct is a typedef of base_ic";
  const hldb::StructTypespec *const structTypespec = alias->getActual<hldb::StructTypespec>();
  ASSERT_NE(structTypespec, nullptr) << "7.2: mystruct aliases a struct";
  const hldb::Struct *const structure = structTypespec->getStruct();
  ASSERT_NE(structure, nullptr);
  EXPECT_FALSE(structure->getPacked()) << "7.2: 'packed' is not written";

  const hldb::TypespecMemberCollection *const members = structure->getMembers();
  ASSERT_EQ(sizeOf(members), 2u) << "the struct declares members i and b";
  EXPECT_EQ(members->at(0)->getName(), "i");
  EXPECT_EQ(members->at(1)->getName(), "b");

  const hldb::RefTypespec *const iType = members->at(0)->getTypespec();
  ASSERT_NE(iType, nullptr) << "'int i' has a data type";
  EXPECT_NE(iType->getActual<hldb::IntTypespec>(), nullptr) << "6.11: member i is declared 'int'";
}

// Line 8: 'enum { RED, GREEN, BLUE }' declares three enumeration constants
// in this order. None has an explicit value, so RED is 0, GREEN is 1 and
// BLUE is 2 (6.19).
TEST_F(IfaceClassTest7Test, ColorsIsEnumRedGreenBlue) {
  const hldb::RefTypespec *const alias = getAliasOf("colors");
  ASSERT_NE(alias, nullptr) << "colors is a typedef of base_ic";
  const hldb::EnumTypespec *const enumTypespec = alias->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTypespec, nullptr) << "6.19: colors aliases an enum";
  const hldb::Enum *const enumeration = enumTypespec->getEnum();
  ASSERT_NE(enumeration, nullptr);

  const hldb::EnumConstCollection *const constants = enumeration->getEnumConsts();
  ASSERT_EQ(sizeOf(constants), 3u) << "the enum declares RED, GREEN and BLUE";
  const std::vector<std::string_view> expectedNames = {"RED", "GREEN", "BLUE"};
  const std::vector<std::string_view> expectedValues = {"0", "1", "2"};
  for (size_t index = 0; index < expectedNames.size(); ++index) {
    const hldb::EnumConst *const constant = constants->at(index);
    EXPECT_EQ(constant->getName(), expectedNames[index]) << "constant " << (index + 1) << " in source order";
    // HLC does not materialize implicit enum values (suite convention, see
    // chapter-6 6.19--enum_anon and 6.19.1--enum_typedef); when a value is
    // stored, it shall be the one 6.19 assigns.
    if (constant->getValue() == nullptr) continue;
    const hldb::Constant *const value = constant->getValue<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "a stored value of " << expectedNames[index] << " is a constant";
    EXPECT_EQ(literalOf(value), expectedValues[index])
        << "6.19: " << expectedNames[index] << " is " << expectedValues[index];
  }
}

// Line 9: 'virtual blah_if' is a virtual interface type (25.9) naming
// blah_if. blah_if is declared nowhere, so the type is bound to no
// interface definition. A type that fails to resolve has no settled model;
// when HLDB does build an interface typespec for it, that typespec shall be
// virtual (the keyword is written).
TEST_F(IfaceClassTest7Test, HarnessIfNamesUnresolvedBlahIf) {
  const hldb::RefTypespec *const alias = getAliasOf("harness_if");
  ASSERT_NE(alias, nullptr) << "harness_if is a typedef of base_ic";
  EXPECT_TRUE(contains(alias->getName(), "blah_if"))
      << "harness_if aliases virtual blah_if, got '" << alias->getName() << "'";

  const hldb::InterfaceTypespec *const interfaceTypespec = alias->getActual<hldb::InterfaceTypespec>();
  if (interfaceTypespec != nullptr) {
    EXPECT_TRUE(interfaceTypespec->getVirtual()) << "25.9: the 'virtual' keyword is written";
    EXPECT_EQ(interfaceTypespec->getInterface(), nullptr) << "blah_if is declared nowhere";
  }
  EXPECT_EQ(hldb::getReferenced<hldb::Interface>(alias), nullptr) << "blah_if is declared nowhere";
}

// Line 10: 'some_class#(3, 2, 1)' is a class type naming some_class (8.25).
// some_class is declared nowhere, so the type is bound to no class
// definition.
TEST_F(IfaceClassTest7Test, CarTypeNamesUnresolvedSomeClass) {
  const hldb::RefTypespec *const alias = getAliasOf("car_type");
  ASSERT_NE(alias, nullptr) << "car_type is a typedef of base_ic";
  EXPECT_TRUE(contains(alias->getName(), "some_class"))
      << "car_type aliases some_class#(3, 2, 1), got '" << alias->getName() << "'";
  EXPECT_EQ(hldb::getReferenced<hldb::ClassDefn>(alias), nullptr) << "some_class is declared nowhere";
}

// Nothing in the design declares blah_if or some_class.
TEST_F(IfaceClassTest7Test, BlahIfAndSomeClassAreNotDeclared) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countNamed(m_design->getAllInterfaces(), "blah_if"), 0u) << "blah_if is declared nowhere";
  EXPECT_EQ(countNamed(m_design->getAllClasses(), "some_class"), 0u) << "some_class is declared nowhere";
}

// base_ic is the only declaration in the file (3.2-3.12): one class and no
// other design element.
TEST_F(IfaceClassTest7Test, NoOtherDesignElementsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 1u) << "base_ic is the only class";
  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 0u) << "the file declares no module";
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u) << "the file declares no interface";
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);
}

// No compilation-unit item (3.12.1) other than base_ic is declared: the
// typedefs are members of base_ic, and the file declares no subroutine,
// let, parameter or variable.
TEST_F(IfaceClassTest7Test, NoCompilationUnitDeclarations) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getTaskFuncs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getLetDecls()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
}

// With no module in the file there is nothing to instantiate implicitly as
// a top-level module (23.3.1). This holds before elaboration (no instance
// tree yet) and after it (no module to elaborate), so it is not gated on
// getElaborated().
TEST_F(IfaceClassTest7Test, NoTopLevelInstances) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getTopModules()), 0u);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
