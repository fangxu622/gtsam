/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation, 
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file CallRecord.h
 * @date November 21, 2014
 * @author Frank Dellaert
 * @author Paul Furgale
 * @author Hannes Sommer
 * @brief Internals for Expression.h, not for general consumption
 */

#pragma once

#include <gtsam/base/VerticalBlockMatrix.h>
#include <gtsam/base/FastVector.h>
#include <gtsam/base/Matrix.h>

namespace gtsam {

class JacobianMap;
// forward declaration

//-----------------------------------------------------------------------------
/**
 * MaxVirtualStaticRows defines how many separate virtual reverseAD with specific
 * static rows (1..MaxVirtualStaticRows) methods will be part of the CallRecord interface.
 */
const int MaxVirtualStaticRows = 4;

namespace internal {

/**
 * ConvertToDynamicIf converts to a dense matrix with dynamic rows iff
 * ConvertToDynamicRows (colums stay as they are) otherwise
 * it just passes dense Eigen matrices through.
 */
template<bool ConvertToDynamicRows>
struct ConvertToDynamicRowsIf {
  template<typename Derived>
  static Eigen::Matrix<double, Eigen::Dynamic, Derived::ColsAtCompileTime> convert(
      const Eigen::MatrixBase<Derived> & x) {
    return x;
  }
};

template<>
struct ConvertToDynamicRowsIf<false> {
  template<int Rows, int Cols>
  static const Eigen::Matrix<double, Rows, Cols> & convert(
      const Eigen::Matrix<double, Rows, Cols> & x) {
    return x;
  }
};

/**
 * Recursive definition of an interface having several purely
 * virtual _reverseAD(const Eigen::Matrix<double, Rows, Cols> &, JacobianMap&)
 * with Rows in 1..MaxSupportedStaticRows
 */
template<int MaxSupportedStaticRows, int Cols>
struct ReverseADInterface: ReverseADInterface<MaxSupportedStaticRows - 1,
    Cols> {
  using ReverseADInterface<MaxSupportedStaticRows - 1, Cols>::_reverseAD;
  virtual void _reverseAD(
      const Eigen::Matrix<double, MaxSupportedStaticRows, Cols> & dFdT,
      JacobianMap& jacobians) const = 0;
};

template<int Cols>
struct ReverseADInterface<0, Cols> {
  virtual void _reverseAD(
      const Eigen::Matrix<double, Eigen::Dynamic, Cols> & dFdT,
      JacobianMap& jacobians) const = 0;
  virtual void _reverseAD(const Matrix & dFdT,
      JacobianMap& jacobians) const = 0;
};

template<typename Derived, int MaxSupportedStaticRows, int Cols>
struct ReverseADImplementor; // forward for CallRecord's friend declaration
}

/**
 * The CallRecord class stores the Jacobians of applying a function
 * with respect to each of its arguments. It also stores an executation trace
 * (defined below) for each of its arguments.
 *
 * It is implemented in the function-style ExpressionNode's nested Record class below.
 */
template<int Cols>
struct CallRecord: private internal::ReverseADInterface<MaxVirtualStaticRows,
    Cols> {

  inline void print(const std::string& indent) const {
    _print(indent);
  }

  inline void startReverseAD(JacobianMap& jacobians) const {
    _startReverseAD(jacobians);
  }

  template<int Rows>
  inline void reverseAD(const Eigen::Matrix<double, Rows, Cols> & dFdT,
      JacobianMap& jacobians) const {
    _reverseAD(
        internal::ConvertToDynamicRowsIf<(Rows > MaxVirtualStaticRows)>::convert(
            dFdT), jacobians);
  }

  virtual ~CallRecord() {
  }

private:
  virtual void _print(const std::string& indent) const = 0;
  virtual void _startReverseAD(JacobianMap& jacobians) const = 0;
  using internal::ReverseADInterface<MaxVirtualStaticRows,
      Cols>::_reverseAD;
  template<typename Derived, int MaxSupportedStaticRows, int OtherCols>
  friend struct internal::ReverseADImplementor;
};

namespace internal {

/**
 * ReverseADImplementor is a utility class used by CallRecordImplementor to
 * implementing the recursive CallRecord interface.
 */
template<typename Derived, int MaxSupportedStaticRows, int Cols>
struct ReverseADImplementor: ReverseADImplementor<Derived,
    MaxSupportedStaticRows - 1, Cols> {

private:
  using ReverseADImplementor<Derived,
      MaxSupportedStaticRows - 1, Cols>::_reverseAD;
  virtual void _reverseAD(
      const Eigen::Matrix<double, MaxSupportedStaticRows, Cols> & dFdT,
      JacobianMap& jacobians) const {
    static_cast<const Derived *>(this)->reverseAD(dFdT, jacobians);
  }
  friend struct internal::ReverseADImplementor<Derived, MaxSupportedStaticRows + 1, Cols>;
};

template<typename Derived, int Cols>
struct ReverseADImplementor<Derived, 0, Cols> : CallRecord<Cols> {
private:
  using CallRecord<Cols>::_reverseAD;
  const Derived & derived() const {
    return static_cast<const Derived&>(*this);
  }
  virtual void _reverseAD(
      const Eigen::Matrix<double, Eigen::Dynamic, Cols> & dFdT,
      JacobianMap& jacobians) const {
    derived().reverseAD(dFdT, jacobians);
  }
  virtual void _reverseAD(const Matrix & dFdT, JacobianMap& jacobians) const {
    derived().reverseAD(dFdT, jacobians);
  }
  friend struct internal::ReverseADImplementor<Derived, 1, Cols>;
};

/**
 * The CallRecordImplementor implements the CallRecord interface for a Derived class by
 * delegating to its corresponding (templated) non-virtual methods.
 */
template<typename Derived, int Cols>
struct CallRecordImplementor: ReverseADImplementor<Derived,
    MaxVirtualStaticRows, Cols> {
private:
  const Derived & derived() const {
    return static_cast<const Derived&>(*this);
  }
  virtual void _print(const std::string& indent) const {
    derived().print(indent);
  }
  virtual void _startReverseAD(JacobianMap& jacobians) const {
    derived().startReverseAD(jacobians);
  }
};

} // internal

} // gtsam
