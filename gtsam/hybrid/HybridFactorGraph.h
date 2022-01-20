/* ----------------------------------------------------------------------------
 * Copyright 2021 The Ambitious Folks of the MRG
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file   HybridFactorGraph.h
 * @brief  Custom hybrid factor graph for discrete + continuous factors
 * @author Kevin Doherty, kdoherty@mit.edu
 * @date   December 2021
 */

#pragma once

#include <gtsam/discrete/DiscreteFactorGraph.h>
#include <gtsam/hybrid/DCFactorGraph.h>
#include <gtsam/hybrid/DCGaussianMixtureFactor.h>

#include <string>

namespace gtsam {

/**
 * @brief Base class for hybrid factor graphs.
 *
 * @tparam FG The type of factor graph to specialize for, e.g.
 * NonlinearFactorGraph, GaussianFactorGraph.
 */
template <typename FG>
class HybridFactorGraph : protected FactorGraph<Factor> {
 public:
  using shared_ptr = boost::shared_ptr<HybridFactorGraph>;
  using Base = FactorGraph<Factor>;

 protected:
  // Separate internal factor graphs for different types of factors
  FG factorGraph_;
  DiscreteFactorGraph discreteGraph_;
  DCFactorGraph dcGraph_;

  /// Check if FACTOR type is derived from DiscreteFactor.
  template <typename FACTOR>
  using IsDiscrete = typename std::enable_if<
      std::is_base_of<DiscreteFactor, FACTOR>::value>::type;

  /// Check if FACTOR type is derived from DCFactor.
  template <typename FACTOR>
  using IsDC =
      typename std::enable_if<std::is_base_of<DCFactor, FACTOR>::value>::type;

  public:
  /// Default constructor
  HybridFactorGraph() = default;

  /**
   * @brief Construct a new Hybrid Factor Graph object.
   *
   * @param factorGraph A factor graph of type `FG`.
   * @param discreteGraph A factor graph with only discrete factors.
   * @param dcGraph A DCFactorGraph containing DCFactors.
   */
  HybridFactorGraph(const FG& factorGraph,
                    const DiscreteFactorGraph& discreteGraph,
                    const DCFactorGraph& dcGraph)
      : factorGraph_(factorGraph),
        discreteGraph_(discreteGraph),
        dcGraph_(dcGraph) {
    Base::push_back(factorGraph);
    Base::push_back(discreteGraph);
    Base::push_back(dcGraph);
  }

  // Allow use of selected FactorGraph methods:
  using Base::empty;
  using Base::reserve;
  using Base::size;
  using Base::operator[];

  /**
   * Add a discrete factor *pointer* to the internal discrete graph
   * @param discreteFactor - boost::shared_ptr to the factor to add
   */
  template <typename FACTOR>
  IsDiscrete<FACTOR> push_discrete(
      const boost::shared_ptr<FACTOR>& discreteFactor) {
    discreteGraph_.push_back(discreteFactor);
    Base::push_back(discreteFactor);
  }

  /**
   * Add a discrete-continuous (DC) factor *pointer* to the internal DC graph
   * @param dcFactor - boost::shared_ptr to the factor to add
   */
  template <typename FACTOR>
  IsDC<FACTOR> push_dc(const boost::shared_ptr<FACTOR>& dcFactor) {
    dcGraph_.push_back(dcFactor);
    Base::push_back(dcFactor);
  }

  /// delete emplace_shared.
  template <class FACTOR, class... Args>
  void emplace_shared(Args&&... args) = delete;

  /// Construct a factor and add (shared pointer to it) to factor graph.
  template <class FACTOR, class... Args>
  IsDiscrete<FACTOR> emplace_discrete(Args&&... args) {
    auto factor = boost::allocate_shared<FACTOR>(
        Eigen::aligned_allocator<FACTOR>(), std::forward<Args>(args)...);
    push_discrete(factor);
  }

  /// Construct a factor and add (shared pointer to it) to factor graph.
  template <class FACTOR, class... Args>
  IsDC<FACTOR> emplace_dc(Args&&... args) {
    auto factor = boost::allocate_shared<FACTOR>(
        Eigen::aligned_allocator<FACTOR>(), std::forward<Args>(args)...);
    push_dc(factor);
  }

