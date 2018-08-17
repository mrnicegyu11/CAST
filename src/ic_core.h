////////////////////////////////
// new functions to be tested //
////////////////////////////////
#ifndef cast_ic_core_h_guard
#define cast_ic_core_h_guard

#include "ic_atom.h"
#include "ic_rotation.h"

#include "scon_mathmatrix.h"
#include <algorithm>
#include <array>
#include <boost/graph/adjacency_list.hpp>
#include <cmath>
#include <iterator>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "coords_io_XYZ.h"


namespace coords {
class DL_Coordinates : public Coordinates {
public:
  std::shared_ptr<input::formats::xyz::helper::Parser<float_type>> parser;
  DL_Coordinates(Coordinates const& coords,
                 std::unique_ptr<input::formats::xyz> format)
      : Coordinates(coords) {

    if (!format) {
      throw std::runtime_error(
          "You need to pass a format with fragments i. e. a pdb format.\n");
    }
    parser = format->parser;
  }
};
} // namespace coords

namespace ic_core {

using coords::float_type;

coords::Representation_3D grads_to_bohr(coords::Representation_3D const& grads);
coords::Representation_3D
rep3d_bohr_to_ang(coords::Representation_3D const& bohr);

struct internal_coord {
  virtual float_type val(coords::Representation_3D const& xyz) const = 0;
  virtual std::vector<float_type>
  der_vec(coords::Representation_3D const& xyz) const = 0;
  virtual float_type
  hessian_guess(coords::Representation_3D const& xyz) const = 0;
  virtual std::string info(coords::Representation_3D const& xyz) const = 0;
};

struct distance : public internal_coord {
  distance(const unsigned int& index_a, const unsigned int& index_b,
           const std::string& elem_a, const std::string& elem_b)
      : index_a_{ index_a }, index_b_{ index_b }, elem_a_{ elem_a }, elem_b_{
          elem_b
        } {}

  std::size_t index_a_;
  std::size_t index_b_;
  std::string elem_a_;
  std::string elem_b_;

  float_type val(coords::Representation_3D const& xyz) const override;
  std::pair<scon::c3<float_type>, scon::c3<float_type>>
  der(coords::Representation_3D const& xyz) const;
  std::vector<float_type>
  der_vec(coords::Representation_3D const& xyz) const override;
  float_type hessian_guess(coords::Representation_3D const& xyz) const override;
  std::string info(coords::Representation_3D const& xyz) const override;
};

struct angle : internal_coord {
  angle(const unsigned int& index_a, const unsigned int& index_b,
        const unsigned int& index_c, const std::string& elem_a,
        const std::string& elem_b, const std::string& elem_c)
      : index_a_{ index_a }, index_b_{ index_b }, index_c_{ index_c },
        elem_a_{ elem_a }, elem_b_{ elem_b }, elem_c_{ elem_c } {}

  std::size_t index_a_;
  std::size_t index_b_;
  std::size_t index_c_;
  std::string elem_a_;
  std::string elem_b_;
  std::string elem_c_;

