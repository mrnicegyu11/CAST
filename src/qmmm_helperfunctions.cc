#include"qmmm_helperfunctions.h"


std::vector<LinkAtom> qmmm_helpers::create_link_atoms(coords::Coordinates* coords, std::vector<size_t> &qm_indices, tinker::parameter::parameters const &tp)
{
  std::vector<LinkAtom> links;
	unsigned type, counter = 0;

	for (auto q : qm_indices)
	{
		if (coords->atoms().size() == 0) break;  // this is necessary because this function will be called once in the beginning when coordinates object is not created yet
		for (auto b : coords->atoms().atom(q).bonds())
		{
			if (!is_in(b, qm_indices))
			{
				if (counter < Config::get().energy.qmmm.linkatom_types.size())
				{
					type = Config::get().energy.qmmm.linkatom_types[counter];
				}
				else type = 85;    // if atomtype not found -> 85 (should mostly be correct for OPLSAA force field)
				LinkAtom link(q, b, type, coords, tp);
				links.push_back(link);
				counter += 1;

				if (Config::get().general.verbosity > 3)
				{
					std::cout << "created link atom between QM atom " << q + 1 << " and MM atom " << b + 1 << " with atom type " << link.energy_type << ", position: " << link.position << "\n";
				}
			}
		}
	}
  return links;
}

void qmmm_helpers::calc_link_atom_grad(LinkAtom &l, coords::r3 const &G_L, coords::Coordinates* coords, coords::r3 &G_QM, coords::r3 &G_MM)
{
  double x, y, z;

  double g = l.deq_L_QM / dist(coords->xyz(l.mm), coords->xyz(l.qm));
  coords::r3 n = (coords->xyz(l.mm) - coords->xyz(l.qm)) / dist(coords->xyz(l.mm), coords->xyz(l.qm));

  x = g * scon::dot(G_L, n) *n.x() + (1 - g)*G_L.x();
  y = g * scon::dot(G_L, n) *n.y() + (1 - g)*G_L.y();
  z = g * scon::dot(G_L, n) *n.z() + (1 - g)*G_L.z();
  coords::r3 new_grad(x, y, z);
  G_QM = new_grad;

  x = g * G_L.x() - g * scon::dot(G_L, n) * n.x();
  y = g * G_L.y() - g * scon::dot(G_L, n) * n.y();
  z = g * G_L.z() - g * scon::dot(G_L, n) * n.z();
  coords::r3 new_grad2(x, y, z);
  G_MM = new_grad2;
}


