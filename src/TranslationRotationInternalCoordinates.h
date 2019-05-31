/**
CAST 3
TranslationRotationInternalCoordinates.h
Purpose: Definition of the Rotation Translation Internal Coordinate System


@author Julian Erdmannsdörfer, Michael Prem
@version 3.0
*/

#ifndef TRANSLATION_ROTATION_INTERNAL_COORDINATES_H
#define TRANSLATION_ROTATION_INTERNAL_COORDINATES_H

#include "PrimitiveInternalCoordinates.h"

namespace internals {

  class TRIC : public PrimitiveInternalCoordinates {
  public:
    TRIC(/*const std::vector<coords::Representation_3D>& res_init,
      const std::vector<std::vector<std::size_t>>& res_index,
      CartesianType & xyz_init, BondGraph const& graph*/) : PrimitiveInternalCoordinates{ /*res_init, res_index, xyz_init, graph*/ } {
      //delocalize_ic_system(xyz_init);
    }


    scon::mathmatrix<coords::float_type>& Bmat(CartesianType const& cartesians) override;//F
    scon::mathmatrix<coords::float_type> transposeOfBmat(CartesianType const& cartesian) override; 
    scon::mathmatrix<coords::float_type> pseudoInverseOfGmat(CartesianType const& cartesian) override;
    scon::mathmatrix<coords::float_type>& Gmat(CartesianType const& cartesians) override;//F
    scon::mathmatrix<coords::float_type>& delocalize_ic_system(CartesianType const& cartesians);//F
    scon::mathmatrix<coords::float_type> guess_hessian(CartesianType const& cartesians) const override;
    scon::mathmatrix<coords::float_type> calc(coords::Representation_3D const& xyz) const override;//F
    scon::mathmatrix<coords::float_type> calc_diff(coords::Representation_3D const& lhs, coords::Representation_3D const& rhs) const override;//F
    virtual scon::mathmatrix<coords::float_type> projectorMatrix(CartesianType const& cartesian) override;

    scon::mathmatrix<coords::float_type> const& getDelMat()const { return del_mat; }
  protected:
    scon::mathmatrix<coords::float_type> del_mat;
  };
}
#endif