  coords::float_type val(coords::Representation_3D const& xyz) const override;
  std::tuple<scon::c3<float_type>, scon::c3<float_type>, scon::c3<float_type>>
  der(coords::Representation_3D const& xyz) const;
  std::vector<float_type>
  der_vec(coords::Representation_3D const& xyz) const override;
  float_type hessian_guess(coords::Representation_3D const& xyz) const override;
  std::string info(coords::Representation_3D const& xyz) const override;
};
//
// template<bool isLValue>
// class dihedral_points;
//
// class dihedral_points<true> {
//  using rwcp = std::reference_wrapper<coords::Cartesian_Point const>;
//  rwcp a_;
//  rwcp b_;
//  rwcp c_;
//  rwcp d_;
//};
//
// class dihedral_points<false> {
//  coords::Cartesian_Point a_;
//  coords::Cartesian_Point b_;
//  coords::Cartesian_Point c_;
//  coords::Cartesian_Point d_;
//};
//
// template<bool isLValue>
// class dihedral_indices;
//
// class dihedral_indices<true> {
//  using rwst = std::reference_wrapper<std::size_t const>;
//  rwst index_a_;
//  rwst index_b_;
//  rwst index_c_;
//  rwst index_d_;
//};
//
// class dihedral_indices<false> {
//  std::size_t index_a_;
//  std::size_t index_b_;
//  std::size_t index_c_;
//  std::size_t index_d_;
//};

// template<bool CP_isLVal, bool Ind_isLVal>
struct dihedral : public internal_coord /*: public dihedral_points<CP_isLVal>,
                                           public dihedral_indices<Ind_isLVal>
                                         */
{
  dihedral(const unsigned int& index_a, const unsigned int& index_b,
           const unsigned int& index_c, const unsigned int& index_d)
      : index_a_{ index_a }, index_b_{ index_b }, index_c_{ index_c }, index_d_{
          index_d
        } {}

  coords::float_type val(coords::Representation_3D const& xyz) const override;
  std::tuple<scon::c3<float_type>, scon::c3<float_type>, scon::c3<float_type>,
             scon::c3<float_type>>
  der(coords::Representation_3D const& xyz) const;
  std::vector<float_type>
  der_vec(coords::Representation_3D const& xyz) const override;
  float_type hessian_guess(coords::Representation_3D const& xyz) const override;
  std::string info(coords::Representation_3D const& xyz) const override;

  std::size_t get_ind_a() { return index_a_; }
  std::size_t get_ind_b() { return index_b_; }
  std::size_t get_ind_c() { return index_c_; }
  std::size_t get_ind_d() { return index_d_; }

  std::size_t index_a_;
  std::size_t index_b_;
  std::size_t index_c_;
  std::size_t index_d_;
};

struct out_of_plane : public dihedral {
  using dihedral::dihedral;

  float_type hessian_guess(coords::Representation_3D const& xyz) const override;
  std::string info(coords::Representation_3D const& xyz) const override;
};

struct trans : public internal_coord {
  trans(std::vector<std::size_t> const& index_vec) : indices_(index_vec) {}

  virtual float_type val(coords::Representation_3D const&) const = 0;
  std::vector<std::size_t> indices_;

  template <typename Func>
  coords::Representation_3D der(std::size_t const& sys_size,
                                Func const& coord) const {
    using rep3D = coords::Representation_3D;
    using cp = coords::Cartesian_Point;

    rep3D result(sys_size, cp(0., 0., 0.));

    auto const& s{ indices_.size() };
    for (auto const& i : indices_) {
      result.at(i - 1) = coord(s);
    }
    return result;
  }
  float_type
  hessian_guess(coords::Representation_3D const& /*xyz*/) const override {
    return 0.05;
  }
};

struct trans_x : trans {
  trans_x(const std::vector<std::size_t>& index_vec) : trans(index_vec) {}

  float_type val(const coords::Representation_3D& xyz) const override {
    auto coord_sum{ 0.0 };
    for (auto& i : indices_) {
      coord_sum += xyz.at(i - 1u).x();
    }
    return coord_sum / indices_.size();
  }

  std::vector<float_type>
  der_vec(coords::Representation_3D const& rep) const override;
  std::string info(coords::Representation_3D const& xyz) const override;
};

struct trans_y : trans {
  trans_y(const std::vector<std::size_t>& index_vec) : trans(index_vec) {}

  float_type val(const coords::Representation_3D& xyz) const override {
    auto coord_sum{ 0.0 };
    for (auto& i : indices_) {
      coord_sum += xyz.at(i - 1u).y();
    }
    return coord_sum / indices_.size();
  }

  std::vector<float_type>
  der_vec(coords::Representation_3D const& rep) const override;
  std::string info(coords::Representation_3D const& xyz) const override;
};

struct trans_z : trans {
  trans_z(const std::vector<std::size_t>& index_vec) : trans(index_vec) {}

  float_type val(const coords::Representation_3D& xyz) const override {
    auto coord_sum{ 0.0 };
    for (auto& i : indices_) {
      coord_sum += xyz.at(i - 1u).z();
    }
    return coord_sum / indices_.size();
  }

  std::vector<float_type>
  der_vec(coords::Representation_3D const& rep) const override;
  std::string info(coords::Representation_3D const& xyz) const override;
};

struct rotation {
  template <typename Rep3D, typename IndexVec>
  rotation(Rep3D&& reference, IndexVec&& index_vec)
      : reference_{ std::forward<Rep3D>(reference) },
        indices_{ std::forward<IndexVec>(index_vec) }, rad_gyr_{
          radius_gyration(reference_)
        } {}