std::vector<std::size_t> qmmm_helpers::get_mm_atoms(std::size_t const num_atoms)
{
    std::vector<std::size_t> mm_atoms;
    auto qm_size = Config::get().energy.qmmm.qmatoms.size();
    if (num_atoms > qm_size)
    {
      auto mm_size = num_atoms - qm_size;
      mm_atoms.reserve(mm_size);
      for (unsigned i = 0; i < num_atoms; i++)
      {
        if (!scon::sorted::exists(Config::get().energy.qmmm.qmatoms, i))
        {
          mm_atoms.emplace_back(i);
        }
      }
    }
    return mm_atoms;
}

  std::vector<std::size_t> qmmm_helpers::make_new_indices(std::size_t const num_atoms, std::vector<std::size_t> const& indices)
  {
    std::vector<std::size_t> new_indices;
    new_indices.resize(num_atoms);
    if (num_atoms > 0u)
    {
      std::size_t current_index = 0u;

      for (auto && a : indices)
      {
				new_indices.at(a) = current_index++;
      }
    }
    return new_indices;
  }

  /**creates coordobject for MM interface
  @param cp: coordobj for whole system (QM + MM)
  @param indices: indizes of MM atoms
  @param new_indices: new indizes (see new_indices_mm)*/
  coords::Coordinates qmmm_helpers::make_aco_coords(coords::Coordinates const * cp,
    std::vector<std::size_t> const & indices, std::vector<std::size_t> const & new_indices)
  {
    auto tmp_i = Config::get().general.energy_interface;
    Config::set().general.energy_interface = Config::get().energy.qmmm.mminterface;
    coords::Coordinates new_aco_coords;
    if (cp->size() >= indices.size())
    {
      coords::Atoms new_aco_atoms;
      coords::PES_Point pes;
      pes.structure.cartesian.reserve(indices.size());
      for (auto && a : indices)
      {
        auto && ref_at = (*cp).atoms().atom(a);
        coords::Atom at{ (*cp).atoms().atom(a).number() };
        at.set_energy_type(ref_at.energy_type());
        auto bonds = ref_at.bonds();
        for (auto && b : bonds)
        {
          at.detach_from(b);
        }
        for (auto && b : bonds)
        {
          if (is_in(b, indices)) at.bind_to(new_indices[b]);  // only bind if bonding partner is also in ACO coords
        }
        new_aco_atoms.add(at);
        pes.structure.cartesian.push_back(cp->xyz(a));
      }
      new_aco_coords.init_swap_in(new_aco_atoms, pes);
    }
    Config::set().general.energy_interface = tmp_i;
    return new_aco_coords;
  }

  coords::Coordinates qmmm_helpers::make_mmbig_coords(coords::Coordinates const * cp)
  {
    auto tmp_i = Config::get().general.energy_interface;
    Config::set().general.energy_interface = Config::get().energy.qmmm.mminterface;
    coords::Coordinates new_aco_coords;
    coords::Atoms new_aco_atoms;
    coords::PES_Point pes;

    pes.structure.cartesian.reserve(cp->atoms().size());
    for (auto a{ 0u }; a < cp->atoms().size(); ++a)
    {
      auto && ref_at = (*cp).atoms().atom(a);
      coords::Atom at{ (*cp).atoms().atom(a).number() };
      at.set_energy_type(ref_at.energy_type());
      auto bonds = ref_at.bonds();
      for (auto && b : bonds)
      {
        at.detach_from(b);
      }
      for (auto && b : bonds)
      {
        at.bind_to(b);
      }
      new_aco_atoms.add(at);
      pes.structure.cartesian.push_back(cp->xyz(a));
    }
    new_aco_coords.init_swap_in(new_aco_atoms, pes);
    Config::set().general.energy_interface = tmp_i;
    return new_aco_coords;
  }

  coords::Coordinates qmmm_helpers::make_small_coords(coords::Coordinates const * cp,
    std::vector<std::size_t> const & indices, std::vector<std::size_t> const & new_indices, std::vector<LinkAtom> &link_atoms, config::interface_types::T energy_interface, std::string const& filename)
  {
    auto tmp_i = Config::get().general.energy_interface;
    Config::set().general.energy_interface = energy_interface;
    coords::Coordinates new_qm_coords;
    if (cp->size() >= indices.size())
    {
      coords::Atoms new_qm_atoms;
      coords::Atoms tmp_link_atoms;
      coords::PES_Point pes;
      pes.structure.cartesian.reserve(indices.size()+link_atoms.size());

      for (auto && a : indices)      // add and bind "normal" atoms
      {
        auto && ref_at = (*cp).atoms().atom(a);
        coords::Atom at{ (*cp).atoms().atom(a).number() };
        at.set_energy_type(ref_at.energy_type());
        auto bonds = ref_at.bonds();
        for (auto && b : bonds)
        {
          at.detach_from(b);
        }
        for (auto && b : bonds)
        {
          if (is_in(b, indices)) at.bind_to(new_indices.at(b)); // only bind if bonding partner is also in QM coords
        }
        
        new_qm_atoms.add(at);
        pes.structure.cartesian.push_back(cp->xyz(a));
      }
      
      int counter = 0;
      for (auto a : link_atoms)  // create temporary vector with link atoms
      {
        coords::Atom current("H");
        current.set_energy_type(a.energy_type);
        current.bind_to(new_indices.at(a.qm));
        tmp_link_atoms.add(current);
        pes.structure.cartesian.push_back(a.position);

        for (auto i{ 0u }; i < new_qm_atoms.size(); ++i)  // bind bonding partner in "normal" atoms to link atom
        {
          if (current.is_bound_to(i))
          {
            coords::Atom& current2 = new_qm_atoms.atom(i);
            current2.bind_to(new_qm_atoms.size() + counter);
			      break; // link atom is hydrogen so only one bonding partner
          }
        }
        counter++;
      }
      for (auto a : tmp_link_atoms) new_qm_atoms.add(a);  // add link atoms
      
      new_qm_coords.init_swap_in(new_qm_atoms, pes);
    }

    if (Config::set().energy.qmmm.qm_to_file)  // if desired: write QM region into file
    {
      std::ofstream output(filename);
      output << coords::output::formats::tinker(new_qm_coords);
    }
    
    Config::set().general.energy_interface = tmp_i;
    return new_qm_coords;
  }

	void qmmm_helpers::select_from_ambercharges(std::vector<std::size_t> const & indices)
	{
		std::vector<coords::float_type> c = Config::get().coords.amber_charges;  // get AMBER charges
		std::vector<coords::float_type> charges_temp;
		for (auto i = 0u; i<c.size(); i++)
		{
			if (is_in(i, indices))  // find atom charges for indizes
			{
				charges_temp.push_back(c[i]);   // add those charges to new vector
			}
		}
		Config::set().coords.amber_charges = charges_temp; // set new AMBER charges
	}

	void qmmm_helpers::add_external_charges(std::vector<size_t> &qm_indizes, std::vector<size_t> &ignore_indizes, std::vector<double> &charges, std::vector<size_t> &indizes_of_charges,
		std::vector<LinkAtom> &link_atoms, std::vector<int> &charge_indizes, coords::Coordinates *coords)
	{
		for (auto i : indizes_of_charges)  // go through all atoms from which charges are looked at
		{
			bool use_charge = true;

			for (auto &l : link_atoms)      // look at every link atom
			{
				if (l.mm == i) use_charge = false;   // ignore those atoms that are connected to an atom to the "QM system"
				else
				{
					if (Config::get().energy.qmmm.zerocharge_bonds > 1)   // if desired: also ignore atoms that are two bonds away from "QM system"
					{
						for (auto b : coords->atoms(i).bonds())
						{
              if (b == l.mm) use_charge = false;

              else
              {
                if (Config::get().energy.qmmm.zerocharge_bonds > 2)  // if desired: also ignore atoms that are three bonds away from "QM system"
                {
                  for (auto b2 : coords->atoms(b).bonds())
                  {
                    if (b2 == l.mm) use_charge = false;
                  }
                }
              }
						}
					}
				}
			}
			for (auto &qs : ignore_indizes) // ...and those which are destined to be ignored
			{
				if (qs == i) use_charge = false;
			}

			if (use_charge)  // for the other 
			{
				if (Config::get().energy.qmmm.cutoff != 0.0)  // if cutoff given: test if one "QM atom" is nearer than cutoff
				{
					use_charge = false;
					for (auto qs : qm_indizes)
					{
						auto dist = len(coords->xyz(i) - coords->xyz(qs));
						if (dist < Config::get().energy.qmmm.cutoff)
						{
							use_charge = true;
							break;
						}
					}
				}

				if (use_charge)  // if yes create a PointCharge and add it to vector
				{
					PointCharge new_charge;
					new_charge.charge = charges[find_index(i, indizes_of_charges)];
					new_charge.set_xyz(coords->xyz(i).x(), coords->xyz(i).y(), coords->xyz(i).z());
					Config::set().energy.qmmm.mm_charges.push_back(new_charge);

					charge_indizes.push_back(i);  // add index to charge_indices
				}
			}
		}
	}