/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AutoIncrement.h
 *
 * Defines a class for evaluating values in the Relational Algebra Machine
 *
 ************************************************************************/

#pragma once

#include "ram/Expression.h"
#include <ostream>

namespace souffle::ram {

/**
 * @class AutoIncrement
 * @brief Increment a counter and return its value.
 *
 * Note that there exists a single counter only.
 */
class AutoIncrement : public Expression {
public:
    AutoIncrement* clone() const override {
        return new AutoIncrement();
    }

protected:
    void print(std::ostream& os) const override {
        os << "autoinc()";
    }
};

}  // namespace souffle::ram