  coords::Representation_3D const reference_;
  std::vector<std::size_t> indices_;

  float_type rad_gyr_;

  std::array<float_type, 3u> rot_val(const coords::Representation_3D&) const;
  std::vector<scon::mathmatrix<float_type>>
  rot_der(const coords::Representation_3D&) const;
  scon::mathmatrix<float_type>
  rot_der_mat(std::size_t const&, const coords::Representation_3D&) const;
  float_type radius_gyration(const coords::Representation_3D&);

  // static coords::Representation_3D xyz0;
};

rotation build_rotation(const coords::Representation_3D& target,
                        const std::vector<std::size_t>& index_vec);

class system {
public:
  system(const std::vector<coords::Representation_3D>& res_init,
         const std::vector<std::vector<std::size_t>>& res_index,
         const coords::Representation_3D& xyz_init)
      : res_vec_{ res_init }, res_index_vec_{ res_index }, xyz_{ xyz_init } {}

  std::vector<std::unique_ptr<internal_coord>> primitive_internals;
  std::vector<rotation> rotation_vec_;

private:
  const std::vector<coords::Representation_3D> res_vec_;
  const std::vector<std::vector<std::size_t>> res_index_vec_;
  coords::Representation_3D xyz_;
  scon::mathmatrix<float_type> B_matrix;
  scon::mathmatrix<float_type> G_matrix;
  scon::mathmatrix<float_type> del_mat;
  scon::mathmatrix<float_type> hessian;

  template <typename VertIter>
  static std::vector<std::vector<std::size_t>>
  possible_sets_of_3(VertIter const& vbegin, VertIter const& vend);

  bool new_B_matrix = true;
  bool new_G_matrix = true;

public:
  void append_primitives(std::vector<std::unique_ptr<internal_coord>>&& pic) {
    primitive_internals.insert(primitive_internals.end(),
                               std::make_move_iterator(pic.begin()),
                               std::make_move_iterator(pic.end()));
  }

  std::vector<std::unique_ptr<internal_coord>>
  create_trans_x(const std::vector<std::vector<std::size_t>>&) const;

  std::vector<std::unique_ptr<internal_coord>>
  create_trans_y(const std::vector<std::vector<std::size_t>>&) const;

  std::vector<std::unique_ptr<internal_coord>>
  create_trans_z(const std::vector<std::vector<std::size_t>>&) const;

  std::tuple<std::vector<std::unique_ptr<internal_coord>>,
             std::vector<std::unique_ptr<internal_coord>>,
             std::vector<std::unique_ptr<internal_coord>>>
  create_translations(const std::vector<std::vector<std::size_t>>&) const;

  std::vector<rotation>
  create_rotations(const coords::Representation_3D&,
                   const std::vector<std::vector<std::size_t>>&);

  /*template<typename Graph>
  IC_System create_system(Graph const &);*/

  template <typename Graph>
  std::vector<std::unique_ptr<internal_coord>>
  create_distances(const Graph&) const;

  template <typename Graph>
  std::vector<std::unique_ptr<internal_coord>>
  create_angles(const Graph&) const;

  template <typename Graph>
  std::vector<std::unique_ptr<internal_coord>>
  create_oops(const coords::Representation_3D&, const Graph&) const;

  template <typename Graph>
  std::vector<std::unique_ptr<internal_coord>>
  create_dihedrals(const Graph&) const;

  /*template <typename Graph>
  void create_ic_system(const std::vector<coords::Representation_3D>&,
                        const coords::Representation_3D&, const Graph&);*/

  template <typename Graph>
  void create_ic_system(const Graph&);

  scon::mathmatrix<float_type>& delocalize_ic_system();
  scon::mathmatrix<float_type> guess_hessian();
  scon::mathmatrix<float_type>& initial_delocalized_hessian();
  std::vector<std::vector<float_type>> deriv_vec();
  scon::mathmatrix<float_type>& Bmat();
  scon::mathmatrix<float_type>& Gmat();
  scon::mathmatrix<float_type>& ic_Bmat();
  scon::mathmatrix<float_type>& ic_Gmat();

