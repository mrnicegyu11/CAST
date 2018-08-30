#include "TranslationRotationInternalCoordinates.h"

namespace internals {

  scon::mathmatrix<coords::float_type>& TRIC::delocalize_ic_system(CartesianType const& cartesians) {
    using Mat = scon::mathmatrix<coords::float_type>;

    Mat eigval, eigvec;
    std::tie(eigval, eigvec) = PrimitiveInternalCoordinates::Gmat(cartesians).eigensym(false);

    auto row_index_vec = eigval.sort_idx();
    auto col_index_vec = eigval.find_idx([](coords::float_type const & a) {
      return std::abs(a) > 1e-6;
    });

    del_mat = eigvec.submat(row_index_vec, col_index_vec);
    new_B_matrix = new_G_matrix = true; //B and G got to be calculated for the new ic_system
    return del_mat;
  }


  scon::mathmatrix<coords::float_type>& TRIC::Bmat(CartesianType const& cartesians) {
    if (!new_B_matrix) {
      return B_matrix;
    }
    B_matrix = del_mat.t()*PrimitiveInternalCoordinates::Bmat(cartesians);
    new_B_matrix = false;
    return B_matrix;
  }

  scon::mathmatrix<coords::float_type> TRIC::transposeOfBmat(CartesianType const& cartesian) {
    return Bmat(cartesian).t();
  }

  scon::mathmatrix<coords::float_type> TRIC::pseudoInverseOfGmat(CartesianType const& cartesian) {
    return Gmat(cartesian).pinv();
  }

  scon::mathmatrix<coords::float_type>& TRIC::Gmat(CartesianType const& cartesians) {
    if (!new_G_matrix) {
      return G_matrix;
    }
    Bmat(cartesians);
    G_matrix = B_matrix * B_matrix.t();
    new_G_matrix = false;
    return G_matrix;
  }

  scon::mathmatrix<coords::float_type> TRIC::guess_hessian(CartesianType const& cartesians) const {
    return del_mat.t() * PrimitiveInternalCoordinates::guess_hessian(cartesians) * del_mat;
  }

  scon::mathmatrix<coords::float_type> TRIC::calc(coords::Representation_3D const& xyz) const {
    auto prims = PrimitiveInternalCoordinates::calc(xyz);
    return (prims * del_mat).t();
  }

  scon::mathmatrix<coords::float_type> TRIC::calc_diff(coords::Representation_3D const& lhs, coords::Representation_3D const& rhs) const {
    auto diff = PrimitiveInternalCoordinates::calc_diff(lhs, rhs);
    return (diff * del_mat).t();
  }
}
