/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#ifndef ANALYSIS_IFDS_IDE_PROBLEMS_IFDS_CONSTANALYSIS_H_
#define ANALYSIS_IFDS_IDE_PROBLEMS_IFDS_CONSTANALYSIS_H_

#include <map>
#include <phasar/PhasarLLVM/IfdsIde/DefaultIFDSTabulationProblem.h>
#include <set>
#include <string>
#include <vector>

// Forward declaration of types for which we only use its pointer or ref type
namespace llvm {
class Instruction;
class Function;
class Value;
} // namespace llvm

namespace psr {
class LLVMBasedICFG;
class PointsToGraph;

/**
 * This IFDS analysis will compute possibly mutable memory
 * locations (stack and heap). LLVM's virtual register
 * are not considered since they are in SSA form.
 * A memory location is considered mutable after the second
 * write access. Thus, the first write access is allowed
 * to account for initialization.
 * @brief Computes all possibly mutable memory locations.
 */
class IFDSConstAnalysis : public DefaultIFDSTabulationProblem<
                              const llvm::Instruction *, const llvm::Value *,
                              const llvm::Function *, LLVMBasedICFG &> {
public:
  typedef const llvm::Value *d_t;
  typedef const llvm::Instruction *n_t;
  typedef const llvm::Function *m_t;
  typedef LLVMBasedICFG &i_t;

private:
  PointsToGraph &ptg;
  std::vector<std::string> EntryPoints;
  /// Holds all initialized variables and objects.
  std::set<d_t> Initialized;

public:
  IFDSConstAnalysis(i_t icfg, std::vector<std::string> EntryPoints = {"main"});

  virtual ~IFDSConstAnalysis() = default;

  /**
   * If the current instruction is a store instruction, the memory locations's
   * state of initialization is checked. If the memory location was already
   * initialized, i.e. at least one write access occurred, the
   * pointer operand is generated as a data-flow fact. Also all aliases that
   * meet the 'context-relevant' requirements (see {@link
   * getContextRelevantPointsToSet}) will be generated!
   *
   * Otherwise, the memory location (i.e. memory location's pointer operand) is
   * marked as initialized.
   *
   * To infer the state of initialization, the memory location's pointer operand
   * and all it's aliases are checked to see if one of them is marked as
   * initialized.
   *
   * Vtable updates are ignored, and thus not counting towards an object's
   * mutability state.
   * @brief Processing store instructions by generating new data-flow facts, if
   * more than one write access to the memory location occurred.
   * @param curr Currently analyzed program statement.
   * @param succ Successor statement.
   */
  std::shared_ptr<FlowFunction<d_t>> getNormalFlowFunction(n_t curr,
                                                           n_t succ) override;

  /**
   * The following llvm intrinsics
   *    \li memcpy
   *    \li memmove
   *    \li memset
   * count as a write access to their target memory location.
   * Since those functions are declaration only and cannot be analyzed
   * directly, the effects are modeled by killing all data-flow facts
   * before the call (at this point), and generate respective data-flow facts in
   * the corresponding call-to-return flow function (see {@link
   * getCallToRetFlowFunction}).
   *
   * Call or invoke instructions are handled by mapping actual parameters into
   * formal parameters, i.e. propagating relevant data-flow facts from the
   * caller into the callee context.
   * @brief Processing call/invoke instructions and llvm memory intrinsic
   * functions.
   * @param callStmt Call statement.
   * @param destMthd Callee function.
   */
  std::shared_ptr<FlowFunction<d_t>> getCallFlowFunction(n_t callStmt,
                                                         m_t destMthd) override;

  /**
   * Maps formal parameters back into actual parameters. Data-flow fact(s)
   * associated with the return value are propagated into the caller context.
   * @brief Processing a function return.
   * @param callSite Call site.
   * @param calleeMthd Callee function.
   * @param exitStmt Exit statement in callee.
   * @param retSite Return site.
   */
  std::shared_ptr<FlowFunction<d_t>> getRetFlowFunction(n_t callSite,
                                                        m_t calleeMthd,
                                                        n_t exitStmt,
                                                        n_t retSite) override;

  /**
   * If the called function is a llvm memory intrinsic function, appropriate
   * data-flow facts are generated at this point. In essence, these functions
   * are handled just as store instructions, i.e. generating new data-flow facts
   * if the target memory location (first operand) is already initialized.
   *
   * Otherwise, all data-flow facts are passed as identity.
   * @brief Processing the effects of llvm memory intrinsic functions.
   * @param callSite Call site.
   * @param retSite Return site.
   */
  std::shared_ptr<FlowFunction<d_t>>
  getCallToRetFlowFunction(n_t callSite, n_t retSite,
                           std::set<m_t> callees) override;

  /**
   * @brief Not used for this analysis, i.e. always returning nullptr.
   */
  std::shared_ptr<FlowFunction<d_t>>
  getSummaryFlowFunction(n_t callStmt, m_t destMthd) override;

  /**
   * Only the zero value is valid at the first program statement, i.e.
   * all memory locations are considered immutable.
   * @brief Provides data-flow facts that are valid at the program entry point.
   */
  std::map<n_t, std::set<d_t>> initialSeeds() override;

  /**
   * @brief Returns appropriate zero value.
   */
  d_t createZeroValue() override;

  bool isZeroValue(d_t d) const override;

  std::string DtoString(d_t d) const override;

  std::string NtoString(n_t n) const override;

  std::string MtoString(m_t m) const override;

  /**
   * @note Global Variables are always intialized in llvm IR, and therefore
   * not part of the Initialized set.
   * @brief Checks if the given memory location is initialized
   */
  bool isInitialized(d_t d) const;

  /**
   * @brief Marks the given memory location as initialized.
   */
  void markAsInitialized(d_t d);

  /**
   * @brief Prints all initialized memory locations.
   */
  void printInitMemoryLocations();

  /**
   * @brief Returns the number of initialized memory locations.
   */
  std::size_t initMemoryLocationCount();

  // clang-format off
  /**
   * We only want/need to generate aliases if they meet one of the
   * following conditions
   * <ol>
   *    <li> alias is an instruction from within the current function context
   *    <li> alias is an allocation instruction for stack memory (alloca) or
   *         heap memory (new, new[], malloc, calloc, realloc) from any function
   *         context
   *    <li> alias is a global variable
   *    <li> alias is a formal argument of the current function
   *    <li> alias is a return value of pointer type
   * </ol>
   *
   * Condition (1) is necessary to cover the case, when an initialized
   * memory location is mutated in a function different from where its
   * original allocation site.<br>
   * Condition (3) is necessary to be able to map mutated parameter
   * back to the caller context if needed. <br>
   * Same goes for (4).
   *
   * Everything else will be ignored since we are not interested in
   * intermediate pointer or values of other functions, i.e. values
   * in virtual registers.
   * Only points-to information and the Initialized set determine, if
   * new data-flow facts will be generated.
   * @brief Refines the given points-to information to only context-relevant
   * points-to information.
   * @param PointsToSet that is refined.
   * @param Context dictates which points-to information is relevant.
   */ // clang-format on
  std::set<d_t> getContextRelevantPointsToSet(std::set<d_t> &PointsToSet,
                                              m_t Context);
};

} // namespace psr

#endif /* ANALYSIS_IFDS_IDE_PROBLEMS_IFDS_CONST_ANALYSIS_IFDSCONSTANALYSIS_H_  \
        */