  /**
   * @brief Add a single factor shared pointer to the hybrid factor graph.
   * Dynamically handles the factor type and assigns it to the correct
   * underlying container.
   *
   * @tparam FACTOR The factor type template
   * @param sharedFactor The factor to add to this factor graph.
   */
  template <typename FACTOR>
  void push_back(const boost::shared_ptr<FACTOR>& sharedFactor) {
    if (auto p = boost::dynamic_pointer_cast<DiscreteFactor>(sharedFactor)) {
      push_discrete(p);
    }
    if (auto p = boost::dynamic_pointer_cast<DCFactor>(sharedFactor)) {
      push_dc(p);
    }
  }

  /** Constructor from iterator over factors (shared_ptr or plain objects) */
  template <typename ITERATOR>
  void push_back(ITERATOR firstFactor, ITERATOR lastFactor) {
    for (auto&& it = firstFactor; it != lastFactor; it++) {
      push_back(*it);
    }
  }

  /**
   * Simply prints the factor graph.
   */
  void print(
      const std::string& str = "HybridFactorGraph",
      const KeyFormatter& keyFormatter = DefaultKeyFormatter) const override {
    std::string prefix = str.empty() ? str : str + ".";
    std::cout << prefix << "size: " << size() << std::endl;
    discreteGraph_.print(prefix + "DiscreteFactorGraph", keyFormatter);
    dcGraph_.print(prefix + "DCFactorGraph", keyFormatter);
  }

  /**
   * Utility for retrieving the internal discrete factor graph
   * @return the member variable discreteGraph_
   */
  const gtsam::DiscreteFactorGraph& discreteGraph() const {
    return discreteGraph_;
  }

  /**
   * Utility for retrieving the internal DC factor graph
   * @return the member variable dcGraph_
   */
  const DCFactorGraph& dcGraph() const { return dcGraph_; }

  /**
   * @return true if all internal graphs of `this` are equal to those of
   * `other`
   */
  bool equals(const HybridFactorGraph<FG>& other, double tol = 1e-9) const {
    return Base::equals(other, tol) &&
           discreteGraph_.equals(other.discreteGraph_, tol) &&
           dcGraph_.equals(other.dcGraph_, tol) &&
           factorGraph_.equals(other.factorGraph_, tol);
  }

  /// The total number of factors in the discrete factor graph.
  size_t nrDiscreteFactors() const { return discreteGraph_.size(); }

  /// The total number of factors in the DC factor graph.
  size_t nrDcFactors() const { return dcGraph_.size(); }

  /**
   * Clears all internal factor graphs by creating new instances of them.
   */
  void clear() {
    discreteGraph_ = DiscreteFactorGraph();
    dcGraph_ = DCFactorGraph();
    factorGraph_ = FG();
  }

  /// Get all the discrete keys in the hybrid factor graph.
  virtual DiscreteKeys discreteKeys() const {
    DiscreteKeys result;
    // Discrete keys from the discrete graph.
    result = discreteGraph_.discreteKeys();
    // Discrete keys from the DC factor graph.
    auto dcKeys = dcGraph_.discreteKeys();
    for (auto&& key : dcKeys) {
      // Only insert unique keys
      if (std::find(result.begin(), result.end(), key) == result.end()) {
        result.push_back(key);
      }
    }
    return result;
  }

  using Sum = DCGaussianMixtureFactor::Sum;

  /**
   * @brief Sum all gaussians and Gaussian mixtures together.
   * @return a decision tree of GaussianFactorGraphs
   *
   * Takes all factors, which *must* be all DCGaussianMixtureFactors or
   * GaussianFactors, and "add" them. This might involve decision-trees of
   * different structure, and creating a different decision tree for Gaussians.
   */
  DCGaussianMixtureFactor::Sum sum() const {
    // "sum" all factors, gathering into GaussianFactorGraph
    DCGaussianMixtureFactor::Sum sum;
    for (auto&& dcFactor : dcGraph()) {
      if (auto mixtureFactor =
              boost::dynamic_pointer_cast<DCGaussianMixtureFactor>(dcFactor)) {
        sum += *mixtureFactor;
      } else {
        throw std::runtime_error(
            "HybridFactorGraph::sum can only handle DCGaussianMixtureFactors.");
      }
    }
    return sum;
  }
};

}  // namespace gtsam
