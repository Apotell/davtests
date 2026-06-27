#ifndef UHDM_UHDMPRINTER_H
#define UHDM_UHDMPRINTER_H
#pragma once

#include <uhdm/any.h>
#include <uhdm/uhdm_forward_decl.h>
#include <uhdm/vpi_uhdm.h>

#include <uhdm/UhdmListener.h>

#include <ostream>
#include <string>
#include <vector>

#ifndef UHDM_DEFAULT_PRINTER_TYPE
  #define UHDM_DEFAULT_PRINTER_TYPE UhdmYamlPrinter
#endif

namespace uhdm {
class UhdmPrinter {
 protected:
  using Callstack = std::vector<const Any *>;
  using any_set_t = AnySet;
  static bool s_printIds;

 public:
  virtual ~UhdmPrinter() = default;
  UhdmPrinter(const UhdmPrinter &rhs) = delete;
  UhdmPrinter &operator=(const UhdmPrinter &rhs) = delete;

  static void setPrintIds(bool showOrHide) { s_printIds = showOrHide; }

  void printTree(const Any *any, size_t indent = 0);
  void printTree(vpiHandle handle, size_t indent = 0);

  template <typename C>
  std::enable_if_t<std::is_base_of_v<Any, std::remove_pointer_t<typename C::value_type>>> printTree(const C &collection,
                                                                                                    size_t indent = 0);
  template <typename C>
  std::enable_if_t<std::is_convertible_v<typename C::value_type, vpiHandle>> printTree(const C &collection,
                                                                                       size_t indent = 0);

  void printList(const Any *any, size_t indent = 0);
  void printList(vpiHandle handle, size_t indent = 0);

  template <typename C>
  std::enable_if_t<std::is_base_of_v<Any, std::remove_pointer_t<typename C::value_type>>> printList(const C &collection,
                                                                                                    size_t indent = 0);
  template <typename C>
  std::enable_if_t<std::is_convertible_v<typename C::value_type, vpiHandle>> printList(const C &collection,
                                                                                       size_t indent = 0);

