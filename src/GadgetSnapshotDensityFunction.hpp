/*******************************************************************************
 * This file is part of CMacIonize
 * Copyright (C) 2016 Bert Vandenbroucke (bert.vandenbroucke@gmail.com)
 *
 * CMacIonize is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CMacIonize is distributed in the hope that it will be useful,
 * but WITOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with CMacIonize. If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

/**
 * @file GadgetSnapshotDensityFunction.hpp
 *
 * @brief DensityFunction that reads a density field from a Gadget HDF5 snapshot
 * file: header.
 *
 * @author Bert Vandenbroucke (bv7@st-andrews.ac.uk)
 */
#ifndef GADGETSNAPSHOTDENSITYFUNCTION_HPP
#define GADGETSNAPSHOTDENSITYFUNCTION_HPP

#include "DensityFunction.hpp"
#include <string>
#include <vector>

/**
 * @brief DensityFunction that reads a density field from a Gadget snapshot.
 */
class GadgetSnapshotDensityFunction : public DensityFunction {
private:
  /*! @brief Positions of the SPH particles in the snapshot. */
  std::vector<CoordinateVector> _positions;

  /*! @brief Masses of the SPH particles in the snapshot. */
  std::vector<double> _masses;

  /*! @brief Smoothing lengths of the SPH particles in the snapshot. */
  std::vector<double> _smoothing_lengths;

  double cubic_spline_kernel(double u, double h);

public:
  GadgetSnapshotDensityFunction(std::string name);

  virtual double operator()(CoordinateVector position);
};

#endif // GADGETSNAPSHOTDENSITYFUNCTION_HPP