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
 * @file testGadgetSnapshotDensityFunction.cpp
 *
 * @brief Unit test for the GadgetSnapshotDensityFunction class.
 *
 * @author Bert Vandenbroucke (bv7@st-andrews.ac.uk)
 */
#include "CoordinateVector.hpp"
#include "Error.hpp"
#include "GadgetSnapshotDensityFunction.hpp"
using namespace std;

/**
 * @brief Unit test for the GadgetSnapshotDensityFunction class.
 *
 * @param argc Number of command line arguments.
 * @param argv Command line arguments.
 * @return Exit code: 0 on success.
 */
int main(int argc, char **argv) {
  // before we can test this, we need to make sure we can open and read a
  // Gadget2 snapshot file.
  GadgetSnapshotDensityFunction density("test.hdf5");

  CoordinateVector testpoint(0.5, 0.5, 0.5);
  message("Density(%g, %g, %g): %g", testpoint.x(), testpoint.y(),
          testpoint.z(), density(testpoint));

  return 0;
}