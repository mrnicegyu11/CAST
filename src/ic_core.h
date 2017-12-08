﻿////////////////////////////////
// new functions to be tested //
////////////////////////////////
#ifndef cast_ic_core_h_guard
#define cast_ic_core_h_guard

#pragma once

#include "coords.h"
#include "coords_rep.h"
#include "ic_atom.h"
#include "ic_util.h"
#include "pdb.h"
#include "scon_angle.h"
#include "scon_spherical.h"
#include "scon_vect.h"

#include <algorithm>
#include "scon_mathmatrix.h"
#include <array>
#include <boost/graph/adjacency_list.hpp>
#include <cmath>
#include <iterator>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace ic_core {

using coords::float_type;

class distance {
public:
  distance(const coords::Cartesian_Point& a, const coords::Cartesian_Point& b,
           const unsigned int& index_a, const unsigned int& index_b,
           const std::string& elem_a, const std::string& elem_b)
      : a_{ a }, b_{ b }, index_a_{ index_a }, index_b_{ index_b },
        elem_a_{ elem_a }, elem_b_{ elem_b } {}

  const std::size_t index_a_;
  const std::size_t index_b_;
  const std::string elem_a_;
  const std::string elem_b_;

private:
  const coords::Cartesian_Point a_;
  const coords::Cartesian_Point b_;

public:
  coords::float_type dist();
  std::vector<scon::c3<float_type>> bond_der();
  std::vector<float_type> bond_der_vec(const std::size_t&);
};

class angle {
public:
  angle(const coords::Cartesian_Point& a, const coords::Cartesian_Point& b,
        const coords::Cartesian_Point& c, const unsigned int& index_a,
        const unsigned int& index_b, const unsigned int& index_c,
        const std::string& elem_a, const std::string& elem_b,
        const std::string& elem_c)
      : a_{ a }, b_{ b }, c_{ c }, index_a_{ index_a }, index_b_{ index_b },
        index_c_{ index_c }, elem_a_{ elem_a }, elem_b_{ elem_b }, elem_c_{
          elem_c
        } {}

  const std::size_t index_a_;
  const std::size_t index_b_;
  const std::size_t index_c_;
  const std::string elem_a_;
  const std::string elem_b_;
  const std::string elem_c_;

private:
  const coords::Cartesian_Point a_;
  const coords::Cartesian_Point b_;
  const coords::Cartesian_Point c_;

public:
  coords::float_type ang();
  std::vector<scon::c3<float_type>> angle_der();
  std::vector<float_type> angle_der_vec(const std::size_t&);
};

class dihedral {
public:
  dihedral(const coords::Cartesian_Point& a, const coords::Cartesian_Point& b,
           const coords::Cartesian_Point& c, const coords::Cartesian_Point& d,
           const unsigned int& index_a, const unsigned int& index_b,
           const unsigned int& index_c, const unsigned int& index_d)
      : a_{ a }, b_{ b }, c_{ c }, d_{ d }, index_a_{ index_a },
        index_b_{ index_b }, index_c_{ index_c }, index_d_{ index_d } {}

  const std::size_t index_a_;
  const std::size_t index_b_;
  const std::size_t index_c_;
  const std::size_t index_d_;

private:
  const coords::Cartesian_Point a_;
  const coords::Cartesian_Point b_;
  const coords::Cartesian_Point c_;
  const coords::Cartesian_Point d_;

public:
  coords::float_type dihed();
  std::vector<scon::c3<float_type>> dihed_der();
  std::vector<float_type> dihed_der_vec(const std::size_t&);
};

class out_of_plane {
public:
  out_of_plane(const coords::Cartesian_Point& a,
               const coords::Cartesian_Point& b,
               const coords::Cartesian_Point& c,
               const coords::Cartesian_Point& d, const unsigned int& index_a,
               const unsigned int& index_b, unsigned int& index_c,
               const unsigned int& index_d)
      : a_{ a }, b_{ b }, c_{ c }, d_{ d }, index_a_{ index_a },
        index_b_{ index_b }, index_c_{ index_c }, index_d_{ index_d } {}

  const std::size_t index_a_;
  const std::size_t index_b_;
  const std::size_t index_c_;
  const std::size_t index_d_;

public:
  const coords::Cartesian_Point a_;
  const coords::Cartesian_Point b_;
  const coords::Cartesian_Point c_;
  const coords::Cartesian_Point d_;

public:
  coords::float_type oop();
  std::vector<scon::c3<float_type>> oop_der();
  std::vector<float_type> oop_der_vec(const std::size_t&);
};

class trans_x {
public:
  trans_x(const coords::Representation_3D& rep,
          const std::vector<std::size_t>& index_vec)
      : val_{ create_trans_x(rep) }, rep_{ rep }, indices_{ index_vec } {}

  const float_type val_;
  const std::vector<std::size_t> indices_;

private:
  const coords::Representation_3D rep_;

public:
  float_type create_trans_x(const coords::Representation_3D& res) {
    auto coord_sum{ 0.0 };
    for (auto& i : res) {
      coord_sum += i.x();
    }
    return coord_sum / res.size();
  }