  scon::mathmatrix<float_type> calc() const;
  scon::mathmatrix<float_type>
  calculate_internal_grads(scon::mathmatrix<float_type> const&);
  template <typename Gint>
  scon::mathmatrix<float_type> get_internal_step(Gint&&);
  template <typename Dint>
  void apply_internal_change(Dint&&);
  template <typename Dcart>
  coords::Representation_3D& take_Cartesian_step(Dcart&& d_cart);
  template <typename XYZ>
  coords::Representation_3D& set_xyz(XYZ&& new_xyz);
  template <typename XYZ>
  scon::mathmatrix<float_type> calc_prims(XYZ&& xyz) const;

  template <typename XYZ>
  scon::mathmatrix<float_type> calc(XYZ&& xyz) const;
  template <typename XYZ>
  scon::mathmatrix<float_type> calc_diff(XYZ&& lhs, XYZ&& rhs) const;

  void optimize(coords::DL_Coordinates& coords);
};

template <typename Graph>
inline std::vector<std::unique_ptr<internal_coord>>
system::create_distances(const Graph& g) const {
  using boost::edges;
  using boost::source;
  using boost::target;

  std::vector<std::unique_ptr<internal_coord>> result;
  auto ed = edges(g);
  for (auto it = ed.first; it != ed.second; ++it) {
    auto u = source(*it, g);
    auto v = target(*it, g);
    auto u_index = g[u].atom_serial;
    auto v_index = g[v].atom_serial;
    auto u_elem = g[u].element;
    auto v_elem = g[v].element;
    result.emplace_back(
        std::make_unique<distance>(u_index, v_index, u_elem, v_elem));
  }
  return result;
}

template <typename Graph>
inline std::vector<std::unique_ptr<internal_coord>>
system::create_angles(const Graph& g) const {
  using boost::adjacent_vertices;
  using boost::vertices;

  std::vector<std::unique_ptr<internal_coord>> result;
  auto vert = vertices(g);
  for (auto it = vert.first; it != vert.second; ++it) {
    auto a_vert = adjacent_vertices(*it, g);
    for (auto it2 = a_vert.first; it2 != a_vert.second; ++it2) {
      for (auto it3 = a_vert.first; it3 != a_vert.second; ++it3) {
        if (g[*it2].atom_serial < g[*it3].atom_serial) {
          auto a_index = g[*it2].atom_serial;
          auto b_index = g[*it].atom_serial;
          auto c_index = g[*it3].atom_serial;
          auto a_elem = g[*it2].element;
          auto b_elem = g[*it].element;
          auto c_elem = g[*it3].element;
          result.emplace_back(std::make_unique<angle>(a_index, b_index, c_index,
                                                      a_elem, b_elem, c_elem));
        }
      }
    }
  }
  return result;
}

template <typename VertIter>
inline std::vector<std::vector<std::size_t>>
system::possible_sets_of_3(VertIter const& vbegin, VertIter const& vend) {
  std::vector<std::vector<std::size_t>> result;
  for (auto first = vbegin; first < vend - 2; ++first) {
    for (auto second = first + 1; second < vend - 1; ++second) {
      for (auto third = first + 2; third < vend; ++third) {
        result.emplace_back(
            std::vector<std::size_t>{ *first, *second, *third });
      }
    }
  }
  return result;
}

template <typename Graph>
inline std::vector<std::unique_ptr<internal_coord>>
system::create_oops(const coords::Representation_3D& coords,
                    const Graph& g) const {
  using boost::adjacent_vertices;
  using boost::vertices;
  using scon::dot;

  std::vector<std::unique_ptr<internal_coord>> result;
  auto vert = vertices(g);
  for (auto it = vert.first; it != vert.second; ++it) {
    auto core = g[*it].atom_serial;
    auto core_cp = coords.at(core - 1);
    auto vert_i = adjacent_vertices(*it, g);
    if ((vert_i.second - vert_i.first) >= 3) {
      auto permutations = possible_sets_of_3(vert_i.first, vert_i.second);
      for (auto& combination : permutations) {
        std::sort(combination.begin(), combination.end());

        // auto permutation_vec = ic_util::permutation_from_vec(neighbours);
        // for (auto& permutation : permutation_vec) {
        auto u_cp = coords.at(combination.at(0));
        auto v_cp = coords.at(combination.at(1));
        auto w_cp = coords.at(combination.at(2));
        auto n_vec1 = ic_util::normal_unit_vector(u_cp, v_cp, w_cp);
        auto n_vec2 = ic_util::normal_unit_vector(core_cp, u_cp, v_cp);
        auto dot_n_vecs = dot(n_vec1, n_vec2);
        if (0.95 < std::fabs(dot_n_vecs)) {
          result.emplace_back(std::make_unique<out_of_plane>(
              core, combination.at(0) + 1, combination.at(1) + 1,
              combination.at(2) + 1));
        }
      }
      //}
    }
  }
  return result;
}

template <typename Graph>
inline std::vector<std::unique_ptr<internal_coord>>
system::create_dihedrals(const Graph& g) const {
  using boost::adjacent_vertices;
  using boost::edges;
  using boost::source;
  using boost::target;

  std::vector<std::unique_ptr<internal_coord>> result;
  auto ed = edges(g);
  for (auto it = ed.first; it != ed.second; ++it) {
    auto u = source(*it, g);
    auto v = target(*it, g);
    auto u_vert = adjacent_vertices(u, g);
    for (auto u_vert_it = u_vert.first; u_vert_it != u_vert.second;
         ++u_vert_it) {
      auto v_vert = adjacent_vertices(v, g);
      for (auto v_vert_it = v_vert.first; v_vert_it != v_vert.second;
           ++v_vert_it) {
        if (g[*u_vert_it].atom_serial != g[*v_vert_it].atom_serial &&
            g[*u_vert_it].atom_serial != g[v].atom_serial &&
            g[u].atom_serial != g[*v_vert_it].atom_serial) {
          auto a_index = g[*u_vert_it].atom_serial;
          auto b_index = g[u].atom_serial;
          auto c_index = g[v].atom_serial;
          auto d_index = g[*v_vert_it].atom_serial;
          result.emplace_back(
              std::make_unique<dihedral>(a_index, b_index, c_index, d_index));
        }
      }
    }
  }
  return result;
}

inline std::tuple<std::vector<std::unique_ptr<internal_coord>>,
                  std::vector<std::unique_ptr<internal_coord>>,
                  std::vector<std::unique_ptr<internal_coord>>>
system::create_translations(
    const std::vector<std::vector<std::size_t>>& res_index_vec) const {
  return std::make_tuple(create_trans_x(res_index_vec),
                         create_trans_y(res_index_vec),
                         create_trans_z(res_index_vec));
}

template <typename Graph>
inline void system::create_ic_system(const Graph& g) {
  append_primitives(create_distances(g));
  append_primitives(create_angles(g));
  append_primitives(create_oops(xyz_, g));
  append_primitives(create_dihedrals(g));

  std::vector<std::unique_ptr<internal_coord>> trans_x, trans_y, trans_z;
  std::tie(trans_x, trans_y, trans_z) = create_translations(res_index_vec_);

  append_primitives(std::move(trans_x));
  append_primitives(std::move(trans_y));
  append_primitives(std::move(trans_z));

  rotation_vec_ = create_rotations(xyz_, res_index_vec_);
}

template <typename Gint>
inline scon::mathmatrix<float_type>
ic_core::system::get_internal_step(Gint&& g_int) {
  return -1. * hessian.pinv() * g_int;
}

template <typename Dint>
inline void ic_core::system::apply_internal_change(Dint&& d_int) {
  using ic_util::flatten_c3_vec;

  auto old_xyz = xyz_;
  coords::Representation_3D first_struct, last_good_xyz;
  auto d_int_left = std::forward<Dint>(d_int);
  auto micro_iter{ 0 }, fail_count{ 0 };
  auto damp{ 1. };
  auto old_rmsd{ 0.0 }, old_inorm{ 0.0 };
  for (; micro_iter < 50; ++micro_iter) {

    // std::cout << "Bmat:\n" << ic_Bmat().t() << "\n\n";
    // std::cout << "Ginv:\n" << ic_Gmat().pinv() << "\n\n";

    take_Cartesian_step(damp * ic_Bmat().t() * ic_Gmat().pinv() *
                        d_int_left); // should it not be G^-1*B^T?
    // std::cout << "Cartesian:\n" << xyz_ << std::endl;

    auto d_now = calc_diff(xyz_, old_xyz);
    // std::cout << "Diff internal coordinates:\n" << d_now << std::endl;

    auto d_int_remain = d_int_left - d_now;
    auto cartesian_rmsd = ic_util::Rep3D_to_Mat(old_xyz - xyz_).rmsd();
    auto internal_norm = d_int_remain.norm();
    // std::cout << "Left change internal coordinates:\n" << d_int_remain <<
    // "\n\n";  std::cout << "internal norm: " << internal_norm << "\n\n";
    if (micro_iter == 0) {
      first_struct = xyz_;
      last_good_xyz = xyz_;
      old_rmsd = cartesian_rmsd;
      old_inorm = internal_norm;
    } else {
      if (internal_norm > old_inorm) {
        damp /= 2.;
        ++fail_count;
      } else {
        fail_count = 0;
        damp = std::min(1.2 * damp, 1.);
        old_rmsd = cartesian_rmsd;
        old_inorm = internal_norm;
        last_good_xyz = xyz_;
      }
    }
    if (cartesian_rmsd < 1.e-6 || internal_norm < 1.e-6) {
      std::cout << "Took " << micro_iter << " steps to converge.\n";
      return;
    } else if (fail_count >= 10) {
      std::cout << "Failed ten times to converge.\n";
      xyz_ = first_struct;
      return;
    }

    old_xyz = xyz_;
    d_int_left = std::move(d_int_remain);
  }
  std::cout << "Took all " << micro_iter + 1
            << " steps, still not converged.\n";
}

template <typename Dcart>
coords::Representation_3D&
ic_core::system::take_Cartesian_step(Dcart&& d_cart) {
  auto d_cart_rep3D = ic_util::mat_to_rep3D(std::forward<Dcart>(d_cart));
  // std::cout << "Cartesian Change:\n" << d_cart_rep3D << "\n\n";
  return set_xyz(xyz_ + d_cart_rep3D);
}

template <typename XYZ>
coords::Representation_3D& ic_core::system::set_xyz(XYZ&& new_xyz) {
  new_B_matrix = true;
  new_G_matrix = true;
  return xyz_ = std::forward<XYZ>(new_xyz);
}

template <typename XYZ>
scon::mathmatrix<float_type> ic_core::system::calc_prims(XYZ&& xyz) const {
  std::vector<float_type> primitives;
  primitives.reserve(primitive_internals.size() + rotation_vec_.size() * 3);

  for (auto const& pic : primitive_internals) {
    primitives.emplace_back(pic->val(xyz));
  }
  std::vector<std::array<float_type, 3u>> rotations;
  for (auto const& rot : rotation_vec_) {
    rotations.emplace_back(rot.rot_val(xyz));
  }
  for (auto i = 0u; i < 3; ++i) {
    for (auto j = 0u; j < rotations.size(); ++j) {
      primitives.emplace_back(rotations.at(j).at(i));
    }
  }
  return scon::mathmatrix<float_type>::row_from_vec(primitives);
}

template <typename XYZ>
scon::mathmatrix<float_type> ic_core::system::calc(XYZ&& xyz) const {
  auto prims = calc_prims(std::forward<XYZ>(xyz));
  return (prims * del_mat).t();
}

template <typename XYZ>
scon::mathmatrix<float_type> ic_core::system::calc_diff(XYZ&& lhs,
                                                        XYZ&& rhs) const {
  auto lprims = calc_prims(std::forward<XYZ>(lhs));
  auto rprims = calc_prims(std::forward<XYZ>(rhs));
  auto diff = lprims - rprims;

  for (auto i = 0u; i < primitive_internals.size(); ++i) {
    if (dynamic_cast<dihedral*>(primitive_internals.at(i).get())) {
      if (std::fabs(diff(0, i)) > SCON_PI) {
        if (diff(0, i) < 0.0) {
          diff(0, i) += 2. * SCON_PI;
        } else {
          diff(0, i) -= 2. * SCON_PI;
        }
      }
    }
  }
  // std::cout << "Diff:\n" << diff.t() << "\n";
  return (diff * del_mat).t();
}

std::pair<float_type, float_type>
grms_val_and_max(scon::mathmatrix<float_type> const& grads);
std::pair<float_type, float_type>
drms_val_and_max(coords::Representation_3D const& old_xyz,
                 coords::Representation_3D const& new_xyz);

struct convergence_check {
  int step;
  float_type E_new;
  float_type E_old;
  scon::mathmatrix<float_type>& gxyz;
  coords::Representation_3D const& old_xyz;
  coords::Representation_3D const& new_xyz;
};
bool check_convergence(convergence_check cc);
} // namespace ic_core
#endif // cast_ic_core_h_guard