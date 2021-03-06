/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#ifndef ZEROFLOWFACT_H_
#define ZEROFLOWFACT_H_

#include <iostream>
#include <phasar/PhasarLLVM/IfdsIde/FlowFact.h>
namespace psr {

class ZeroFlowFact : public FlowFact {
public:
  ~ZeroFlowFact() = default;
  std::ostream &print(std::ostream &os) const override;
  static FlowFact *getInstance();
};

} // namespace psr

#endif