  coords::Representation_3D trans_x_der();
  std::vector<float_type> trans_x_der_vec(const std::size_t&);
};

class trans_y {
public:
  trans_y(const coords::Representation_3D& rep,
          const std::vector<std::size_t>& index_vec)
      : val_{ create_trans_y(rep) }, rep_{ rep }, indices_{ index_vec } {}

  const float_type val_;
  const std::vector<std::size_t> indices_;

private:
  const coords::Representation_3D rep_;

public:
  float_type create_trans_y(const coords::Representation_3D& res) {
    auto coord_sum{ 0.0 };
    for (auto& i : res) {
      coord_sum += i.y();
    }
    return coord_sum / res.size();
  }

  coords::Representation_3D trans_y_der();
  std::vector<float_type> trans_y_der_vec(const std::size_t&);
};

class trans_z {
public:
  trans_z(const coords::Representation_3D& rep,
          const std::vector<std::size_t>& index_vec)
      : val_{ create_trans_z(rep) }, rep_{ rep }, indices_{ index_vec } {}

  const float_type val_;
  const std::vector<std::size_t> indices_;

private:
  const coords::Representation_3D rep_;

public:
  float_type create_trans_z(const coords::Representation_3D& res) {
    auto coord_sum{ 0.0 };
    for (auto& i : res) {
      coord_sum += i.z();
    }
    return coord_sum / res.size();
  }

  coords::Representation_3D trans_z_der();
  std::vector<float_type> trans_z_der_vec(const std::size_t&);
};

class rotation {
public:
  rotation(const coords::Representation_3D& target,
           const std::vector<std::size_t>& index_vec)
      : reference_{ target }, rad_gyr_{ radius_gyration(target) }, indices_{
          index_vec
        } {}

  const std::vector<std::size_t> indices_;

private:
  const coords::Representation_3D reference_;
  const float_type rad_gyr_;

public:
  std::array<float_type, 3u> rot_val(const coords::Representation_3D&);
  std::vector<scon::mathmatrix<float_type>> rot_der(const coords::Representation_3D&);
  scon::mathmatrix<float_type> rot_der_mat(const std::size_t&,
                                    const coords::Representation_3D&);
  float_type radius_gyration(const coords::Representation_3D&);
};

class system {
public:
  system(const std::vector<coords::Representation_3D>& res_init,
         const coords::Representation_3D& rep_init,
         const std::vector<std::vector<std::size_t>>& res_index)
      : res_vec_{ res_init }, rep_{ rep_init }, res_index_vec_{ res_index } {}

private:
  const std::vector<coords::Representation_3D> res_vec_;
  const std::vector<std::vector<std::size_t>> res_index_vec_;
  const coords::Representation_3D rep_;

public:
  std::vector<trans_x> trans_x_vec_;
  std::vector<trans_y> trans_y_vec_;
  std::vector<trans_z> trans_z_vec_;
  std::vector<rotation> rotation_vec_;
  std::vector<distance> distance_vec_;
  std::vector<angle> angle_vec_;
  std::vector<out_of_plane> oop_vec_;
  std::vector<dihedral> dihed_vec_;

public:
  std::vector<trans_x>
  create_trans_x(const std::vector<coords::Representation_3D>&,
                 const std::vector<std::vector<std::size_t>>&);

  std::vector<trans_y>
  create_trans_y(const std::vector<coords::Representation_3D>&,
                 const std::vector<std::vector<std::size_t>>&);

  std::vector<trans_z>
  create_trans_z(const std::vector<coords::Representation_3D>&,
                 const std::vector<std::vector<std::size_t>>&);

  std::vector<rotation>
  create_rotations(const std::vector<coords::Representation_3D>&,
                   const std::vector<std::vector<std::size_t>>&);

  template <typename Graph>
  std::vector<distance> create_distances(const coords::Representation_3D&,
                                         const Graph&);

  template <typename Graph>
  std::vector<angle> create_angles(const coords::Representation_3D&,
                                   const Graph&);

  template <typename Graph>
  std::vector<out_of_plane> create_oops(const coords::Representation_3D&,
                                        const Graph&);

  template <typename Graph>
  std::vector<dihedral> create_dihedrals(const coords::Representation_3D&,
                                         const Graph&);

  template <typename Graph>
  void create_ic_system(const std::vector<coords::Representation_3D>&,
                        const coords::Representation_3D&, const Graph&);

  template <typename Graph>
  void create_ic_system(const Graph&);

  std::pair<scon::mathmatrix<float_type>, scon::mathmatrix<float_type>>
  delocalize_ic_system(std::size_t const &, coords::Representation_3D const &);
  scon::mathmatrix<float_type> initial_hessian();
  scon::mathmatrix<float_type> delocalize_hessian(scon::mathmatrix<float_type> const &,
                                                  scon::mathmatrix<float_type> const &);
  scon::mathmatrix<float_type> G_mat_inversion(scon::mathmatrix<float_type> const &);
};

