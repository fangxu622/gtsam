/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */
/**
 * @file 	 NonlinearConstraint.h
 * @brief  
 * @author Duy-Nguyen Ta
 * @date 	 Sep 30, 2013
 */

#pragma once

#include <gtsam/base/Manifold.h>
#include <gtsam/base/numericalDerivative.h>
#include <gtsam/linear/HessianFactor.h>
#include <gtsam/linear/GaussianFactorGraph.h>
#include <gtsam/linear/VectorValues.h>
#include <gtsam/nonlinear/NonlinearFactor.h>

namespace gtsam {

class NonlinearEqualityConstraint {

protected:
  Key dualKey_;
  bool active_;
  
public:
  virtual bool isActive_() const {
    return true; // equality constraints are always active
  }

  void setActive_(bool active_) {
    NonlinearEqualityConstraint::active_ = active_;
  }

  typedef boost::shared_ptr<NonlinearEqualityConstraint> shared_ptr;

  /// Construct with dual key
  NonlinearEqualityConstraint(Key dualKey) :
      dualKey_(dualKey) {
  }

  /**
   * compute the HessianFactor of the (-dual * constraintHessian) for the qp subproblem's objective function
   */
  virtual GaussianFactor::shared_ptr multipliedHessian(const Values& x,
      const VectorValues& duals) const = 0;