 protected:
  virtual std::ostream &beginPrintAny(std::ostream &out, const Any *any, size_t &indent, std::string_view relation,
                                      bool shallowVisit) = 0;
  virtual std::ostream &printIndent(std::ostream &out, size_t indent);
  virtual std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, bool value,
                                      size_t indent) = 0;
  virtual std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, int32_t value,
                                      size_t indent) = 0;
  virtual std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, uint32_t value,
                                      size_t indent) = 0;
  virtual std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name,
                                      std::string_view value, size_t indent) = 0;
  virtual std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name,
                                      const s_vpi_value *value, size_t indent) = 0;
  virtual std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, s_vpi_delay *delay,
                                      size_t indent) = 0;
  virtual std::ostream &endPrintAny(std::ostream &out, const Any *any, size_t &indent, std::string_view relation,
                                    bool shallowVisit) = 0;

  template <typename T>
  std::ostream &printProperty(std::string_view name, T value, size_t indent) = delete;

  void setPrintId(bool showOrHide) { m_printIds = showOrHide; }

  explicit UhdmPrinter(std::ostream &out) : m_out(out) {}
  static std::string getRelationName(int32_t relation, int32_t value);

 private:
  // clang-format off
  void visitAny(std::ostream &out, const Any *any, size_t indent);
  void visitAttribute(std::ostream &out, const Attribute *any, int32_t indent);
  void visitIdentifier(std::ostream &out, const Identifier *any, int32_t indent);
  void visitComment(std::ostream &out, const Comment *any, int32_t indent);
  void visitLetDecl(std::ostream &out, const LetDecl *any, int32_t indent);
  void visitConcurrentAssertions(std::ostream &out, const ConcurrentAssertions *any, int32_t indent);
  void visitProcess(std::ostream &out, const Process *any, int32_t indent);
  void visitAlways(std::ostream &out, const Always *any, int32_t indent);
  void visitFinalStmt(std::ostream &out, const FinalStmt *any, int32_t indent);
  void visitInitial(std::ostream &out, const Initial *any, int32_t indent);
  void visitAtomicStmt(std::ostream &out, const AtomicStmt *any, int32_t indent);
  void visitDelayControl(std::ostream &out, const DelayControl *any, int32_t indent);
  void visitDelayTerm(std::ostream &out, const DelayTerm *any, int32_t indent);
  void visitEventControl(std::ostream &out, const EventControl *any, int32_t indent);
  void visitRepeatControl(std::ostream &out, const RepeatControl *any, int32_t indent);
  void visitScope(std::ostream &out, const Scope *any, int32_t indent);
  void visitBegin(std::ostream &out, const Begin *any, int32_t indent);
  void visitForkStmt(std::ostream &out, const ForkStmt *any, int32_t indent);
  void visitForStmt(std::ostream &out, const ForStmt *any, int32_t indent);
  void visitIfStmt(std::ostream &out, const IfStmt *any, int32_t indent);
  void visitEventStmt(std::ostream &out, const EventStmt *any, int32_t indent);
  void visitThread(std::ostream &out, const Thread *any, int32_t indent);
  void visitForeverStmt(std::ostream &out, const ForeverStmt *any, int32_t indent);
  void visitWaits(std::ostream &out, const Waits *any, int32_t indent);
  void visitWaitStmt(std::ostream &out, const WaitStmt *any, int32_t indent);
  void visitWaitFork(std::ostream &out, const WaitFork *any, int32_t indent);
  void visitOrderedWait(std::ostream &out, const OrderedWait *any, int32_t indent);
  void visitDisables(std::ostream &out, const Disables *any, int32_t indent);
  void visitDisable(std::ostream &out, const Disable *any, int32_t indent);
  void visitDisableFork(std::ostream &out, const DisableFork *any, int32_t indent);
  void visitContinueStmt(std::ostream &out, const ContinueStmt *any, int32_t indent);
  void visitBreakStmt(std::ostream &out, const BreakStmt *any, int32_t indent);
  void visitReturnStmt(std::ostream &out, const ReturnStmt *any, int32_t indent);
  void visitWhileStmt(std::ostream &out, const WhileStmt *any, int32_t indent);
  void visitRepeat(std::ostream &out, const Repeat *any, int32_t indent);
  void visitDoWhile(std::ostream &out, const DoWhile *any, int32_t indent);
  void visitIfElse(std::ostream &out, const IfElse *any, int32_t indent);
  void visitCaseStmt(std::ostream &out, const CaseStmt *any, int32_t indent);
  void visitForce(std::ostream &out, const Force *any, int32_t indent);
  void visitAssignStmt(std::ostream &out, const AssignStmt *any, int32_t indent);
  void visitDeassign(std::ostream &out, const Deassign *any, int32_t indent);
  void visitRelease(std::ostream &out, const Release *any, int32_t indent);
  void visitNullStmt(std::ostream &out, const NullStmt *any, int32_t indent);
  void visitExpectStmt(std::ostream &out, const ExpectStmt *any, int32_t indent);
  void visitForeachStmt(std::ostream &out, const ForeachStmt *any, int32_t indent);
  void visitGenScope(std::ostream &out, const GenScope *any, int32_t indent);
  void visitGenScopeArray(std::ostream &out, const GenScopeArray *any, int32_t indent);
  void visitAssert(std::ostream &out, const Assert *any, int32_t indent);
  void visitCover(std::ostream &out, const Cover *any, int32_t indent);
  void visitAssume(std::ostream &out, const Assume *any, int32_t indent);
  void visitRestrict(std::ostream &out, const Restrict *any, int32_t indent);
  void visitImmediateAssert(std::ostream &out, const ImmediateAssert *any, int32_t indent);
  void visitImmediateAssume(std::ostream &out, const ImmediateAssume *any, int32_t indent);
  void visitImmediateCover(std::ostream &out, const ImmediateCover *any, int32_t indent);
  void visitExpr(std::ostream &out, const Expr *any, int32_t indent);
  void visitCaseItem(std::ostream &out, const CaseItem *any, int32_t indent);
  void visitAssignment(std::ostream &out, const Assignment *any, int32_t indent);
  void visitAnyPattern(std::ostream &out, const AnyPattern *any, int32_t indent);
  void visitTaggedPattern(std::ostream &out, const TaggedPattern *any, int32_t indent);
  void visitStructPattern(std::ostream &out, const StructPattern *any, int32_t indent);
  void visitUnsupportedExpr(std::ostream &out, const UnsupportedExpr *any, int32_t indent);
  void visitUnsupportedStmt(std::ostream &out, const UnsupportedStmt *any, int32_t indent);
  void visitPreprocMacroDefinition(std::ostream &out, const PreprocMacroDefinition *any, int32_t indent);
  void visitPreprocMacroInstance(std::ostream &out, const PreprocMacroInstance *any, int32_t indent);
  void visitSourceFile(std::ostream &out, const SourceFile *any, int32_t indent);
  void visitSequenceInst(std::ostream &out, const SequenceInst *any, int32_t indent);
  void visitSeqFormalDecl(std::ostream &out, const SeqFormalDecl *any, int32_t indent);
  void visitSequenceDecl(std::ostream &out, const SequenceDecl *any, int32_t indent);
  void visitPropFormalDecl(std::ostream &out, const PropFormalDecl *any, int32_t indent);
  void visitPropertyInst(std::ostream &out, const PropertyInst *any, int32_t indent);
  void visitPropertySpec(std::ostream &out, const PropertySpec *any, int32_t indent);
  void visitPropertyDecl(std::ostream &out, const PropertyDecl *any, int32_t indent);
  void visitClockedProperty(std::ostream &out, const ClockedProperty *any, int32_t indent);
  void visitCasePropertyItem(std::ostream &out, const CasePropertyItem *any, int32_t indent);
  void visitCaseProperty(std::ostream &out, const CaseProperty *any, int32_t indent);
  void visitMulticlockSequenceExpr(std::ostream &out, const MulticlockSequenceExpr *any, int32_t indent);
  void visitClockedSeq(std::ostream &out, const ClockedSeq *any, int32_t indent);
  void visitSimpleExpr(std::ostream &out, const SimpleExpr *any, int32_t indent);
  void visitConstant(std::ostream &out, const Constant *any, int32_t indent);
  void visitLetExpr(std::ostream &out, const LetExpr *any, int32_t indent);
  void visitOperation(std::ostream &out, const Operation *any, int32_t indent);
  void visitRefObj(std::ostream &out, const RefObj *any, int32_t indent);
  void visitRefInstance(std::ostream &out, const RefInstance *any, int32_t indent);
  void visitRefTypespec(std::ostream &out, const RefTypespec *any, int32_t indent);
  void visitSelect(std::ostream &out, const Select *any, int32_t indent);
  void visitPartSelect(std::ostream &out, const PartSelect *any, int32_t indent);
  void visitIndexedPartSelect(std::ostream &out, const IndexedPartSelect *any, int32_t indent);
  void visitVarSelect(std::ostream &out, const VarSelect *any, int32_t indent);
  void visitBitSelect(std::ostream &out, const BitSelect *any, int32_t indent);
  void visitVariable(std::ostream &out, const Variable *any, int32_t indent);
  void visitHierPath(std::ostream &out, const HierPath *any, int32_t indent);
  void visitArrayExpr(std::ostream &out, const ArrayExpr *any, int32_t indent);
  void visitRegArray(std::ostream &out, const RegArray *any, int32_t indent);
  void visitReg(std::ostream &out, const Reg *any, int32_t indent);
  void visitTaskFunc(std::ostream &out, const TaskFunc *any, int32_t indent);
  void visitTask(std::ostream &out, const Task *any, int32_t indent);
  void visitFunction(std::ostream &out, const Function *any, int32_t indent);
  void visitTaskFuncDecl(std::ostream &out, const TaskFuncDecl *any, int32_t indent);
  void visitTaskDecl(std::ostream &out, const TaskDecl *any, int32_t indent);
  void visitFunctionDecl(std::ostream &out, const FunctionDecl *any, int32_t indent);
  void visitModport(std::ostream &out, const Modport *any, int32_t indent);
  void visitInterfaceTFDecl(std::ostream &out, const InterfaceTFDecl *any, int32_t indent);
  void visitContAssign(std::ostream &out, const ContAssign *any, int32_t indent);
  void visitContAssignBit(std::ostream &out, const ContAssignBit *any, int32_t indent);
  void visitPorts(std::ostream &out, const Ports *any, int32_t indent);
  void visitPort(std::ostream &out, const Port *any, int32_t indent);
  void visitPortBit(std::ostream &out, const PortBit *any, int32_t indent);
  void visitCheckerPort(std::ostream &out, const CheckerPort *any, int32_t indent);
  void visitCheckerInstPort(std::ostream &out, const CheckerInstPort *any, int32_t indent);
  void visitPrimitive(std::ostream &out, const Primitive *any, int32_t indent);
  void visitGate(std::ostream &out, const Gate *any, int32_t indent);
  void visitSwitchTran(std::ostream &out, const SwitchTran *any, int32_t indent);
  void visitUdp(std::ostream &out, const Udp *any, int32_t indent);
  void visitModPath(std::ostream &out, const ModPath *any, int32_t indent);
  void visitTchk(std::ostream &out, const Tchk *any, int32_t indent);
  void visitRange(std::ostream &out, const Range *any, int32_t indent);
  void visitUdpDefn(std::ostream &out, const UdpDefn *any, int32_t indent);
  void visitTableEntry(std::ostream &out, const TableEntry *any, int32_t indent);
  void visitIODecl(std::ostream &out, const IODecl *any, int32_t indent);
  void visitAlias(std::ostream &out, const Alias *any, int32_t indent);
  void visitClockingBlock(std::ostream &out, const ClockingBlock *any, int32_t indent);
  void visitClockingIODecl(std::ostream &out, const ClockingIODecl *any, int32_t indent);
  void visitCoverageOption(std::ostream &out, const CoverageOption *any, int32_t indent);
  void visitCoverBin(std::ostream &out, const CoverBin *any, int32_t indent);
  void visitCoverPoint(std::ostream &out, const CoverPoint *any, int32_t indent);
  void visitCoverCross(std::ostream &out, const CoverCross *any, int32_t indent);
  void visitCoverGroup(std::ostream &out, const CoverGroup *any, int32_t indent);
  void visitParamAssign(std::ostream &out, const ParamAssign *any, int32_t indent);
  void visitInstanceArray(std::ostream &out, const InstanceArray *any, int32_t indent);
  void visitInterfaceArray(std::ostream &out, const InterfaceArray *any, int32_t indent);
  void visitProgramArray(std::ostream &out, const ProgramArray *any, int32_t indent);
  void visitModuleArray(std::ostream &out, const ModuleArray *any, int32_t indent);
  void visitPrimitiveArray(std::ostream &out, const PrimitiveArray *any, int32_t indent);
  void visitGateArray(std::ostream &out, const GateArray *any, int32_t indent);
  void visitSwitchArray(std::ostream &out, const SwitchArray *any, int32_t indent);
  void visitUdpArray(std::ostream &out, const UdpArray *any, int32_t indent);
  void visitTypespec(std::ostream &out, const Typespec *any, int32_t indent);
  void visitPrimTerm(std::ostream &out, const PrimTerm *any, int32_t indent);
  void visitPathTerm(std::ostream &out, const PathTerm *any, int32_t indent);
  void visitTchkTerm(std::ostream &out, const TchkTerm *any, int32_t indent);
  void visitNet(std::ostream &out, const Net *any, int32_t indent);
  void visitEventTypespec(std::ostream &out, const EventTypespec *any, int32_t indent);
  void visitNamedEvent(std::ostream &out, const NamedEvent *any, int32_t indent);
  void visitNamedEventArray(std::ostream &out, const NamedEventArray *any, int32_t indent);
  void visitParameter(std::ostream &out, const Parameter *any, int32_t indent);
  void visitDefParam(std::ostream &out, const DefParam *any, int32_t indent);
  void visitSpecParam(std::ostream &out, const SpecParam *any, int32_t indent);
  void visitClassTypespec(std::ostream &out, const ClassTypespec *any, int32_t indent);
  void visitExtends(std::ostream &out, const Extends *any, int32_t indent);
  void visitImplements(std::ostream &out, const Implements *any, int32_t indent);
  void visitClassDefn(std::ostream &out, const ClassDefn *any, int32_t indent);
  void visitClassObj(std::ostream &out, const ClassObj *any, int32_t indent);
  void visitInstance(std::ostream &out, const Instance *any, int32_t indent);
  void visitInterface(std::ostream &out, const Interface *any, int32_t indent);
  void visitProgram(std::ostream &out, const Program *any, int32_t indent);
  void visitPackage(std::ostream &out, const Package *any, int32_t indent);
  void visitModule(std::ostream &out, const Module *any, int32_t indent);
  void visitCheckerDecl(std::ostream &out, const CheckerDecl *any, int32_t indent);
  void visitCheckerInst(std::ostream &out, const CheckerInst *any, int32_t indent);
  void visitBindDirective(std::ostream &out, const BindDirective *any, int32_t indent);
  void visitShortRealTypespec(std::ostream &out, const ShortRealTypespec *any, int32_t indent);
  void visitRealTypespec(std::ostream &out, const RealTypespec *any, int32_t indent);
  void visitByteTypespec(std::ostream &out, const ByteTypespec *any, int32_t indent);
  void visitShortIntTypespec(std::ostream &out, const ShortIntTypespec *any, int32_t indent);
  void visitIntTypespec(std::ostream &out, const IntTypespec *any, int32_t indent);
  void visitLongIntTypespec(std::ostream &out, const LongIntTypespec *any, int32_t indent);
  void visitIntegerTypespec(std::ostream &out, const IntegerTypespec *any, int32_t indent);
  void visitTimeTypespec(std::ostream &out, const TimeTypespec *any, int32_t indent);
  void visitEnumTypespec(std::ostream &out, const EnumTypespec *any, int32_t indent);
  void visitStringTypespec(std::ostream &out, const StringTypespec *any, int32_t indent);
  void visitTypedefTypespec(std::ostream &out, const TypedefTypespec *any, int32_t indent);
  void visitChandleTypespec(std::ostream &out, const ChandleTypespec *any, int32_t indent);
  void visitModuleTypespec(std::ostream &out, const ModuleTypespec *any, int32_t indent);
  void visitProgramTypespec(std::ostream &out, const ProgramTypespec *any, int32_t indent);
  void visitUdpDefnTypespec(std::ostream &out, const UdpDefnTypespec *any, int32_t indent);
  void visitStructTypespec(std::ostream &out, const StructTypespec *any, int32_t indent);
  void visitUnionTypespec(std::ostream &out, const UnionTypespec *any, int32_t indent);
  void visitLogicTypespec(std::ostream &out, const LogicTypespec *any, int32_t indent);
  void visitArrayTypespec(std::ostream &out, const ArrayTypespec *any, int32_t indent);
  void visitPackageTypespec(std::ostream &out, const PackageTypespec *any, int32_t indent);
  void visitVoidTypespec(std::ostream &out, const VoidTypespec *any, int32_t indent);
  void visitUnsupportedTypespec(std::ostream &out, const UnsupportedTypespec *any, int32_t indent);
  void visitSequenceTypespec(std::ostream &out, const SequenceTypespec *any, int32_t indent);
  void visitPropertyTypespec(std::ostream &out, const PropertyTypespec *any, int32_t indent);
  void visitInterfaceTypespec(std::ostream &out, const InterfaceTypespec *any, int32_t indent);
  void visitTypeParameter(std::ostream &out, const TypeParameter *any, int32_t indent);
  void visitTypespecMember(std::ostream &out, const TypespecMember *any, int32_t indent);
  void visitEnumConst(std::ostream &out, const EnumConst *any, int32_t indent);
  void visitBitTypespec(std::ostream &out, const BitTypespec *any, int32_t indent);
  void visitTFCall(std::ostream &out, const TFCall *any, int32_t indent);
  void visitUserSystf(std::ostream &out, const UserSystf *any, int32_t indent);
  void visitSysFuncCall(std::ostream &out, const SysFuncCall *any, int32_t indent);
  void visitSysTaskCall(std::ostream &out, const SysTaskCall *any, int32_t indent);
  void visitMethodFuncCall(std::ostream &out, const MethodFuncCall *any, int32_t indent);
  void visitMethodTaskCall(std::ostream &out, const MethodTaskCall *any, int32_t indent);
  void visitFuncCall(std::ostream &out, const FuncCall *any, int32_t indent);
  void visitTaskCall(std::ostream &out, const TaskCall *any, int32_t indent);
  void visitConstraintExpr(std::ostream &out, const ConstraintExpr *any, int32_t indent);
  void visitConstraintOrdering(std::ostream &out, const ConstraintOrdering *any, int32_t indent);
  void visitConstraint(std::ostream &out, const Constraint *any, int32_t indent);
  void visitImportTypespec(std::ostream &out, const ImportTypespec *any, int32_t indent);
  void visitDistItem(std::ostream &out, const DistItem *any, int32_t indent);
  void visitDistribution(std::ostream &out, const Distribution *any, int32_t indent);
  void visitImplication(std::ostream &out, const Implication *any, int32_t indent);
  void visitConstrIf(std::ostream &out, const ConstrIf *any, int32_t indent);
  void visitConstrIfElse(std::ostream &out, const ConstrIfElse *any, int32_t indent);
  void visitConstrForeach(std::ostream &out, const ConstrForeach *any, int32_t indent);
  void visitSoftDisable(std::ostream &out, const SoftDisable *any, int32_t indent);
  void visitUniqueness(std::ostream &out, const Uniqueness *any, int32_t indent);
  void visitGenStmt(std::ostream &out, const GenStmt *any, int32_t indent);
  void visitGenIf(std::ostream &out, const GenIf *any, int32_t indent);
  void visitGenIfElse(std::ostream &out, const GenIfElse *any, int32_t indent);
  void visitGenFor(std::ostream &out, const GenFor *any, int32_t indent);
  void visitGenCase(std::ostream &out, const GenCase *any, int32_t indent);
  void visitGenRegion(std::ostream &out, const GenRegion *any, int32_t indent);
  void visitClause(std::ostream &out, const Clause *any, int32_t indent);
  void visitConfigRule(std::ostream &out, const ConfigRule *any, int32_t indent);
  void visitConfigDecl(std::ostream &out, const ConfigDecl *any, int32_t indent);
  void visitLibrary(std::ostream &out, const Library *any, int32_t indent);
  void visitIncludeStmt(std::ostream &out, const IncludeStmt *any, int32_t indent);
  void visitDesign(std::ostream &out, const Design *any, int32_t indent);
  // clang-format on

 protected:
  struct AnyComparer final {
    bool operator()(const Any *lhs, const Any *rhs) const {
      return (lhs->getUhdmType() == rhs->getUhdmType()) ? (lhs->getUhdmId() < rhs->getUhdmId())
                                                        : (lhs->getUhdmType() < rhs->getUhdmType());
    }
  };
  using OrderedAnySet = std::set<const Any *, AnyComparer>;

  void printAny(std::ostream &out, const Any *any, size_t indent, std::string_view relation);
  template <typename T, typename = std::enable_if_t<std::is_base_of_v<Any, T>>>
  std::ostream &printCollection(std::ostream &out, const std::vector<T *> &collection, size_t indent,
                                std::string_view relation);

  // clang-format off
  virtual std::ostream &printAnyCollection(std::ostream &out, const AnyCollection &collection, size_t indent, std::string_view relation);
  virtual std::ostream &printAttributeCollection(std::ostream &out, const AttributeCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printIdentifierCollection(std::ostream &out, const IdentifierCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printCommentCollection(std::ostream &out, const CommentCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printLetDeclCollection(std::ostream &out, const LetDeclCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printConcurrentAssertionsCollection(std::ostream &out, const ConcurrentAssertionsCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printProcessCollection(std::ostream &out, const ProcessCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printAlwaysCollection(std::ostream &out, const AlwaysCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printFinalStmtCollection(std::ostream &out, const FinalStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printInitialCollection(std::ostream &out, const InitialCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printAtomicStmtCollection(std::ostream &out, const AtomicStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printDelayControlCollection(std::ostream &out, const DelayControlCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printDelayTermCollection(std::ostream &out, const DelayTermCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printEventControlCollection(std::ostream &out, const EventControlCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printRepeatControlCollection(std::ostream &out, const RepeatControlCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printScopeCollection(std::ostream &out, const ScopeCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printBeginCollection(std::ostream &out, const BeginCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printForkStmtCollection(std::ostream &out, const ForkStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printForStmtCollection(std::ostream &out, const ForStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printIfStmtCollection(std::ostream &out, const IfStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printEventStmtCollection(std::ostream &out, const EventStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printThreadCollection(std::ostream &out, const ThreadCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printForeverStmtCollection(std::ostream &out, const ForeverStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printWaitsCollection(std::ostream &out, const WaitsCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printWaitStmtCollection(std::ostream &out, const WaitStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printWaitForkCollection(std::ostream &out, const WaitForkCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printOrderedWaitCollection(std::ostream &out, const OrderedWaitCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printDisablesCollection(std::ostream &out, const DisablesCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printDisableCollection(std::ostream &out, const DisableCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printDisableForkCollection(std::ostream &out, const DisableForkCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printContinueStmtCollection(std::ostream &out, const ContinueStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printBreakStmtCollection(std::ostream &out, const BreakStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printReturnStmtCollection(std::ostream &out, const ReturnStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printWhileStmtCollection(std::ostream &out, const WhileStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printRepeatCollection(std::ostream &out, const RepeatCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printDoWhileCollection(std::ostream &out, const DoWhileCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printIfElseCollection(std::ostream &out, const IfElseCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printCaseStmtCollection(std::ostream &out, const CaseStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printForceCollection(std::ostream &out, const ForceCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printAssignStmtCollection(std::ostream &out, const AssignStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printDeassignCollection(std::ostream &out, const DeassignCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printReleaseCollection(std::ostream &out, const ReleaseCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printNullStmtCollection(std::ostream &out, const NullStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printExpectStmtCollection(std::ostream &out, const ExpectStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printForeachStmtCollection(std::ostream &out, const ForeachStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printGenScopeCollection(std::ostream &out, const GenScopeCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printGenScopeArrayCollection(std::ostream &out, const GenScopeArrayCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printAssertCollection(std::ostream &out, const AssertCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printCoverCollection(std::ostream &out, const CoverCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printAssumeCollection(std::ostream &out, const AssumeCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printRestrictCollection(std::ostream &out, const RestrictCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printImmediateAssertCollection(std::ostream &out, const ImmediateAssertCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printImmediateAssumeCollection(std::ostream &out, const ImmediateAssumeCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printImmediateCoverCollection(std::ostream &out, const ImmediateCoverCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printExprCollection(std::ostream &out, const ExprCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printCaseItemCollection(std::ostream &out, const CaseItemCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printAssignmentCollection(std::ostream &out, const AssignmentCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printAnyPatternCollection(std::ostream &out, const AnyPatternCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printTaggedPatternCollection(std::ostream &out, const TaggedPatternCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printStructPatternCollection(std::ostream &out, const StructPatternCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printUnsupportedExprCollection(std::ostream &out, const UnsupportedExprCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printUnsupportedStmtCollection(std::ostream &out, const UnsupportedStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printPreprocMacroDefinitionCollection(std::ostream &out, const PreprocMacroDefinitionCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printPreprocMacroInstanceCollection(std::ostream &out, const PreprocMacroInstanceCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printSourceFileCollection(std::ostream &out, const SourceFileCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printSequenceInstCollection(std::ostream &out, const SequenceInstCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printSeqFormalDeclCollection(std::ostream &out, const SeqFormalDeclCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printSequenceDeclCollection(std::ostream &out, const SequenceDeclCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printPropFormalDeclCollection(std::ostream &out, const PropFormalDeclCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printPropertyInstCollection(std::ostream &out, const PropertyInstCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printPropertySpecCollection(std::ostream &out, const PropertySpecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printPropertyDeclCollection(std::ostream &out, const PropertyDeclCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printClockedPropertyCollection(std::ostream &out, const ClockedPropertyCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printCasePropertyItemCollection(std::ostream &out, const CasePropertyItemCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printCasePropertyCollection(std::ostream &out, const CasePropertyCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printMulticlockSequenceExprCollection(std::ostream &out, const MulticlockSequenceExprCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printClockedSeqCollection(std::ostream &out, const ClockedSeqCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printSimpleExprCollection(std::ostream &out, const SimpleExprCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printConstantCollection(std::ostream &out, const ConstantCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printLetExprCollection(std::ostream &out, const LetExprCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printOperationCollection(std::ostream &out, const OperationCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printRefObjCollection(std::ostream &out, const RefObjCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printRefInstanceCollection(std::ostream &out, const RefInstanceCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printRefTypespecCollection(std::ostream &out, const RefTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printSelectCollection(std::ostream &out, const SelectCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printPartSelectCollection(std::ostream &out, const PartSelectCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printIndexedPartSelectCollection(std::ostream &out, const IndexedPartSelectCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printVarSelectCollection(std::ostream &out, const VarSelectCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printBitSelectCollection(std::ostream &out, const BitSelectCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printVariableCollection(std::ostream &out, const VariableCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printHierPathCollection(std::ostream &out, const HierPathCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printArrayExprCollection(std::ostream &out, const ArrayExprCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printRegArrayCollection(std::ostream &out, const RegArrayCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printRegCollection(std::ostream &out, const RegCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printTaskFuncCollection(std::ostream &out, const TaskFuncCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printTaskCollection(std::ostream &out, const TaskCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printFunctionCollection(std::ostream &out, const FunctionCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printTaskFuncDeclCollection(std::ostream &out, const TaskFuncDeclCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printTaskDeclCollection(std::ostream &out, const TaskDeclCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printFunctionDeclCollection(std::ostream &out, const FunctionDeclCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printModportCollection(std::ostream &out, const ModportCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printInterfaceTFDeclCollection(std::ostream &out, const InterfaceTFDeclCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printContAssignCollection(std::ostream &out, const ContAssignCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printContAssignBitCollection(std::ostream &out, const ContAssignBitCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printPortsCollection(std::ostream &out, const PortsCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printPortCollection(std::ostream &out, const PortCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printPortBitCollection(std::ostream &out, const PortBitCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printCheckerPortCollection(std::ostream &out, const CheckerPortCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printCheckerInstPortCollection(std::ostream &out, const CheckerInstPortCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printPrimitiveCollection(std::ostream &out, const PrimitiveCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printGateCollection(std::ostream &out, const GateCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printSwitchTranCollection(std::ostream &out, const SwitchTranCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printUdpCollection(std::ostream &out, const UdpCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printModPathCollection(std::ostream &out, const ModPathCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printTchkCollection(std::ostream &out, const TchkCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printRangeCollection(std::ostream &out, const RangeCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printUdpDefnCollection(std::ostream &out, const UdpDefnCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printTableEntryCollection(std::ostream &out, const TableEntryCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printIODeclCollection(std::ostream &out, const IODeclCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printAliasCollection(std::ostream &out, const AliasCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printClockingBlockCollection(std::ostream &out, const ClockingBlockCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printClockingIODeclCollection(std::ostream &out, const ClockingIODeclCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printCoverageOptionCollection(std::ostream &out, const CoverageOptionCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printCoverBinCollection(std::ostream &out, const CoverBinCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printCoverPointCollection(std::ostream &out, const CoverPointCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printCoverCrossCollection(std::ostream &out, const CoverCrossCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printCoverGroupCollection(std::ostream &out, const CoverGroupCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printParamAssignCollection(std::ostream &out, const ParamAssignCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printInstanceArrayCollection(std::ostream &out, const InstanceArrayCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printInterfaceArrayCollection(std::ostream &out, const InterfaceArrayCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printProgramArrayCollection(std::ostream &out, const ProgramArrayCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printModuleArrayCollection(std::ostream &out, const ModuleArrayCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printPrimitiveArrayCollection(std::ostream &out, const PrimitiveArrayCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printGateArrayCollection(std::ostream &out, const GateArrayCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printSwitchArrayCollection(std::ostream &out, const SwitchArrayCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printUdpArrayCollection(std::ostream &out, const UdpArrayCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printTypespecCollection(std::ostream &out, const TypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printPrimTermCollection(std::ostream &out, const PrimTermCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printPathTermCollection(std::ostream &out, const PathTermCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printTchkTermCollection(std::ostream &out, const TchkTermCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printNetCollection(std::ostream &out, const NetCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printEventTypespecCollection(std::ostream &out, const EventTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printNamedEventCollection(std::ostream &out, const NamedEventCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printNamedEventArrayCollection(std::ostream &out, const NamedEventArrayCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printParameterCollection(std::ostream &out, const ParameterCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printDefParamCollection(std::ostream &out, const DefParamCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printSpecParamCollection(std::ostream &out, const SpecParamCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printClassTypespecCollection(std::ostream &out, const ClassTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printExtendsCollection(std::ostream &out, const ExtendsCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printImplementsCollection(std::ostream &out, const ImplementsCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printClassDefnCollection(std::ostream &out, const ClassDefnCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printClassObjCollection(std::ostream &out, const ClassObjCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printInstanceCollection(std::ostream &out, const InstanceCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printInterfaceCollection(std::ostream &out, const InterfaceCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printProgramCollection(std::ostream &out, const ProgramCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printPackageCollection(std::ostream &out, const PackageCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printModuleCollection(std::ostream &out, const ModuleCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printCheckerDeclCollection(std::ostream &out, const CheckerDeclCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printCheckerInstCollection(std::ostream &out, const CheckerInstCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printBindDirectiveCollection(std::ostream &out, const BindDirectiveCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printShortRealTypespecCollection(std::ostream &out, const ShortRealTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printRealTypespecCollection(std::ostream &out, const RealTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printByteTypespecCollection(std::ostream &out, const ByteTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printShortIntTypespecCollection(std::ostream &out, const ShortIntTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printIntTypespecCollection(std::ostream &out, const IntTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printLongIntTypespecCollection(std::ostream &out, const LongIntTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printIntegerTypespecCollection(std::ostream &out, const IntegerTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printTimeTypespecCollection(std::ostream &out, const TimeTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printEnumTypespecCollection(std::ostream &out, const EnumTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printStringTypespecCollection(std::ostream &out, const StringTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printTypedefTypespecCollection(std::ostream &out, const TypedefTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printChandleTypespecCollection(std::ostream &out, const ChandleTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printModuleTypespecCollection(std::ostream &out, const ModuleTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printProgramTypespecCollection(std::ostream &out, const ProgramTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printUdpDefnTypespecCollection(std::ostream &out, const UdpDefnTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printStructTypespecCollection(std::ostream &out, const StructTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printUnionTypespecCollection(std::ostream &out, const UnionTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printLogicTypespecCollection(std::ostream &out, const LogicTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printArrayTypespecCollection(std::ostream &out, const ArrayTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printPackageTypespecCollection(std::ostream &out, const PackageTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printVoidTypespecCollection(std::ostream &out, const VoidTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printUnsupportedTypespecCollection(std::ostream &out, const UnsupportedTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printSequenceTypespecCollection(std::ostream &out, const SequenceTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printPropertyTypespecCollection(std::ostream &out, const PropertyTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printInterfaceTypespecCollection(std::ostream &out, const InterfaceTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printTypeParameterCollection(std::ostream &out, const TypeParameterCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printTypespecMemberCollection(std::ostream &out, const TypespecMemberCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printEnumConstCollection(std::ostream &out, const EnumConstCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printBitTypespecCollection(std::ostream &out, const BitTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printTFCallCollection(std::ostream &out, const TFCallCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printUserSystfCollection(std::ostream &out, const UserSystfCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printSysFuncCallCollection(std::ostream &out, const SysFuncCallCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printSysTaskCallCollection(std::ostream &out, const SysTaskCallCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printMethodFuncCallCollection(std::ostream &out, const MethodFuncCallCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printMethodTaskCallCollection(std::ostream &out, const MethodTaskCallCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printFuncCallCollection(std::ostream &out, const FuncCallCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printTaskCallCollection(std::ostream &out, const TaskCallCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printConstraintExprCollection(std::ostream &out, const ConstraintExprCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printConstraintOrderingCollection(std::ostream &out, const ConstraintOrderingCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printConstraintCollection(std::ostream &out, const ConstraintCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printImportTypespecCollection(std::ostream &out, const ImportTypespecCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printDistItemCollection(std::ostream &out, const DistItemCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printDistributionCollection(std::ostream &out, const DistributionCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printImplicationCollection(std::ostream &out, const ImplicationCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printConstrIfCollection(std::ostream &out, const ConstrIfCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printConstrIfElseCollection(std::ostream &out, const ConstrIfElseCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printConstrForeachCollection(std::ostream &out, const ConstrForeachCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printSoftDisableCollection(std::ostream &out, const SoftDisableCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printUniquenessCollection(std::ostream &out, const UniquenessCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printGenStmtCollection(std::ostream &out, const GenStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printGenIfCollection(std::ostream &out, const GenIfCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printGenIfElseCollection(std::ostream &out, const GenIfElseCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printGenForCollection(std::ostream &out, const GenForCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printGenCaseCollection(std::ostream &out, const GenCaseCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printGenRegionCollection(std::ostream &out, const GenRegionCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printClauseCollection(std::ostream &out, const ClauseCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printConfigRuleCollection(std::ostream &out, const ConfigRuleCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printConfigDeclCollection(std::ostream &out, const ConfigDeclCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printLibraryCollection(std::ostream &out, const LibraryCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printIncludeStmtCollection(std::ostream &out, const IncludeStmtCollection &collection, int32_t indent, std::string_view relation);
  virtual std::ostream &printDesignCollection(std::ostream &out, const DesignCollection &collection, int32_t indent, std::string_view relation);
  // clang-format on

 protected:
  std::ostream &m_out;
  Callstack m_callstack;
  any_set_t m_visited;
  bool m_forceShallowVisit = false;
  bool m_printIds = s_printIds;
};

template <typename C>
std::enable_if_t<std::is_base_of_v<Any, std::remove_pointer_t<typename C::value_type>>>
UhdmPrinter::printTree(const C &collection, size_t indent /* = 0 */) {
  for (const Any *any : collection) {
    printTree(any, indent);
  }
}

template <typename C>
std::enable_if_t<std::is_convertible_v<typename C::value_type, vpiHandle>>
UhdmPrinter::printTree(const C &handles, size_t indent /* = 0 */) {
  for (const vpiHandle h : handles) {
    printTree(h, indent);
  }
}

template <typename C>
std::enable_if_t<std::is_base_of_v<Any, std::remove_pointer_t<typename C::value_type>>>
UhdmPrinter::printList(const C &collection, size_t indent /* = 0 */) {
  UhdmListener listener;
  for (const Any *any : collection) {
    listener.listenAny(any);
  }

  const OrderedAnySet visited(listener.getVisited().cbegin(), listener.getVisited().cend());

  const bool forceShallowVisit = m_forceShallowVisit;
  m_forceShallowVisit = true;
  for (const Any *any : visited) {
    printAny(m_out, any, indent, "");
  }
  m_forceShallowVisit = forceShallowVisit;
}

template <typename C>
std::enable_if_t<std::is_convertible_v<typename C::value_type, vpiHandle>>
UhdmPrinter::printList(const C &handles, size_t indent /* = 0 */) {
  UhdmListener listener;
  for (const vpiHandle &h : handles) {
    listener.listenAny((const Any *)((const UhdmHandle *)h)->object);
  }

  const OrderedAnySet visited(listener.getVisited().cbegin(), listener.getVisited().cend());

  const bool forceShallowVisit = m_forceShallowVisit;
  m_forceShallowVisit = true;
  for (const Any *object : visited) {
    printAny(m_out, object, indent, "");
  }
  m_forceShallowVisit = forceShallowVisit;
}

class UhdmYamlPrinter final : public UhdmPrinter {
 public:
  explicit UhdmYamlPrinter(std::ostream &out) : UhdmPrinter(out) {}

 private:
  std::ostream &beginPrintAny(std::ostream &out, const Any *any, size_t &indent, std::string_view relation,
                              bool shallowVisit) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, bool value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, int32_t value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, uint32_t value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, std::string_view value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, const s_vpi_value *value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, s_vpi_delay *delay,
                              size_t indent) override;
  std::ostream &endPrintAny(std::ostream &out, const Any *any, size_t &indent, std::string_view relation,
                            bool shallowVisit) override;
  template <typename T, typename = std::enable_if_t<std::is_base_of_v<Any, T>>>
  std::ostream &printCollection(std::ostream &out, const std::vector<T *> &collection, size_t indent,
                                std::string_view relation);

  // clang-format off
  std::ostream &printAnyCollection(std::ostream &out, const AnyCollection &collection, size_t indent, std::string_view relation) override;
  std::ostream &printAttributeCollection(std::ostream &out, const AttributeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIdentifierCollection(std::ostream &out, const IdentifierCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCommentCollection(std::ostream &out, const CommentCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLetDeclCollection(std::ostream &out, const LetDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConcurrentAssertionsCollection(std::ostream &out, const ConcurrentAssertionsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printProcessCollection(std::ostream &out, const ProcessCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAlwaysCollection(std::ostream &out, const AlwaysCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printFinalStmtCollection(std::ostream &out, const FinalStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInitialCollection(std::ostream &out, const InitialCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAtomicStmtCollection(std::ostream &out, const AtomicStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDelayControlCollection(std::ostream &out, const DelayControlCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDelayTermCollection(std::ostream &out, const DelayTermCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEventControlCollection(std::ostream &out, const EventControlCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRepeatControlCollection(std::ostream &out, const RepeatControlCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printScopeCollection(std::ostream &out, const ScopeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBeginCollection(std::ostream &out, const BeginCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForkStmtCollection(std::ostream &out, const ForkStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForStmtCollection(std::ostream &out, const ForStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIfStmtCollection(std::ostream &out, const IfStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEventStmtCollection(std::ostream &out, const EventStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printThreadCollection(std::ostream &out, const ThreadCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForeverStmtCollection(std::ostream &out, const ForeverStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printWaitsCollection(std::ostream &out, const WaitsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printWaitStmtCollection(std::ostream &out, const WaitStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printWaitForkCollection(std::ostream &out, const WaitForkCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printOrderedWaitCollection(std::ostream &out, const OrderedWaitCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDisablesCollection(std::ostream &out, const DisablesCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDisableCollection(std::ostream &out, const DisableCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDisableForkCollection(std::ostream &out, const DisableForkCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printContinueStmtCollection(std::ostream &out, const ContinueStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBreakStmtCollection(std::ostream &out, const BreakStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printReturnStmtCollection(std::ostream &out, const ReturnStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printWhileStmtCollection(std::ostream &out, const WhileStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRepeatCollection(std::ostream &out, const RepeatCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDoWhileCollection(std::ostream &out, const DoWhileCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIfElseCollection(std::ostream &out, const IfElseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCaseStmtCollection(std::ostream &out, const CaseStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForceCollection(std::ostream &out, const ForceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAssignStmtCollection(std::ostream &out, const AssignStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDeassignCollection(std::ostream &out, const DeassignCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printReleaseCollection(std::ostream &out, const ReleaseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printNullStmtCollection(std::ostream &out, const NullStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printExpectStmtCollection(std::ostream &out, const ExpectStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForeachStmtCollection(std::ostream &out, const ForeachStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenScopeCollection(std::ostream &out, const GenScopeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenScopeArrayCollection(std::ostream &out, const GenScopeArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAssertCollection(std::ostream &out, const AssertCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverCollection(std::ostream &out, const CoverCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAssumeCollection(std::ostream &out, const AssumeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRestrictCollection(std::ostream &out, const RestrictCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImmediateAssertCollection(std::ostream &out, const ImmediateAssertCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImmediateAssumeCollection(std::ostream &out, const ImmediateAssumeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImmediateCoverCollection(std::ostream &out, const ImmediateCoverCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printExprCollection(std::ostream &out, const ExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCaseItemCollection(std::ostream &out, const CaseItemCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAssignmentCollection(std::ostream &out, const AssignmentCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAnyPatternCollection(std::ostream &out, const AnyPatternCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaggedPatternCollection(std::ostream &out, const TaggedPatternCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printStructPatternCollection(std::ostream &out, const StructPatternCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUnsupportedExprCollection(std::ostream &out, const UnsupportedExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUnsupportedStmtCollection(std::ostream &out, const UnsupportedStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPreprocMacroDefinitionCollection(std::ostream &out, const PreprocMacroDefinitionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPreprocMacroInstanceCollection(std::ostream &out, const PreprocMacroInstanceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSourceFileCollection(std::ostream &out, const SourceFileCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSequenceInstCollection(std::ostream &out, const SequenceInstCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSeqFormalDeclCollection(std::ostream &out, const SeqFormalDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSequenceDeclCollection(std::ostream &out, const SequenceDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropFormalDeclCollection(std::ostream &out, const PropFormalDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropertyInstCollection(std::ostream &out, const PropertyInstCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropertySpecCollection(std::ostream &out, const PropertySpecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropertyDeclCollection(std::ostream &out, const PropertyDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClockedPropertyCollection(std::ostream &out, const ClockedPropertyCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCasePropertyItemCollection(std::ostream &out, const CasePropertyItemCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCasePropertyCollection(std::ostream &out, const CasePropertyCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printMulticlockSequenceExprCollection(std::ostream &out, const MulticlockSequenceExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClockedSeqCollection(std::ostream &out, const ClockedSeqCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSimpleExprCollection(std::ostream &out, const SimpleExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstantCollection(std::ostream &out, const ConstantCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLetExprCollection(std::ostream &out, const LetExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printOperationCollection(std::ostream &out, const OperationCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRefObjCollection(std::ostream &out, const RefObjCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRefInstanceCollection(std::ostream &out, const RefInstanceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRefTypespecCollection(std::ostream &out, const RefTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSelectCollection(std::ostream &out, const SelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPartSelectCollection(std::ostream &out, const PartSelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIndexedPartSelectCollection(std::ostream &out, const IndexedPartSelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printVarSelectCollection(std::ostream &out, const VarSelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBitSelectCollection(std::ostream &out, const BitSelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printVariableCollection(std::ostream &out, const VariableCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printHierPathCollection(std::ostream &out, const HierPathCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printArrayExprCollection(std::ostream &out, const ArrayExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRegArrayCollection(std::ostream &out, const RegArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRegCollection(std::ostream &out, const RegCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskFuncCollection(std::ostream &out, const TaskFuncCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskCollection(std::ostream &out, const TaskCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printFunctionCollection(std::ostream &out, const FunctionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskFuncDeclCollection(std::ostream &out, const TaskFuncDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskDeclCollection(std::ostream &out, const TaskDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printFunctionDeclCollection(std::ostream &out, const FunctionDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModportCollection(std::ostream &out, const ModportCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInterfaceTFDeclCollection(std::ostream &out, const InterfaceTFDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printContAssignCollection(std::ostream &out, const ContAssignCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printContAssignBitCollection(std::ostream &out, const ContAssignBitCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPortsCollection(std::ostream &out, const PortsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPortCollection(std::ostream &out, const PortCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPortBitCollection(std::ostream &out, const PortBitCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCheckerPortCollection(std::ostream &out, const CheckerPortCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCheckerInstPortCollection(std::ostream &out, const CheckerInstPortCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPrimitiveCollection(std::ostream &out, const PrimitiveCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGateCollection(std::ostream &out, const GateCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSwitchTranCollection(std::ostream &out, const SwitchTranCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUdpCollection(std::ostream &out, const UdpCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModPathCollection(std::ostream &out, const ModPathCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTchkCollection(std::ostream &out, const TchkCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRangeCollection(std::ostream &out, const RangeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUdpDefnCollection(std::ostream &out, const UdpDefnCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTableEntryCollection(std::ostream &out, const TableEntryCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIODeclCollection(std::ostream &out, const IODeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAliasCollection(std::ostream &out, const AliasCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClockingBlockCollection(std::ostream &out, const ClockingBlockCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClockingIODeclCollection(std::ostream &out, const ClockingIODeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverageOptionCollection(std::ostream &out, const CoverageOptionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverBinCollection(std::ostream &out, const CoverBinCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverPointCollection(std::ostream &out, const CoverPointCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverCrossCollection(std::ostream &out, const CoverCrossCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverGroupCollection(std::ostream &out, const CoverGroupCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printParamAssignCollection(std::ostream &out, const ParamAssignCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInstanceArrayCollection(std::ostream &out, const InstanceArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInterfaceArrayCollection(std::ostream &out, const InterfaceArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printProgramArrayCollection(std::ostream &out, const ProgramArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModuleArrayCollection(std::ostream &out, const ModuleArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPrimitiveArrayCollection(std::ostream &out, const PrimitiveArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGateArrayCollection(std::ostream &out, const GateArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSwitchArrayCollection(std::ostream &out, const SwitchArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUdpArrayCollection(std::ostream &out, const UdpArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTypespecCollection(std::ostream &out, const TypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPrimTermCollection(std::ostream &out, const PrimTermCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPathTermCollection(std::ostream &out, const PathTermCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTchkTermCollection(std::ostream &out, const TchkTermCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printNetCollection(std::ostream &out, const NetCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEventTypespecCollection(std::ostream &out, const EventTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printNamedEventCollection(std::ostream &out, const NamedEventCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printNamedEventArrayCollection(std::ostream &out, const NamedEventArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printParameterCollection(std::ostream &out, const ParameterCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDefParamCollection(std::ostream &out, const DefParamCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSpecParamCollection(std::ostream &out, const SpecParamCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClassTypespecCollection(std::ostream &out, const ClassTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printExtendsCollection(std::ostream &out, const ExtendsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImplementsCollection(std::ostream &out, const ImplementsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClassDefnCollection(std::ostream &out, const ClassDefnCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClassObjCollection(std::ostream &out, const ClassObjCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInstanceCollection(std::ostream &out, const InstanceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInterfaceCollection(std::ostream &out, const InterfaceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printProgramCollection(std::ostream &out, const ProgramCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPackageCollection(std::ostream &out, const PackageCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModuleCollection(std::ostream &out, const ModuleCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCheckerDeclCollection(std::ostream &out, const CheckerDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCheckerInstCollection(std::ostream &out, const CheckerInstCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBindDirectiveCollection(std::ostream &out, const BindDirectiveCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printShortRealTypespecCollection(std::ostream &out, const ShortRealTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRealTypespecCollection(std::ostream &out, const RealTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printByteTypespecCollection(std::ostream &out, const ByteTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printShortIntTypespecCollection(std::ostream &out, const ShortIntTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIntTypespecCollection(std::ostream &out, const IntTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLongIntTypespecCollection(std::ostream &out, const LongIntTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIntegerTypespecCollection(std::ostream &out, const IntegerTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTimeTypespecCollection(std::ostream &out, const TimeTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEnumTypespecCollection(std::ostream &out, const EnumTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printStringTypespecCollection(std::ostream &out, const StringTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTypedefTypespecCollection(std::ostream &out, const TypedefTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printChandleTypespecCollection(std::ostream &out, const ChandleTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModuleTypespecCollection(std::ostream &out, const ModuleTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printProgramTypespecCollection(std::ostream &out, const ProgramTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUdpDefnTypespecCollection(std::ostream &out, const UdpDefnTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printStructTypespecCollection(std::ostream &out, const StructTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUnionTypespecCollection(std::ostream &out, const UnionTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLogicTypespecCollection(std::ostream &out, const LogicTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printArrayTypespecCollection(std::ostream &out, const ArrayTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPackageTypespecCollection(std::ostream &out, const PackageTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printVoidTypespecCollection(std::ostream &out, const VoidTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUnsupportedTypespecCollection(std::ostream &out, const UnsupportedTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSequenceTypespecCollection(std::ostream &out, const SequenceTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropertyTypespecCollection(std::ostream &out, const PropertyTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInterfaceTypespecCollection(std::ostream &out, const InterfaceTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTypeParameterCollection(std::ostream &out, const TypeParameterCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTypespecMemberCollection(std::ostream &out, const TypespecMemberCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEnumConstCollection(std::ostream &out, const EnumConstCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBitTypespecCollection(std::ostream &out, const BitTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTFCallCollection(std::ostream &out, const TFCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUserSystfCollection(std::ostream &out, const UserSystfCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSysFuncCallCollection(std::ostream &out, const SysFuncCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSysTaskCallCollection(std::ostream &out, const SysTaskCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printMethodFuncCallCollection(std::ostream &out, const MethodFuncCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printMethodTaskCallCollection(std::ostream &out, const MethodTaskCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printFuncCallCollection(std::ostream &out, const FuncCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskCallCollection(std::ostream &out, const TaskCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstraintExprCollection(std::ostream &out, const ConstraintExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstraintOrderingCollection(std::ostream &out, const ConstraintOrderingCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstraintCollection(std::ostream &out, const ConstraintCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImportTypespecCollection(std::ostream &out, const ImportTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDistItemCollection(std::ostream &out, const DistItemCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDistributionCollection(std::ostream &out, const DistributionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImplicationCollection(std::ostream &out, const ImplicationCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstrIfCollection(std::ostream &out, const ConstrIfCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstrIfElseCollection(std::ostream &out, const ConstrIfElseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstrForeachCollection(std::ostream &out, const ConstrForeachCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSoftDisableCollection(std::ostream &out, const SoftDisableCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUniquenessCollection(std::ostream &out, const UniquenessCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenStmtCollection(std::ostream &out, const GenStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenIfCollection(std::ostream &out, const GenIfCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenIfElseCollection(std::ostream &out, const GenIfElseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenForCollection(std::ostream &out, const GenForCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenCaseCollection(std::ostream &out, const GenCaseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenRegionCollection(std::ostream &out, const GenRegionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClauseCollection(std::ostream &out, const ClauseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConfigRuleCollection(std::ostream &out, const ConfigRuleCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConfigDeclCollection(std::ostream &out, const ConfigDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLibraryCollection(std::ostream &out, const LibraryCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIncludeStmtCollection(std::ostream &out, const IncludeStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDesignCollection(std::ostream &out, const DesignCollection &collection, int32_t indent, std::string_view relation) override;
  // clang-format on
};

class UhdmDumpPrinter final : public UhdmPrinter {
 public:
  explicit UhdmDumpPrinter(std::ostream &out) : UhdmPrinter(out) {}

 private:
  std::ostream &beginPrintAny(std::ostream &out, const Any *any, size_t &indent, std::string_view relation,
                              bool shallowVisit) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, bool value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, int32_t value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, uint32_t value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, std::string_view value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, const s_vpi_value *value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, s_vpi_delay *delay,
                              size_t indent) override;
  std::ostream &endPrintAny(std::ostream &out, const Any *any, size_t &indent, std::string_view relation,
                            bool shallowVisit) override;
  template <typename T, typename = std::enable_if_t<std::is_base_of_v<Any, T>>>
  std::ostream &printCollection(std::ostream &out, const std::vector<T *> &collection, size_t indent,
                                std::string_view relation);

  // clang-format off
  std::ostream &printAnyCollection(std::ostream &out, const AnyCollection &collection, size_t indent, std::string_view relation) override;
  std::ostream &printAttributeCollection(std::ostream &out, const AttributeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIdentifierCollection(std::ostream &out, const IdentifierCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCommentCollection(std::ostream &out, const CommentCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLetDeclCollection(std::ostream &out, const LetDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConcurrentAssertionsCollection(std::ostream &out, const ConcurrentAssertionsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printProcessCollection(std::ostream &out, const ProcessCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAlwaysCollection(std::ostream &out, const AlwaysCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printFinalStmtCollection(std::ostream &out, const FinalStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInitialCollection(std::ostream &out, const InitialCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAtomicStmtCollection(std::ostream &out, const AtomicStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDelayControlCollection(std::ostream &out, const DelayControlCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDelayTermCollection(std::ostream &out, const DelayTermCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEventControlCollection(std::ostream &out, const EventControlCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRepeatControlCollection(std::ostream &out, const RepeatControlCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printScopeCollection(std::ostream &out, const ScopeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBeginCollection(std::ostream &out, const BeginCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForkStmtCollection(std::ostream &out, const ForkStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForStmtCollection(std::ostream &out, const ForStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIfStmtCollection(std::ostream &out, const IfStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEventStmtCollection(std::ostream &out, const EventStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printThreadCollection(std::ostream &out, const ThreadCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForeverStmtCollection(std::ostream &out, const ForeverStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printWaitsCollection(std::ostream &out, const WaitsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printWaitStmtCollection(std::ostream &out, const WaitStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printWaitForkCollection(std::ostream &out, const WaitForkCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printOrderedWaitCollection(std::ostream &out, const OrderedWaitCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDisablesCollection(std::ostream &out, const DisablesCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDisableCollection(std::ostream &out, const DisableCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDisableForkCollection(std::ostream &out, const DisableForkCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printContinueStmtCollection(std::ostream &out, const ContinueStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBreakStmtCollection(std::ostream &out, const BreakStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printReturnStmtCollection(std::ostream &out, const ReturnStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printWhileStmtCollection(std::ostream &out, const WhileStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRepeatCollection(std::ostream &out, const RepeatCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDoWhileCollection(std::ostream &out, const DoWhileCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIfElseCollection(std::ostream &out, const IfElseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCaseStmtCollection(std::ostream &out, const CaseStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForceCollection(std::ostream &out, const ForceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAssignStmtCollection(std::ostream &out, const AssignStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDeassignCollection(std::ostream &out, const DeassignCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printReleaseCollection(std::ostream &out, const ReleaseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printNullStmtCollection(std::ostream &out, const NullStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printExpectStmtCollection(std::ostream &out, const ExpectStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForeachStmtCollection(std::ostream &out, const ForeachStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenScopeCollection(std::ostream &out, const GenScopeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenScopeArrayCollection(std::ostream &out, const GenScopeArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAssertCollection(std::ostream &out, const AssertCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverCollection(std::ostream &out, const CoverCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAssumeCollection(std::ostream &out, const AssumeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRestrictCollection(std::ostream &out, const RestrictCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImmediateAssertCollection(std::ostream &out, const ImmediateAssertCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImmediateAssumeCollection(std::ostream &out, const ImmediateAssumeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImmediateCoverCollection(std::ostream &out, const ImmediateCoverCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printExprCollection(std::ostream &out, const ExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCaseItemCollection(std::ostream &out, const CaseItemCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAssignmentCollection(std::ostream &out, const AssignmentCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAnyPatternCollection(std::ostream &out, const AnyPatternCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaggedPatternCollection(std::ostream &out, const TaggedPatternCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printStructPatternCollection(std::ostream &out, const StructPatternCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUnsupportedExprCollection(std::ostream &out, const UnsupportedExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUnsupportedStmtCollection(std::ostream &out, const UnsupportedStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPreprocMacroDefinitionCollection(std::ostream &out, const PreprocMacroDefinitionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPreprocMacroInstanceCollection(std::ostream &out, const PreprocMacroInstanceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSourceFileCollection(std::ostream &out, const SourceFileCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSequenceInstCollection(std::ostream &out, const SequenceInstCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSeqFormalDeclCollection(std::ostream &out, const SeqFormalDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSequenceDeclCollection(std::ostream &out, const SequenceDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropFormalDeclCollection(std::ostream &out, const PropFormalDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropertyInstCollection(std::ostream &out, const PropertyInstCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropertySpecCollection(std::ostream &out, const PropertySpecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropertyDeclCollection(std::ostream &out, const PropertyDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClockedPropertyCollection(std::ostream &out, const ClockedPropertyCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCasePropertyItemCollection(std::ostream &out, const CasePropertyItemCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCasePropertyCollection(std::ostream &out, const CasePropertyCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printMulticlockSequenceExprCollection(std::ostream &out, const MulticlockSequenceExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClockedSeqCollection(std::ostream &out, const ClockedSeqCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSimpleExprCollection(std::ostream &out, const SimpleExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstantCollection(std::ostream &out, const ConstantCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLetExprCollection(std::ostream &out, const LetExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printOperationCollection(std::ostream &out, const OperationCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRefObjCollection(std::ostream &out, const RefObjCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRefInstanceCollection(std::ostream &out, const RefInstanceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRefTypespecCollection(std::ostream &out, const RefTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSelectCollection(std::ostream &out, const SelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPartSelectCollection(std::ostream &out, const PartSelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIndexedPartSelectCollection(std::ostream &out, const IndexedPartSelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printVarSelectCollection(std::ostream &out, const VarSelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBitSelectCollection(std::ostream &out, const BitSelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printVariableCollection(std::ostream &out, const VariableCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printHierPathCollection(std::ostream &out, const HierPathCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printArrayExprCollection(std::ostream &out, const ArrayExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRegArrayCollection(std::ostream &out, const RegArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRegCollection(std::ostream &out, const RegCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskFuncCollection(std::ostream &out, const TaskFuncCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskCollection(std::ostream &out, const TaskCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printFunctionCollection(std::ostream &out, const FunctionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskFuncDeclCollection(std::ostream &out, const TaskFuncDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskDeclCollection(std::ostream &out, const TaskDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printFunctionDeclCollection(std::ostream &out, const FunctionDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModportCollection(std::ostream &out, const ModportCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInterfaceTFDeclCollection(std::ostream &out, const InterfaceTFDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printContAssignCollection(std::ostream &out, const ContAssignCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printContAssignBitCollection(std::ostream &out, const ContAssignBitCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPortsCollection(std::ostream &out, const PortsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPortCollection(std::ostream &out, const PortCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPortBitCollection(std::ostream &out, const PortBitCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCheckerPortCollection(std::ostream &out, const CheckerPortCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCheckerInstPortCollection(std::ostream &out, const CheckerInstPortCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPrimitiveCollection(std::ostream &out, const PrimitiveCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGateCollection(std::ostream &out, const GateCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSwitchTranCollection(std::ostream &out, const SwitchTranCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUdpCollection(std::ostream &out, const UdpCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModPathCollection(std::ostream &out, const ModPathCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTchkCollection(std::ostream &out, const TchkCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRangeCollection(std::ostream &out, const RangeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUdpDefnCollection(std::ostream &out, const UdpDefnCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTableEntryCollection(std::ostream &out, const TableEntryCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIODeclCollection(std::ostream &out, const IODeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAliasCollection(std::ostream &out, const AliasCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClockingBlockCollection(std::ostream &out, const ClockingBlockCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClockingIODeclCollection(std::ostream &out, const ClockingIODeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverageOptionCollection(std::ostream &out, const CoverageOptionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverBinCollection(std::ostream &out, const CoverBinCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverPointCollection(std::ostream &out, const CoverPointCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverCrossCollection(std::ostream &out, const CoverCrossCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverGroupCollection(std::ostream &out, const CoverGroupCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printParamAssignCollection(std::ostream &out, const ParamAssignCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInstanceArrayCollection(std::ostream &out, const InstanceArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInterfaceArrayCollection(std::ostream &out, const InterfaceArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printProgramArrayCollection(std::ostream &out, const ProgramArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModuleArrayCollection(std::ostream &out, const ModuleArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPrimitiveArrayCollection(std::ostream &out, const PrimitiveArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGateArrayCollection(std::ostream &out, const GateArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSwitchArrayCollection(std::ostream &out, const SwitchArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUdpArrayCollection(std::ostream &out, const UdpArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTypespecCollection(std::ostream &out, const TypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPrimTermCollection(std::ostream &out, const PrimTermCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPathTermCollection(std::ostream &out, const PathTermCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTchkTermCollection(std::ostream &out, const TchkTermCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printNetCollection(std::ostream &out, const NetCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEventTypespecCollection(std::ostream &out, const EventTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printNamedEventCollection(std::ostream &out, const NamedEventCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printNamedEventArrayCollection(std::ostream &out, const NamedEventArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printParameterCollection(std::ostream &out, const ParameterCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDefParamCollection(std::ostream &out, const DefParamCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSpecParamCollection(std::ostream &out, const SpecParamCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClassTypespecCollection(std::ostream &out, const ClassTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printExtendsCollection(std::ostream &out, const ExtendsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImplementsCollection(std::ostream &out, const ImplementsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClassDefnCollection(std::ostream &out, const ClassDefnCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClassObjCollection(std::ostream &out, const ClassObjCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInstanceCollection(std::ostream &out, const InstanceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInterfaceCollection(std::ostream &out, const InterfaceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printProgramCollection(std::ostream &out, const ProgramCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPackageCollection(std::ostream &out, const PackageCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModuleCollection(std::ostream &out, const ModuleCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCheckerDeclCollection(std::ostream &out, const CheckerDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCheckerInstCollection(std::ostream &out, const CheckerInstCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBindDirectiveCollection(std::ostream &out, const BindDirectiveCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printShortRealTypespecCollection(std::ostream &out, const ShortRealTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRealTypespecCollection(std::ostream &out, const RealTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printByteTypespecCollection(std::ostream &out, const ByteTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printShortIntTypespecCollection(std::ostream &out, const ShortIntTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIntTypespecCollection(std::ostream &out, const IntTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLongIntTypespecCollection(std::ostream &out, const LongIntTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIntegerTypespecCollection(std::ostream &out, const IntegerTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTimeTypespecCollection(std::ostream &out, const TimeTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEnumTypespecCollection(std::ostream &out, const EnumTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printStringTypespecCollection(std::ostream &out, const StringTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTypedefTypespecCollection(std::ostream &out, const TypedefTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printChandleTypespecCollection(std::ostream &out, const ChandleTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModuleTypespecCollection(std::ostream &out, const ModuleTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printProgramTypespecCollection(std::ostream &out, const ProgramTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUdpDefnTypespecCollection(std::ostream &out, const UdpDefnTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printStructTypespecCollection(std::ostream &out, const StructTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUnionTypespecCollection(std::ostream &out, const UnionTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLogicTypespecCollection(std::ostream &out, const LogicTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printArrayTypespecCollection(std::ostream &out, const ArrayTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPackageTypespecCollection(std::ostream &out, const PackageTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printVoidTypespecCollection(std::ostream &out, const VoidTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUnsupportedTypespecCollection(std::ostream &out, const UnsupportedTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSequenceTypespecCollection(std::ostream &out, const SequenceTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropertyTypespecCollection(std::ostream &out, const PropertyTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInterfaceTypespecCollection(std::ostream &out, const InterfaceTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTypeParameterCollection(std::ostream &out, const TypeParameterCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTypespecMemberCollection(std::ostream &out, const TypespecMemberCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEnumConstCollection(std::ostream &out, const EnumConstCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBitTypespecCollection(std::ostream &out, const BitTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTFCallCollection(std::ostream &out, const TFCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUserSystfCollection(std::ostream &out, const UserSystfCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSysFuncCallCollection(std::ostream &out, const SysFuncCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSysTaskCallCollection(std::ostream &out, const SysTaskCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printMethodFuncCallCollection(std::ostream &out, const MethodFuncCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printMethodTaskCallCollection(std::ostream &out, const MethodTaskCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printFuncCallCollection(std::ostream &out, const FuncCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskCallCollection(std::ostream &out, const TaskCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstraintExprCollection(std::ostream &out, const ConstraintExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstraintOrderingCollection(std::ostream &out, const ConstraintOrderingCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstraintCollection(std::ostream &out, const ConstraintCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImportTypespecCollection(std::ostream &out, const ImportTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDistItemCollection(std::ostream &out, const DistItemCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDistributionCollection(std::ostream &out, const DistributionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImplicationCollection(std::ostream &out, const ImplicationCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstrIfCollection(std::ostream &out, const ConstrIfCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstrIfElseCollection(std::ostream &out, const ConstrIfElseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstrForeachCollection(std::ostream &out, const ConstrForeachCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSoftDisableCollection(std::ostream &out, const SoftDisableCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUniquenessCollection(std::ostream &out, const UniquenessCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenStmtCollection(std::ostream &out, const GenStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenIfCollection(std::ostream &out, const GenIfCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenIfElseCollection(std::ostream &out, const GenIfElseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenForCollection(std::ostream &out, const GenForCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenCaseCollection(std::ostream &out, const GenCaseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenRegionCollection(std::ostream &out, const GenRegionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClauseCollection(std::ostream &out, const ClauseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConfigRuleCollection(std::ostream &out, const ConfigRuleCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConfigDeclCollection(std::ostream &out, const ConfigDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLibraryCollection(std::ostream &out, const LibraryCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIncludeStmtCollection(std::ostream &out, const IncludeStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDesignCollection(std::ostream &out, const DesignCollection &collection, int32_t indent, std::string_view relation) override;
  // clang-format on
};

class UhdmJsonPrinter final : public UhdmPrinter {
 public:
  explicit UhdmJsonPrinter(std::ostream &out);
  ~UhdmJsonPrinter() override;

 private:
  std::ostream &beginPrintAny(std::ostream &out, const Any *any, size_t &indent, std::string_view relation,
                              bool shallowVisit) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, bool value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, int32_t value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, uint32_t value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, std::string_view value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, const s_vpi_value *value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, s_vpi_delay *delay,
                              size_t indent) override;
  std::ostream &endPrintAny(std::ostream &out, const Any *any, size_t &indent, std::string_view relation,
                            bool shallowVisit) override;
  template <typename T, typename = std::enable_if_t<std::is_base_of_v<Any, T>>>
  std::ostream &printCollection(std::ostream &out, const std::vector<T *> &collection, size_t indent,
                                std::string_view relation);

  // clang-format off
  std::ostream &printAnyCollection(std::ostream &out, const AnyCollection &collection, size_t indent, std::string_view relation) override;
  std::ostream &printAttributeCollection(std::ostream &out, const AttributeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIdentifierCollection(std::ostream &out, const IdentifierCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCommentCollection(std::ostream &out, const CommentCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLetDeclCollection(std::ostream &out, const LetDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConcurrentAssertionsCollection(std::ostream &out, const ConcurrentAssertionsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printProcessCollection(std::ostream &out, const ProcessCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAlwaysCollection(std::ostream &out, const AlwaysCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printFinalStmtCollection(std::ostream &out, const FinalStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInitialCollection(std::ostream &out, const InitialCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAtomicStmtCollection(std::ostream &out, const AtomicStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDelayControlCollection(std::ostream &out, const DelayControlCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDelayTermCollection(std::ostream &out, const DelayTermCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEventControlCollection(std::ostream &out, const EventControlCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRepeatControlCollection(std::ostream &out, const RepeatControlCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printScopeCollection(std::ostream &out, const ScopeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBeginCollection(std::ostream &out, const BeginCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForkStmtCollection(std::ostream &out, const ForkStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForStmtCollection(std::ostream &out, const ForStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIfStmtCollection(std::ostream &out, const IfStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEventStmtCollection(std::ostream &out, const EventStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printThreadCollection(std::ostream &out, const ThreadCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForeverStmtCollection(std::ostream &out, const ForeverStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printWaitsCollection(std::ostream &out, const WaitsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printWaitStmtCollection(std::ostream &out, const WaitStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printWaitForkCollection(std::ostream &out, const WaitForkCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printOrderedWaitCollection(std::ostream &out, const OrderedWaitCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDisablesCollection(std::ostream &out, const DisablesCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDisableCollection(std::ostream &out, const DisableCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDisableForkCollection(std::ostream &out, const DisableForkCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printContinueStmtCollection(std::ostream &out, const ContinueStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBreakStmtCollection(std::ostream &out, const BreakStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printReturnStmtCollection(std::ostream &out, const ReturnStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printWhileStmtCollection(std::ostream &out, const WhileStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRepeatCollection(std::ostream &out, const RepeatCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDoWhileCollection(std::ostream &out, const DoWhileCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIfElseCollection(std::ostream &out, const IfElseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCaseStmtCollection(std::ostream &out, const CaseStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForceCollection(std::ostream &out, const ForceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAssignStmtCollection(std::ostream &out, const AssignStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDeassignCollection(std::ostream &out, const DeassignCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printReleaseCollection(std::ostream &out, const ReleaseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printNullStmtCollection(std::ostream &out, const NullStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printExpectStmtCollection(std::ostream &out, const ExpectStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForeachStmtCollection(std::ostream &out, const ForeachStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenScopeCollection(std::ostream &out, const GenScopeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenScopeArrayCollection(std::ostream &out, const GenScopeArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAssertCollection(std::ostream &out, const AssertCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverCollection(std::ostream &out, const CoverCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAssumeCollection(std::ostream &out, const AssumeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRestrictCollection(std::ostream &out, const RestrictCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImmediateAssertCollection(std::ostream &out, const ImmediateAssertCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImmediateAssumeCollection(std::ostream &out, const ImmediateAssumeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImmediateCoverCollection(std::ostream &out, const ImmediateCoverCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printExprCollection(std::ostream &out, const ExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCaseItemCollection(std::ostream &out, const CaseItemCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAssignmentCollection(std::ostream &out, const AssignmentCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAnyPatternCollection(std::ostream &out, const AnyPatternCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaggedPatternCollection(std::ostream &out, const TaggedPatternCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printStructPatternCollection(std::ostream &out, const StructPatternCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUnsupportedExprCollection(std::ostream &out, const UnsupportedExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUnsupportedStmtCollection(std::ostream &out, const UnsupportedStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPreprocMacroDefinitionCollection(std::ostream &out, const PreprocMacroDefinitionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPreprocMacroInstanceCollection(std::ostream &out, const PreprocMacroInstanceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSourceFileCollection(std::ostream &out, const SourceFileCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSequenceInstCollection(std::ostream &out, const SequenceInstCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSeqFormalDeclCollection(std::ostream &out, const SeqFormalDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSequenceDeclCollection(std::ostream &out, const SequenceDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropFormalDeclCollection(std::ostream &out, const PropFormalDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropertyInstCollection(std::ostream &out, const PropertyInstCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropertySpecCollection(std::ostream &out, const PropertySpecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropertyDeclCollection(std::ostream &out, const PropertyDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClockedPropertyCollection(std::ostream &out, const ClockedPropertyCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCasePropertyItemCollection(std::ostream &out, const CasePropertyItemCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCasePropertyCollection(std::ostream &out, const CasePropertyCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printMulticlockSequenceExprCollection(std::ostream &out, const MulticlockSequenceExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClockedSeqCollection(std::ostream &out, const ClockedSeqCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSimpleExprCollection(std::ostream &out, const SimpleExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstantCollection(std::ostream &out, const ConstantCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLetExprCollection(std::ostream &out, const LetExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printOperationCollection(std::ostream &out, const OperationCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRefObjCollection(std::ostream &out, const RefObjCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRefInstanceCollection(std::ostream &out, const RefInstanceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRefTypespecCollection(std::ostream &out, const RefTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSelectCollection(std::ostream &out, const SelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPartSelectCollection(std::ostream &out, const PartSelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIndexedPartSelectCollection(std::ostream &out, const IndexedPartSelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printVarSelectCollection(std::ostream &out, const VarSelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBitSelectCollection(std::ostream &out, const BitSelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printVariableCollection(std::ostream &out, const VariableCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printHierPathCollection(std::ostream &out, const HierPathCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printArrayExprCollection(std::ostream &out, const ArrayExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRegArrayCollection(std::ostream &out, const RegArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRegCollection(std::ostream &out, const RegCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskFuncCollection(std::ostream &out, const TaskFuncCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskCollection(std::ostream &out, const TaskCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printFunctionCollection(std::ostream &out, const FunctionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskFuncDeclCollection(std::ostream &out, const TaskFuncDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskDeclCollection(std::ostream &out, const TaskDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printFunctionDeclCollection(std::ostream &out, const FunctionDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModportCollection(std::ostream &out, const ModportCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInterfaceTFDeclCollection(std::ostream &out, const InterfaceTFDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printContAssignCollection(std::ostream &out, const ContAssignCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printContAssignBitCollection(std::ostream &out, const ContAssignBitCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPortsCollection(std::ostream &out, const PortsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPortCollection(std::ostream &out, const PortCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPortBitCollection(std::ostream &out, const PortBitCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCheckerPortCollection(std::ostream &out, const CheckerPortCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCheckerInstPortCollection(std::ostream &out, const CheckerInstPortCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPrimitiveCollection(std::ostream &out, const PrimitiveCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGateCollection(std::ostream &out, const GateCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSwitchTranCollection(std::ostream &out, const SwitchTranCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUdpCollection(std::ostream &out, const UdpCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModPathCollection(std::ostream &out, const ModPathCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTchkCollection(std::ostream &out, const TchkCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRangeCollection(std::ostream &out, const RangeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUdpDefnCollection(std::ostream &out, const UdpDefnCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTableEntryCollection(std::ostream &out, const TableEntryCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIODeclCollection(std::ostream &out, const IODeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAliasCollection(std::ostream &out, const AliasCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClockingBlockCollection(std::ostream &out, const ClockingBlockCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClockingIODeclCollection(std::ostream &out, const ClockingIODeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverageOptionCollection(std::ostream &out, const CoverageOptionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverBinCollection(std::ostream &out, const CoverBinCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverPointCollection(std::ostream &out, const CoverPointCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverCrossCollection(std::ostream &out, const CoverCrossCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverGroupCollection(std::ostream &out, const CoverGroupCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printParamAssignCollection(std::ostream &out, const ParamAssignCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInstanceArrayCollection(std::ostream &out, const InstanceArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInterfaceArrayCollection(std::ostream &out, const InterfaceArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printProgramArrayCollection(std::ostream &out, const ProgramArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModuleArrayCollection(std::ostream &out, const ModuleArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPrimitiveArrayCollection(std::ostream &out, const PrimitiveArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGateArrayCollection(std::ostream &out, const GateArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSwitchArrayCollection(std::ostream &out, const SwitchArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUdpArrayCollection(std::ostream &out, const UdpArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTypespecCollection(std::ostream &out, const TypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPrimTermCollection(std::ostream &out, const PrimTermCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPathTermCollection(std::ostream &out, const PathTermCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTchkTermCollection(std::ostream &out, const TchkTermCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printNetCollection(std::ostream &out, const NetCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEventTypespecCollection(std::ostream &out, const EventTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printNamedEventCollection(std::ostream &out, const NamedEventCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printNamedEventArrayCollection(std::ostream &out, const NamedEventArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printParameterCollection(std::ostream &out, const ParameterCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDefParamCollection(std::ostream &out, const DefParamCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSpecParamCollection(std::ostream &out, const SpecParamCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClassTypespecCollection(std::ostream &out, const ClassTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printExtendsCollection(std::ostream &out, const ExtendsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImplementsCollection(std::ostream &out, const ImplementsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClassDefnCollection(std::ostream &out, const ClassDefnCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClassObjCollection(std::ostream &out, const ClassObjCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInstanceCollection(std::ostream &out, const InstanceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInterfaceCollection(std::ostream &out, const InterfaceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printProgramCollection(std::ostream &out, const ProgramCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPackageCollection(std::ostream &out, const PackageCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModuleCollection(std::ostream &out, const ModuleCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCheckerDeclCollection(std::ostream &out, const CheckerDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCheckerInstCollection(std::ostream &out, const CheckerInstCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBindDirectiveCollection(std::ostream &out, const BindDirectiveCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printShortRealTypespecCollection(std::ostream &out, const ShortRealTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRealTypespecCollection(std::ostream &out, const RealTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printByteTypespecCollection(std::ostream &out, const ByteTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printShortIntTypespecCollection(std::ostream &out, const ShortIntTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIntTypespecCollection(std::ostream &out, const IntTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLongIntTypespecCollection(std::ostream &out, const LongIntTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIntegerTypespecCollection(std::ostream &out, const IntegerTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTimeTypespecCollection(std::ostream &out, const TimeTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEnumTypespecCollection(std::ostream &out, const EnumTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printStringTypespecCollection(std::ostream &out, const StringTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTypedefTypespecCollection(std::ostream &out, const TypedefTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printChandleTypespecCollection(std::ostream &out, const ChandleTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModuleTypespecCollection(std::ostream &out, const ModuleTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printProgramTypespecCollection(std::ostream &out, const ProgramTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUdpDefnTypespecCollection(std::ostream &out, const UdpDefnTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printStructTypespecCollection(std::ostream &out, const StructTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUnionTypespecCollection(std::ostream &out, const UnionTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLogicTypespecCollection(std::ostream &out, const LogicTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printArrayTypespecCollection(std::ostream &out, const ArrayTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPackageTypespecCollection(std::ostream &out, const PackageTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printVoidTypespecCollection(std::ostream &out, const VoidTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUnsupportedTypespecCollection(std::ostream &out, const UnsupportedTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSequenceTypespecCollection(std::ostream &out, const SequenceTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropertyTypespecCollection(std::ostream &out, const PropertyTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInterfaceTypespecCollection(std::ostream &out, const InterfaceTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTypeParameterCollection(std::ostream &out, const TypeParameterCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTypespecMemberCollection(std::ostream &out, const TypespecMemberCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEnumConstCollection(std::ostream &out, const EnumConstCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBitTypespecCollection(std::ostream &out, const BitTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTFCallCollection(std::ostream &out, const TFCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUserSystfCollection(std::ostream &out, const UserSystfCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSysFuncCallCollection(std::ostream &out, const SysFuncCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSysTaskCallCollection(std::ostream &out, const SysTaskCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printMethodFuncCallCollection(std::ostream &out, const MethodFuncCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printMethodTaskCallCollection(std::ostream &out, const MethodTaskCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printFuncCallCollection(std::ostream &out, const FuncCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskCallCollection(std::ostream &out, const TaskCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstraintExprCollection(std::ostream &out, const ConstraintExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstraintOrderingCollection(std::ostream &out, const ConstraintOrderingCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstraintCollection(std::ostream &out, const ConstraintCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImportTypespecCollection(std::ostream &out, const ImportTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDistItemCollection(std::ostream &out, const DistItemCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDistributionCollection(std::ostream &out, const DistributionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImplicationCollection(std::ostream &out, const ImplicationCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstrIfCollection(std::ostream &out, const ConstrIfCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstrIfElseCollection(std::ostream &out, const ConstrIfElseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstrForeachCollection(std::ostream &out, const ConstrForeachCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSoftDisableCollection(std::ostream &out, const SoftDisableCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUniquenessCollection(std::ostream &out, const UniquenessCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenStmtCollection(std::ostream &out, const GenStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenIfCollection(std::ostream &out, const GenIfCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenIfElseCollection(std::ostream &out, const GenIfElseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenForCollection(std::ostream &out, const GenForCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenCaseCollection(std::ostream &out, const GenCaseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenRegionCollection(std::ostream &out, const GenRegionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClauseCollection(std::ostream &out, const ClauseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConfigRuleCollection(std::ostream &out, const ConfigRuleCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConfigDeclCollection(std::ostream &out, const ConfigDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLibraryCollection(std::ostream &out, const LibraryCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIncludeStmtCollection(std::ostream &out, const IncludeStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDesignCollection(std::ostream &out, const DesignCollection &collection, int32_t indent, std::string_view relation) override;
  // clang-format on

 private:
  std::ostream &printPending(std::ostream &out);

 private:
  std::string m_pending;
};

class UhdmXmlPrinter final : public UhdmPrinter {
 public:
  explicit UhdmXmlPrinter(std::ostream &out);
  ~UhdmXmlPrinter() override;

 private:
  std::ostream &beginPrintAny(std::ostream &out, const Any *any, size_t &indent, std::string_view relation,
                              bool shallowVisit) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, bool value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, int32_t value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, uint32_t value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, std::string_view value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, const s_vpi_value *value,
                              size_t indent) override;
  std::ostream &printProperty(std::ostream &out, int32_t relation, std::string_view name, s_vpi_delay *delay,
                              size_t indent) override;
  std::ostream &endPrintAny(std::ostream &out, const Any *any, size_t &indent, std::string_view relation,
                            bool shallowVisit) override;
  template <typename T, typename = std::enable_if_t<std::is_base_of_v<Any, T>>>
  std::ostream &printCollection(std::ostream &out, const std::vector<T *> &collection, size_t indent,
                                std::string_view relation);

  // clang-format off
  std::ostream &printAnyCollection(std::ostream &out, const AnyCollection &collection, size_t indent, std::string_view relation) override;
  std::ostream &printAttributeCollection(std::ostream &out, const AttributeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIdentifierCollection(std::ostream &out, const IdentifierCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCommentCollection(std::ostream &out, const CommentCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLetDeclCollection(std::ostream &out, const LetDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConcurrentAssertionsCollection(std::ostream &out, const ConcurrentAssertionsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printProcessCollection(std::ostream &out, const ProcessCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAlwaysCollection(std::ostream &out, const AlwaysCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printFinalStmtCollection(std::ostream &out, const FinalStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInitialCollection(std::ostream &out, const InitialCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAtomicStmtCollection(std::ostream &out, const AtomicStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDelayControlCollection(std::ostream &out, const DelayControlCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDelayTermCollection(std::ostream &out, const DelayTermCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEventControlCollection(std::ostream &out, const EventControlCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRepeatControlCollection(std::ostream &out, const RepeatControlCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printScopeCollection(std::ostream &out, const ScopeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBeginCollection(std::ostream &out, const BeginCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForkStmtCollection(std::ostream &out, const ForkStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForStmtCollection(std::ostream &out, const ForStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIfStmtCollection(std::ostream &out, const IfStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEventStmtCollection(std::ostream &out, const EventStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printThreadCollection(std::ostream &out, const ThreadCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForeverStmtCollection(std::ostream &out, const ForeverStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printWaitsCollection(std::ostream &out, const WaitsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printWaitStmtCollection(std::ostream &out, const WaitStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printWaitForkCollection(std::ostream &out, const WaitForkCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printOrderedWaitCollection(std::ostream &out, const OrderedWaitCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDisablesCollection(std::ostream &out, const DisablesCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDisableCollection(std::ostream &out, const DisableCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDisableForkCollection(std::ostream &out, const DisableForkCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printContinueStmtCollection(std::ostream &out, const ContinueStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBreakStmtCollection(std::ostream &out, const BreakStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printReturnStmtCollection(std::ostream &out, const ReturnStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printWhileStmtCollection(std::ostream &out, const WhileStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRepeatCollection(std::ostream &out, const RepeatCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDoWhileCollection(std::ostream &out, const DoWhileCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIfElseCollection(std::ostream &out, const IfElseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCaseStmtCollection(std::ostream &out, const CaseStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForceCollection(std::ostream &out, const ForceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAssignStmtCollection(std::ostream &out, const AssignStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDeassignCollection(std::ostream &out, const DeassignCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printReleaseCollection(std::ostream &out, const ReleaseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printNullStmtCollection(std::ostream &out, const NullStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printExpectStmtCollection(std::ostream &out, const ExpectStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printForeachStmtCollection(std::ostream &out, const ForeachStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenScopeCollection(std::ostream &out, const GenScopeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenScopeArrayCollection(std::ostream &out, const GenScopeArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAssertCollection(std::ostream &out, const AssertCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverCollection(std::ostream &out, const CoverCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAssumeCollection(std::ostream &out, const AssumeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRestrictCollection(std::ostream &out, const RestrictCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImmediateAssertCollection(std::ostream &out, const ImmediateAssertCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImmediateAssumeCollection(std::ostream &out, const ImmediateAssumeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImmediateCoverCollection(std::ostream &out, const ImmediateCoverCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printExprCollection(std::ostream &out, const ExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCaseItemCollection(std::ostream &out, const CaseItemCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAssignmentCollection(std::ostream &out, const AssignmentCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAnyPatternCollection(std::ostream &out, const AnyPatternCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaggedPatternCollection(std::ostream &out, const TaggedPatternCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printStructPatternCollection(std::ostream &out, const StructPatternCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUnsupportedExprCollection(std::ostream &out, const UnsupportedExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUnsupportedStmtCollection(std::ostream &out, const UnsupportedStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPreprocMacroDefinitionCollection(std::ostream &out, const PreprocMacroDefinitionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPreprocMacroInstanceCollection(std::ostream &out, const PreprocMacroInstanceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSourceFileCollection(std::ostream &out, const SourceFileCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSequenceInstCollection(std::ostream &out, const SequenceInstCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSeqFormalDeclCollection(std::ostream &out, const SeqFormalDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSequenceDeclCollection(std::ostream &out, const SequenceDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropFormalDeclCollection(std::ostream &out, const PropFormalDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropertyInstCollection(std::ostream &out, const PropertyInstCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropertySpecCollection(std::ostream &out, const PropertySpecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropertyDeclCollection(std::ostream &out, const PropertyDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClockedPropertyCollection(std::ostream &out, const ClockedPropertyCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCasePropertyItemCollection(std::ostream &out, const CasePropertyItemCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCasePropertyCollection(std::ostream &out, const CasePropertyCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printMulticlockSequenceExprCollection(std::ostream &out, const MulticlockSequenceExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClockedSeqCollection(std::ostream &out, const ClockedSeqCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSimpleExprCollection(std::ostream &out, const SimpleExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstantCollection(std::ostream &out, const ConstantCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLetExprCollection(std::ostream &out, const LetExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printOperationCollection(std::ostream &out, const OperationCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRefObjCollection(std::ostream &out, const RefObjCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRefInstanceCollection(std::ostream &out, const RefInstanceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRefTypespecCollection(std::ostream &out, const RefTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSelectCollection(std::ostream &out, const SelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPartSelectCollection(std::ostream &out, const PartSelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIndexedPartSelectCollection(std::ostream &out, const IndexedPartSelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printVarSelectCollection(std::ostream &out, const VarSelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBitSelectCollection(std::ostream &out, const BitSelectCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printVariableCollection(std::ostream &out, const VariableCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printHierPathCollection(std::ostream &out, const HierPathCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printArrayExprCollection(std::ostream &out, const ArrayExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRegArrayCollection(std::ostream &out, const RegArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRegCollection(std::ostream &out, const RegCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskFuncCollection(std::ostream &out, const TaskFuncCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskCollection(std::ostream &out, const TaskCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printFunctionCollection(std::ostream &out, const FunctionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskFuncDeclCollection(std::ostream &out, const TaskFuncDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskDeclCollection(std::ostream &out, const TaskDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printFunctionDeclCollection(std::ostream &out, const FunctionDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModportCollection(std::ostream &out, const ModportCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInterfaceTFDeclCollection(std::ostream &out, const InterfaceTFDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printContAssignCollection(std::ostream &out, const ContAssignCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printContAssignBitCollection(std::ostream &out, const ContAssignBitCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPortsCollection(std::ostream &out, const PortsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPortCollection(std::ostream &out, const PortCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPortBitCollection(std::ostream &out, const PortBitCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCheckerPortCollection(std::ostream &out, const CheckerPortCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCheckerInstPortCollection(std::ostream &out, const CheckerInstPortCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPrimitiveCollection(std::ostream &out, const PrimitiveCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGateCollection(std::ostream &out, const GateCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSwitchTranCollection(std::ostream &out, const SwitchTranCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUdpCollection(std::ostream &out, const UdpCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModPathCollection(std::ostream &out, const ModPathCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTchkCollection(std::ostream &out, const TchkCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRangeCollection(std::ostream &out, const RangeCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUdpDefnCollection(std::ostream &out, const UdpDefnCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTableEntryCollection(std::ostream &out, const TableEntryCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIODeclCollection(std::ostream &out, const IODeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printAliasCollection(std::ostream &out, const AliasCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClockingBlockCollection(std::ostream &out, const ClockingBlockCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClockingIODeclCollection(std::ostream &out, const ClockingIODeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverageOptionCollection(std::ostream &out, const CoverageOptionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverBinCollection(std::ostream &out, const CoverBinCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverPointCollection(std::ostream &out, const CoverPointCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverCrossCollection(std::ostream &out, const CoverCrossCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCoverGroupCollection(std::ostream &out, const CoverGroupCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printParamAssignCollection(std::ostream &out, const ParamAssignCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInstanceArrayCollection(std::ostream &out, const InstanceArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInterfaceArrayCollection(std::ostream &out, const InterfaceArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printProgramArrayCollection(std::ostream &out, const ProgramArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModuleArrayCollection(std::ostream &out, const ModuleArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPrimitiveArrayCollection(std::ostream &out, const PrimitiveArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGateArrayCollection(std::ostream &out, const GateArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSwitchArrayCollection(std::ostream &out, const SwitchArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUdpArrayCollection(std::ostream &out, const UdpArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTypespecCollection(std::ostream &out, const TypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPrimTermCollection(std::ostream &out, const PrimTermCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPathTermCollection(std::ostream &out, const PathTermCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTchkTermCollection(std::ostream &out, const TchkTermCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printNetCollection(std::ostream &out, const NetCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEventTypespecCollection(std::ostream &out, const EventTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printNamedEventCollection(std::ostream &out, const NamedEventCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printNamedEventArrayCollection(std::ostream &out, const NamedEventArrayCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printParameterCollection(std::ostream &out, const ParameterCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDefParamCollection(std::ostream &out, const DefParamCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSpecParamCollection(std::ostream &out, const SpecParamCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClassTypespecCollection(std::ostream &out, const ClassTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printExtendsCollection(std::ostream &out, const ExtendsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImplementsCollection(std::ostream &out, const ImplementsCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClassDefnCollection(std::ostream &out, const ClassDefnCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClassObjCollection(std::ostream &out, const ClassObjCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInstanceCollection(std::ostream &out, const InstanceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInterfaceCollection(std::ostream &out, const InterfaceCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printProgramCollection(std::ostream &out, const ProgramCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPackageCollection(std::ostream &out, const PackageCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModuleCollection(std::ostream &out, const ModuleCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCheckerDeclCollection(std::ostream &out, const CheckerDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printCheckerInstCollection(std::ostream &out, const CheckerInstCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBindDirectiveCollection(std::ostream &out, const BindDirectiveCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printShortRealTypespecCollection(std::ostream &out, const ShortRealTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printRealTypespecCollection(std::ostream &out, const RealTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printByteTypespecCollection(std::ostream &out, const ByteTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printShortIntTypespecCollection(std::ostream &out, const ShortIntTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIntTypespecCollection(std::ostream &out, const IntTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLongIntTypespecCollection(std::ostream &out, const LongIntTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIntegerTypespecCollection(std::ostream &out, const IntegerTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTimeTypespecCollection(std::ostream &out, const TimeTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEnumTypespecCollection(std::ostream &out, const EnumTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printStringTypespecCollection(std::ostream &out, const StringTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTypedefTypespecCollection(std::ostream &out, const TypedefTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printChandleTypespecCollection(std::ostream &out, const ChandleTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printModuleTypespecCollection(std::ostream &out, const ModuleTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printProgramTypespecCollection(std::ostream &out, const ProgramTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUdpDefnTypespecCollection(std::ostream &out, const UdpDefnTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printStructTypespecCollection(std::ostream &out, const StructTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUnionTypespecCollection(std::ostream &out, const UnionTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLogicTypespecCollection(std::ostream &out, const LogicTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printArrayTypespecCollection(std::ostream &out, const ArrayTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPackageTypespecCollection(std::ostream &out, const PackageTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printVoidTypespecCollection(std::ostream &out, const VoidTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUnsupportedTypespecCollection(std::ostream &out, const UnsupportedTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSequenceTypespecCollection(std::ostream &out, const SequenceTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printPropertyTypespecCollection(std::ostream &out, const PropertyTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printInterfaceTypespecCollection(std::ostream &out, const InterfaceTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTypeParameterCollection(std::ostream &out, const TypeParameterCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTypespecMemberCollection(std::ostream &out, const TypespecMemberCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printEnumConstCollection(std::ostream &out, const EnumConstCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printBitTypespecCollection(std::ostream &out, const BitTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTFCallCollection(std::ostream &out, const TFCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUserSystfCollection(std::ostream &out, const UserSystfCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSysFuncCallCollection(std::ostream &out, const SysFuncCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSysTaskCallCollection(std::ostream &out, const SysTaskCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printMethodFuncCallCollection(std::ostream &out, const MethodFuncCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printMethodTaskCallCollection(std::ostream &out, const MethodTaskCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printFuncCallCollection(std::ostream &out, const FuncCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printTaskCallCollection(std::ostream &out, const TaskCallCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstraintExprCollection(std::ostream &out, const ConstraintExprCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstraintOrderingCollection(std::ostream &out, const ConstraintOrderingCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstraintCollection(std::ostream &out, const ConstraintCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImportTypespecCollection(std::ostream &out, const ImportTypespecCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDistItemCollection(std::ostream &out, const DistItemCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDistributionCollection(std::ostream &out, const DistributionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printImplicationCollection(std::ostream &out, const ImplicationCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstrIfCollection(std::ostream &out, const ConstrIfCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstrIfElseCollection(std::ostream &out, const ConstrIfElseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConstrForeachCollection(std::ostream &out, const ConstrForeachCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printSoftDisableCollection(std::ostream &out, const SoftDisableCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printUniquenessCollection(std::ostream &out, const UniquenessCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenStmtCollection(std::ostream &out, const GenStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenIfCollection(std::ostream &out, const GenIfCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenIfElseCollection(std::ostream &out, const GenIfElseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenForCollection(std::ostream &out, const GenForCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenCaseCollection(std::ostream &out, const GenCaseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printGenRegionCollection(std::ostream &out, const GenRegionCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printClauseCollection(std::ostream &out, const ClauseCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConfigRuleCollection(std::ostream &out, const ConfigRuleCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printConfigDeclCollection(std::ostream &out, const ConfigDeclCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printLibraryCollection(std::ostream &out, const LibraryCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printIncludeStmtCollection(std::ostream &out, const IncludeStmtCollection &collection, int32_t indent, std::string_view relation) override;
  std::ostream &printDesignCollection(std::ostream &out, const DesignCollection &collection, int32_t indent, std::string_view relation) override;
  // clang-format on

 private:
  std::ostream &printPending(std::ostream &out);

 private:
  std::string m_pending;
};

using UhdmDefaultPrinter = UHDM_DEFAULT_PRINTER_TYPE;

// Print object(s) to the output stream as a tree
void print_tree(const Any *any, std::ostream &out);
std::string print_tree(const Any *any);

void print_tree(vpiHandle handle, std::ostream &out);
std::string print_tree(vpiHandle handle);

template <typename C>
std::enable_if_t<std::is_base_of_v<Any, std::remove_pointer_t<typename C::value_type>>> print_tree(const C &objects,
                                                                                                   std::ostream &out) {
  if (UhdmPrinter *const printer = new UhdmDefaultPrinter(out)) {
    printer->printTree(objects);
    delete printer;
  }
}

template <typename C>
std::enable_if_t<std::is_convertible_v<typename C::value_type, vpiHandle>> print_tree(const C &handles,
                                                                                      std::ostream &out) {
  if (UhdmPrinter *const printer = new UhdmDefaultPrinter(out)) {
    printer->printTree(handles);
    delete printer;
  }
}

// Print object(s) to the output stream as list
void print_list(const Any *any, std::ostream &out);
std::string print_list(const Any *any);

void print_list(vpiHandle handle, std::ostream &out);
std::string print_list(vpiHandle handle);

template <typename C>
std::enable_if_t<std::is_base_of_v<Any, std::remove_pointer_t<typename C::value_type>>> print_list(const C &objects,
                                                                                                   std::ostream &out) {
  if (UhdmPrinter *const printer = new UhdmDefaultPrinter(out)) {
    printer->printList(objects);
    delete printer;
  }
}

template <typename C>
std::enable_if_t<std::is_convertible_v<typename C::value_type, vpiHandle>> print_list(const C &handles,
                                                                                      std::ostream &out) {
  if (UhdmPrinter *const printer = new UhdmDefaultPrinter(out)) {
    printer->printList(handles);
    delete printer;
  }
}

}  // namespace uhdm

extern "C" {
// For debug use in GDB
void uhdm_print_ids(bool showOrHide);
void uhdm_decompile(const uhdm::Any *any);
void vpi_decompile(vpiHandle handle);
}

#endif  // UHDM_UHDMPRINTER_H