template <typename Graph>
inline std::vector<distance>
system::create_distances(const coords::Representation_3D& coords,
                         const Graph& g) {
  using boost::edges;
  using boost::source;
  using boost::target;

  std::vector<ic_core::distance> result;
  auto ed = edges(g);
  for (auto it = ed.first; it != ed.second; ++it) {
    auto u = source(*it, g);
    auto v = target(*it, g);
    auto u_index = g[u].atom_serial;
    auto v_index = g[v].atom_serial;
    auto u_elem = g[u].element;
    auto v_elem = g[v].element;
    auto u_atom = coords.at(u_index - 1);
    auto v_atom = coords.at(v_index - 1);
    ic_core::distance d(u_atom, v_atom, u_index, v_index, u_elem, v_elem);
    result.emplace_back(d);
  }
  return result;
}

template <typename Graph>
inline std::vector<angle>
system::create_angles(const coords::Representation_3D& coords, const Graph& g) {
  using boost::adjacent_vertices;
  using boost::vertices;

  std::vector<angle> result;
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
          auto a = coords.at(a_index - 1);
          auto b = coords.at(b_index - 1);
          auto c = coords.at(c_index - 1);
          angle ang(a, b, c, a_index, b_index, c_index, a_elem, b_elem, c_elem);
          result.emplace_back(ang);
        }
      }
    }
  }
  return result;
}

template <typename Graph>
inline std::vector<out_of_plane>
system::create_oops(const coords::Representation_3D& coords, const Graph& g) {
  using boost::adjacent_vertices;
  using boost::vertices;
  using scon::dot;

  std::vector<out_of_plane> result;
  auto vert = vertices(g);
  for (auto it = vert.first; it != vert.second; ++it) {
    auto a_vert = adjacent_vertices(*it, g);
    for (auto it2 = a_vert.first; it2 != a_vert.second; ++it2) {
      for (auto it3 = a_vert.first; it3 != a_vert.second; ++it3) {
        for (auto it4 = a_vert.first; it4 != a_vert.second; ++it4) {
          if (g[*it2].atom_serial < g[*it3].atom_serial &&
              g[*it3].atom_serial < g[*it4].atom_serial) {
            auto core = g[*it].atom_serial;
            auto core_cp = coords.at(core - 1);
            auto a = g[*it2].atom_serial;
            auto b = g[*it3].atom_serial;
            auto c = g[*it4].atom_serial;
            std::vector<unsigned int> vec = { a, b, c };
            auto permutation_vec = ic_util::permutation_from_vec(vec);
            for (auto& i : permutation_vec) {
              auto u_cp = coords.at(i.at(0) - 1);
              auto v_cp = coords.at(i.at(1) - 1);
              auto w_cp = coords.at(i.at(2) - 1);
              auto n_vec1 = ic_util::normal_unit_vector(u_cp, v_cp, w_cp);
              auto n_vec2 = ic_util::normal_unit_vector(core_cp, u_cp, v_cp);
              auto dot_n_vecs = dot(n_vec1, n_vec2);
              if (0.95 < dot_n_vecs && dot_n_vecs < 1.05) {
                out_of_plane o(core_cp, u_cp, v_cp, w_cp, core, a, b, c);
                result.emplace_back(o);
              }
            }
          }
        }
      }
    }
  }
  return result;
}

template <typename Graph>
inline std::vector<dihedral>
system::create_dihedrals(const coords::Representation_3D& coords,
                         const Graph& g) {
  using boost::adjacent_vertices;
  using boost::edges;
  using boost::source;
  using boost::target;

  std::vector<dihedral> result;
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
          auto a = coords.at(a_index - 1);
          auto b = coords.at(b_index - 1);
          auto c = coords.at(c_index - 1);
          auto d = coords.at(d_index - 1);
          dihedral dihed(a, b, c, d, a_index, b_index, c_index, d_index);
          result.emplace_back(dihed);
        }
      }
    }
  }
  return result;
}

template <typename Graph>
inline void
system::create_ic_system(const std::vector<coords::Representation_3D>& res_vec,
                         const coords::Representation_3D& coords,
                         const Graph& g) {
  trans_x_vec_ = create_trans_x(res_vec);
  trans_y_vec_ = create_trans_y(res_vec);
  trans_z_vec_ = create_trans_z(res_vec);
  rotation_vec_ = create_rotations(res_vec);
  distance_vec_ = create_distances(coords, g);
  angle_vec_ = create_angles(coords, g);
  oop_vec_ = create_oops(coords, g);
  dihed_vec_ = create_dihedrals(coords, g);
}

template <typename Graph>
inline void system::create_ic_system(const Graph& g) {
  trans_x_vec_ = create_trans_x(res_vec_, res_index_vec_);
  trans_y_vec_ = create_trans_y(res_vec_, res_index_vec_);
  trans_z_vec_ = create_trans_z(res_vec_, res_index_vec_);
  rotation_vec_ = create_rotations(res_vec_, res_index_vec_);
  distance_vec_ = create_distances(rep_, g);
  angle_vec_ = create_angles(rep_, g);
  oop_vec_ = create_oops(rep_, g);
  dihed_vec_ = create_dihedrals(rep_, g);
}
}
#endif // cast_ic_core_h_guard