  /// return the dual key
  Key dualKey() const {
    return dualKey_;
  }
};

/* ************************************************************************* */
/** A convenient base class for creating a nonlinear equality constraint with 1
 * variables.  To derive from this class, implement evaluateError(). */
template<class VALUE>
class NonlinearEqualityConstraint1: public NoiseModelFactor1<VALUE>,
    public NonlinearEqualityConstraint {

public:

  // typedefs for value types pulled from keys
  typedef VALUE X;

protected:

  typedef NoiseModelFactor1<VALUE> Base;
  typedef NonlinearEqualityConstraint1<VALUE> This;

private:

  static const int X1Dim = traits < VALUE > ::dimension;

public:

  /**
   * Constructor
   * @param j key of the variable
   * @param constraintDim number of dimensions of the constraint error function
   */
  NonlinearEqualityConstraint1(Key key, Key dualKey, size_t constraintDim = 1) :
      Base(noiseModel::Constrained::All(constraintDim), key), NonlinearEqualityConstraint(
          dualKey) {
  }

  virtual ~NonlinearEqualityConstraint1() {
  }

  /**
   *  Override this method to finish implementing a binary factor.
   *  If any of the optional Matrix reference arguments are specified, it should compute
   *  both the function evaluation and its derivative(s) in X1 (and/or X2).
   */
  virtual Vector
  evaluateError(const X&, boost::optional<Matrix&> H1 = boost::none) const = 0;

  /** produce a Gaussian factor graph containing all Hessian Factors, scaled by lambda,
   * corresponding to this constraint */
  virtual GaussianFactor::shared_ptr multipliedHessian(const Values& x,
      const VectorValues& duals) const {
    if (!this->active(x)) {
      return GaussianFactor::shared_ptr();
    }
    const X& x1 = x.at < X > (Base::key());
    const Vector& lambda = duals.at(this->dualKey());

    std::vector < Matrix > G11;
    evaluateHessians(x1, G11);

    if (dim(lambda) != G11.size()) {
      throw std::runtime_error(
          "Error in evaluateHessians: the number of returned Gij matrices must be the same as the constraint dimension!");
    }

    // Combine the Lagrange-multiplier part into this constraint factor
    Matrix lG11sum = zeros(G11[0].rows(), G11[0].cols());
    for (int i = 0; i < lambda.rows(); ++i) {
      lG11sum += -lambda[i] * G11[i];
    }

    HessianFactor::shared_ptr hf(
        new HessianFactor(Base::key(), lG11sum, zero(X1Dim), 100.0));
    return hf;
  }

  /** evaluate Hessians for lambda factors */
  virtual void evaluateHessians(const X& x1, std::vector<Matrix>& G11) const {

    static const bool debug = false;

    boost::function<Vector(const X&)> vecH1(
        boost::bind(&This::vectorizeH1t, this, _1));

    Matrix G11all = numericalDerivative11(vecH1, x1, 1e-5);

    if (debug) {
      gtsam::print(G11all, "G11all: ");
      std::cout << "x1dim: " << X1Dim << std::endl;
    }

    for (size_t i = 0; i < Base::get_noiseModel()->dim(); ++i) {
      G11.push_back(G11all.block(i * X1Dim, 0, X1Dim, X1Dim));
      if (debug)
        gtsam::print(G11[i], "G11: ");
    }
  }

private:
  /** Vectorize the transpose of Jacobian H1 to compute the Hessian numerically */
  Vector vectorizeH1t(const X& x1) const {
    Matrix H1;
    evaluateError(x1, H1);
    Matrix H1t = H1.transpose();
    H1t.resize(H1t.size(), 1);
    return Vector(H1t);
  }

  /** Serialization function */
  friend class boost::serialization::access;
  template<class ARCHIVE>
  void serialize(ARCHIVE & ar, const unsigned int version) {
    ar
        & boost::serialization::make_nvp("NoiseModelFactor",
            boost::serialization::base_object < Base > (*this));
  }
};
// \class NonlinearEqualityConstraint1

/* ************************************************************************* */
/** A convenient base class for creating your own NonlinearConstraint with 2
 * variables.  To derive from this class, implement evaluateError(). */
template<class VALUE1, class VALUE2>
class NonlinearEqualityConstraint2: public NoiseModelFactor2<VALUE1, VALUE2>,
    public NonlinearEqualityConstraint {

public:

  // typedefs for value types pulled from keys
  typedef VALUE1 X1;
  typedef VALUE2 X2;

protected:

  typedef NoiseModelFactor2<VALUE1, VALUE2> Base;
  typedef NonlinearEqualityConstraint2<VALUE1, VALUE2> This;

private:
  static const int X1Dim = traits < VALUE1 > ::dimension;
  static const int X2Dim = traits < VALUE2 > ::dimension;

public:

  /**
   * Constructor
   * @param j1 key of the first variable
   * @param j2 key of the second variable
   * @param constraintDim number of dimensions of the constraint error function
   */
  NonlinearEqualityConstraint2(Key j1, Key j2, Key dualKey,
      size_t constraintDim = 1) :
      Base(noiseModel::Constrained::All(constraintDim), j1, j2), NonlinearEqualityConstraint(
          dualKey) {
  }

  virtual ~NonlinearEqualityConstraint2() {
  }

  /**
   *  Override this method to finish implementing a binary factor.
   *  If any of the optional Matrix reference arguments are specified, it should compute
   *  both the function evaluation and its derivative(s) in X1 (and/or X2).
   */
  virtual Vector
  evaluateError(const X1&, const X2&, boost::optional<Matrix&> H1 = boost::none,
      boost::optional<Matrix&> H2 = boost::none) const = 0;

  /** produce a Gaussian factor graph containing all Hessian Factors, scaled by lambda,
   * corresponding to this constraint */
  virtual GaussianFactor::shared_ptr multipliedHessian(const Values& x,
      const VectorValues& duals) const {
    if (!this->active(x)) {
      return GaussianFactor::shared_ptr();
    }
    const X1& x1 = x.at < X1 > (Base::key1());
    const X2& x2 = x.at < X2 > (Base::key2());
    const Vector& lambda = duals.at(this->dualKey());

    std::vector<Matrix> G11, G12, G22;
    evaluateHessians(x1, x2, G11, G12, G22);

    if (dim(lambda) != G11.size() || dim(lambda) != G12.size()
        || dim(lambda) != G22.size()) {
      throw std::runtime_error(
          "Error in evaluateHessians: the number of returned Gij matrices must be the same as the constraint dimension!");
    }

    // Combine the Lagrange-multiplier part into this constraint factor
    Matrix lG11sum = zeros(G11[0].rows(), G11[0].cols()), lG12sum = zeros(
        G12[0].rows(), G12[0].cols()), lG22sum = zeros(G22[0].rows(),
        G22[0].cols());
    for (int i = 0; i < lambda.rows(); ++i) {
      lG11sum += -lambda[i] * G11[i];
      lG12sum += -lambda[i] * G12[i];
      lG22sum += -lambda[i] * G22[i];
    }

    return boost::make_shared < HessianFactor
        > (Base::key1(), Base::key2(), lG11sum, lG12sum, zero(X1Dim), lG22sum, zero(
            X2Dim), 0.0);
  }

  /** evaluate Hessians for lambda factors */
  virtual void evaluateHessians(const X1& x1, const X2& x2,
      std::vector<Matrix>& G11, std::vector<Matrix>& G12,
      std::vector<Matrix>& G22) const {

    static const bool debug = false;

    boost::function<Vector(const X1&, const X2&)> vecH1(
        boost::bind(&This::vectorizeH1t, this, _1, _2)), vecH2(
        boost::bind(&This::vectorizeH2t, this, _1, _2));

    Matrix G11all = numericalDerivative21(vecH1, x1, x2, 1e-5);
    Matrix G12all = numericalDerivative22(vecH1, x1, x2, 1e-5);
    Matrix G22all = numericalDerivative22(vecH2, x1, x2, 1e-5);

    if (debug) {
      gtsam::print(G11all, "G11all: ");
      gtsam::print(G12all, "G12all: ");
      gtsam::print(G22all, "G22all: ");
      std::cout << "x1dim: " << traits < VALUE1 > ::dimension << std::endl;
      std::cout << "x2dim: " << traits < VALUE2 > ::dimension << std::endl;
    }

    for (size_t i = 0; i < Base::get_noiseModel()->dim(); ++i) {
      G11.push_back(G11all.block(i * X1Dim, 0, X1Dim, X1Dim));
      if (debug)
        gtsam::print(G11[i], "G11: ");

      G12.push_back(G12all.block(i * X1Dim, 0, X1Dim, X2Dim));
      if (debug)
        gtsam::print(G12[i], "G12: ");

      G22.push_back(G22all.block(i * X2Dim, 0, X2Dim, X2Dim));
      if (debug)
        gtsam::print(G22[i], "G22: ");
    }
  }

private:
  /** Vectorize the transpose of Jacobian H1 to compute the Hessian numerically */
  Vector vectorizeH1t(const X1& x1, const X2& x2) const {
    Matrix H1;
    Vector err = evaluateError(x1, x2, H1);
    Matrix H1t = H1.transpose();
    H1t.resize(H1t.size(), 1);
    return Vector(H1t);
  }

  /** Vectorize the transpose of Jacobian H2 to compute the Hessian numerically */
  Vector vectorizeH2t(const X1& x1, const X2& x2) const {
    Matrix H2;
    Vector err = evaluateError(x1, x2, boost::none, H2);
    Matrix H2t = H2.transpose();
    H2t.resize(H2t.size(), 1);
    return Vector(H2t);
  }

  /** Serialization function */
  friend class boost::serialization::access;
  template<class ARCHIVE>
  void serialize(ARCHIVE & ar, const unsigned int version) {
    ar
        & boost::serialization::make_nvp("NoiseModelFactor",
            boost::serialization::base_object < Base > (*this));
  }
};
// \class NonlinearEqualityConstraint2

/* ************************************************************************* */
/** A convenient base class for creating your own NonlinearConstraint with 3
 * variables.  To derive from this class, implement evaluateError(). */
template<class VALUE1, class VALUE2, class VALUE3>
class NonlinearEqualityConstraint3: public NoiseModelFactor3<VALUE1, VALUE2,
    VALUE3>, public NonlinearEqualityConstraint {

public:

  // typedefs for value types pulled from keys
  typedef VALUE1 X1;
  typedef VALUE2 X2;
  typedef VALUE3 X3;

protected:

  typedef NoiseModelFactor3<VALUE1, VALUE2, VALUE3> Base;
  typedef NonlinearEqualityConstraint3<VALUE1, VALUE2, VALUE3> This;

private:
  static const int X1Dim = traits < VALUE1 > ::dimension;
  static const int X2Dim = traits < VALUE2 > ::dimension;
  static const int X3Dim = traits < VALUE3 > ::dimension;

public:

  /**
   * Constructor
   * @param j1 key of the first variable
   * @param j2 key of the second variable
   * @param constraintDim number of dimensions of the constraint error function
   */
  NonlinearEqualityConstraint3(Key j1, Key j2, Key j3, Key dualKey,
      size_t constraintDim = 1) :
      Base(noiseModel::Constrained::All(constraintDim), j1, j2, j3), NonlinearEqualityConstraint(
          dualKey) {
  }

  virtual ~NonlinearEqualityConstraint3() {
  }

  /**
   *  Override this method to finish implementing a binary factor.
   *  If any of the optional Matrix reference arguments are specified, it should compute
   *  both the function evaluation and its derivative(s) in X1 (and/or X2).
   */
  virtual Vector
  evaluateError(const X1&, const X2&, const X3&, boost::optional<Matrix&> H1 =
      boost::none, boost::optional<Matrix&> H2 = boost::none,
      boost::optional<Matrix&> H3 = boost::none) const = 0;

  /** produce a Gaussian factor graph containing all Hessian Factors, scaled by lambda,
   * corresponding to this constraint */
  virtual GaussianFactor::shared_ptr multipliedHessian(const Values& x,
      const VectorValues& duals) const {
    if (!this->active(x)) {
      return GaussianFactor::shared_ptr();
    }
    const X1& x1 = x.at < X1 > (Base::key1());
    const X2& x2 = x.at < X2 > (Base::key2());
    const X3& x3 = x.at < X3 > (Base::key3());
    const Vector& lambda = duals.at(this->dualKey());

    std::vector<Matrix> G11, G12, G13, G22, G23, G33;
    evaluateHessians(x1, x2, x3, G11, G12, G13, G22, G23, G33);

    if (dim(lambda) != G11.size() || dim(lambda) != G12.size()
        || dim(lambda) != G13.size() || dim(lambda) != G22.size()
        || dim(lambda) != G23.size() || dim(lambda) != G33.size()) {
      throw std::runtime_error(
          "Error in evaluateHessians: the number of returned Gij matrices must be the same as the constraint dimension!");
    }

    // Combine the Lagrange-multiplier part into this constraint factor
    Matrix lG11sum = zeros(G11[0].rows(), G11[0].cols()), lG12sum = zeros(
        G12[0].rows(), G12[0].cols()), lG13sum = zeros(G13[0].rows(),
        G13[0].cols()), lG22sum = zeros(G22[0].rows(), G22[0].cols()), lG23sum =
        zeros(G23[0].rows(), G23[0].cols()), lG33sum = zeros(G33[0].rows(),
        G33[0].cols());
    for (int i = 0; i < lambda.rows(); ++i) {
      lG11sum += -lambda[i] * G11[i];
      lG12sum += -lambda[i] * G12[i];
      lG13sum += -lambda[i] * G13[i];
      lG22sum += -lambda[i] * G22[i];
      lG23sum += -lambda[i] * G23[i];
      lG33sum += -lambda[i] * G33[i];
    }

    return boost::shared_ptr < HessianFactor
        > (new HessianFactor(Base::key2(), Base::key2(), Base::key3(), lG11sum,
            lG12sum, lG13sum, zero(X1Dim), lG22sum, lG23sum, zero(X2Dim),
            lG33sum, zero(X3Dim), 0.0));
  }

  /**
   * Default Hessian computation using numerical derivatives
   *
   * As an example, assuming we have f(x1,x2,x3) where dim(f) = 2, dim(x1) = 3, dim(x2) = 2, dim(x3) = 1
   *
   * The Jacobian is:
   *        f1x1 f1x1 f1x1 | f1x2 f1x2 | f1x3
   *        f2x1 f2x1 f2x1 | f2x2 f2x2 | f2x3
   *
   * We transpose it to have the gradients:
   *        f1x1 f2x1
   *        f1x1 f2x1
   *        f1x1 f2x1
   *        f1x2 f2x2
   *        f1x2 f2x2
   *        f1x3 f2x3
   * Then we vectorize this gradient to have:
   *        [f1x1
   *         f1x1
   *         f1x1
   *         f1x2
   *         f1x2
   *         f1x3
   *         f2x1
   *         f2x1
   *         f2x1
   *         f2x2
   *         f2x2
   *         f2x3]
   *
   * The Derivative of this gradient is then:
   *        [f1x1x1 f1x1x1 f1x1x1 | f1x1x2 f1x1x2 | f1x1x3
   *         f1x1x1 f1x1x1 f1x1x1 | f1x1x2 f1x1x2 | f1x1x3
   *         f1x1x1 f1x1x1 f1x1x1 | f1x1x2 f1x1x2 | f1x1x3
   *         ---------------------|---------------|-------
   *         f1x2x1 f1x2x1 f1x2x1 | f1x2x2 f1x2x2 | f1x2x3
   *         f1x2x1 f1x2x1 f1x2x1 | f1x2x2 f1x2x2 | f1x2x3
   *         ---------------------|---------------|-------
   *         f1x3x1 f1x3x1 f1x3x1 | f1x3x2 f1x3x2 | f1x3x3
   *         =============================================
   *         f2x1x1 f2x1x1 f2x1x1 | f2x1x2 f2x1x2 | f2x1x3
   *         f2x1x1 f2x1x1 f2x1x1 | f2x1x2 f2x1x2 | f2x1x3
   *         f2x1x1 f2x1x1 f2x1x1 | f2x1x2 f2x1x2 | f2x1x3
   *         ---------------------|---------------|-------
   *         f2x2x1 f2x2x1 f2x2x1 | f2x2x2 f2x2x2 | f2x2x3
   *         f2x2x1 f2x2x1 f2x2x1 | f2x2x2 f2x2x2 | f2x2x3
   *         ---------------------|---------------|-------
   *         f2x3x1 f2x3x1 f2x3x1 | f2x3x2 f2x3x2 | f2x3x3 ]
   *
   * It is the combination of the Hessian of each component of f
   * stacking on top of each other.
   *
   * */
  virtual void evaluateHessians(const X1& x1, const X2& x2, const X3& x3,
      std::vector<Matrix>& G11, std::vector<Matrix>& G12,
      std::vector<Matrix>& G13, std::vector<Matrix>& G22,
      std::vector<Matrix>& G23, std::vector<Matrix>& G33) const {

    static const bool debug = false;

    boost::function<Vector(const X1&, const X2&, const X3&)> vecH1(
        boost::bind(&This::vectorizeH1t, this, _1, _2, _3)), vecH2(
        boost::bind(&This::vectorizeH2t, this, _1, _2, _3)), vecH3(
        boost::bind(&This::vectorizeH3t, this, _1, _2, _3));

    Matrix G11all = numericalDerivative31(vecH1, x1, x2, x3, 1e-5);
    Matrix G12all = numericalDerivative32(vecH1, x1, x2, x3, 1e-5);
    Matrix G13all = numericalDerivative33(vecH1, x1, x2, x3, 1e-5);
    Matrix G22all = numericalDerivative32(vecH2, x1, x2, x3, 1e-5);
    Matrix G23all = numericalDerivative33(vecH2, x1, x2, x3, 1e-5);
    Matrix G33all = numericalDerivative33(vecH3, x1, x2, x3, 1e-5);

    if (debug) {
      gtsam::print(G11all, "G11all: ");
      gtsam::print(G12all, "G12all: ");
      gtsam::print(G13all, "G13all: ");
      gtsam::print(G22all, "G22all: ");
      gtsam::print(G23all, "G23all: ");
      gtsam::print(G33all, "G33all: ");
      std::cout << "x1dim: " << X1Dim << std::endl;
      std::cout << "x2dim: " << X2Dim << std::endl;
      std::cout << "x3dim: " << X3Dim << std::endl;
    }

    for (size_t i = 0; i < Base::get_noiseModel()->dim(); ++i) {
      G11.push_back(G11all.block(i * X1Dim, 0, X1Dim, X1Dim));
      if (debug)
        gtsam::print(G11[i], "G11: ");

      G12.push_back(G12all.block(i * X1Dim, 0, X1Dim, X2Dim));
      if (debug)
        gtsam::print(G12[i], "G12: ");

      G13.push_back(G13all.block(i * X1Dim, 0, X1Dim, X3Dim));
      if (debug)
        gtsam::print(G13[i], "G13: ");

      G22.push_back(G22all.block(i * X2Dim, 0, X2Dim, X2Dim));
      if (debug)
        gtsam::print(G22[i], "G22: ");

      G23.push_back(G23all.block(i * X2Dim, 0, X2Dim, X3Dim));
      if (debug)
        gtsam::print(G23[i], "G23: ");

      G33.push_back(G33all.block(i * X3Dim, 0, X3Dim, X3Dim));
      if (debug)
        gtsam::print(G33[i], "G33: ");
    }
  }

private:
  /** Vectorize the transpose of Jacobian H1 to compute the Hessian numerically */
  Vector vectorizeH1t(const X1& x1, const X2& x2, const X3& x3) const {
    Matrix H1;
    Vector err = evaluateError(x1, x2, x3, H1);
    Matrix H1t = H1.transpose();
    H1t.resize(H1t.size(), 1);
    return Vector(H1t);
  }

  /** Vectorize the transpose of Jacobian H2 to compute the Hessian numerically */
  Vector vectorizeH2t(const X1& x1, const X2& x2, const X3& x3) const {
    Matrix H2;
    Vector err = evaluateError(x1, x2, x3, boost::none, H2);
    Matrix H2t = H2.transpose();
    H2t.resize(H2t.size(), 1);
    return Vector(H2t);
  }

  /** Vectorize the transpose of Jacobian H3 to compute the Hessian numerically */
  Vector vectorizeH3t(const X1& x1, const X2& x2, const X3& x3) const {
    Matrix H3;
    Vector err = evaluateError(x1, x2, x3, boost::none, boost::none, H3);
    Matrix H3t = H3.transpose();
    H3t.resize(H3t.size(), 1);
    return Vector(H3t);
  }

  /** Serialization function */
  friend class boost::serialization::access;
  template<class ARCHIVE>
  void serialize(ARCHIVE & ar, const unsigned int version) {
    ar
        & boost::serialization::make_nvp("NoiseModelFactor",
            boost::serialization::base_object < Base > (*this));
  }
};
// \class NonlinearEqualityConstraint3

} /* namespace gtsam */