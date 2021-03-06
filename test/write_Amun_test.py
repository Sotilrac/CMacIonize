#! /usr/bin/python

################################################################################
# This file is part of CMacIonize
# Copyright (C) 2019 Bert Vandenbroucke (bert.vandenbroucke@gmail.com)
#
# CMacIonize is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# CMacIonize is distributed in the hope that it will be useful,
# but WITOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with CMacIonize. If not, see <http://www.gnu.org/licenses/>.
################################################################################

##
# @file write_Amun_test.py
#
# @brief Write "Amun_test_00-3.hdf5", a minimal set of Amun snapshots that can
# be used to test the AmunSnapshotDensityFunction.
#
# We set up a simple grid of 32x32x32 cells.
#
# The density in the entire box is given by 1.+x+y+z.
#
# @author Bert Vandenbroucke (bv7@st-andrews.ac.uk)
##

import numpy as np
import h5py

##
# @brief Hydro value function.
#
# @param x X coordinate of a position.
# @param y Y coordinate of a position.
# @param z Z coordinate of a position.
# @return Hydro values at that position.
##
def values(x, y, z):
  density = 1. + x + y + z
  velocity_x = -x + y - z
  velocity_y = x - y - z
  velocity_z = -x - y + z
  pressure = 2. + x + y + z
  return density, velocity_x, velocity_y, velocity_z, pressure

dims = np.array([16, 16, 32], dtype = np.int32)
pdims = np.array([2, 2, 1], dtype = np.int32)
rho = np.zeros((32, 32, 32))
vx = np.zeros((32, 32, 32))
vy = np.zeros((32, 32, 32))
vz = np.zeros((32, 32, 32))
P = np.zeros((32, 32, 32))
for ix in range(32):
  for iy in range(32):
    for iz in range(32):
      rho[iz, iy, ix], vx[iz, iy, ix], vy[iz, iy, ix], vz[iz, iy, ix], \
        P[iz, iy, ix]  = values((ix + 0.5) / 32.,
                                (iy + 0.5) / 32.,
                                (iz + 0.5) / 32.)

# loop over the 4 files
for f in range(4):
  # open the file for writing
  file = h5py.File("Amun_test_0{0}.h5".format(f), 'w')

  attributes = file.create_group("/attributes")
  attributes.attrs["dims"] = dims
  attributes.attrs["pdims"] = pdims

  xbeg = (f / 2) * 16
  xend = xbeg + 16
  ybeg = (f % 2) * 16
  yend = ybeg + 16
  rho_block = rho[:, xbeg:xend, ybeg:yend]
  vx_block = vx[:, xbeg:xend, ybeg:yend]
  vy_block = vy[:, xbeg:xend, ybeg:yend]
  vz_block = vz[:, xbeg:xend, ybeg:yend]
  P_block = P[:, xbeg:xend, ybeg:yend]
  variables = file.create_group("/variables")
  variables.create_dataset("dens", data = rho_block, dtype = np.float32)
  variables.create_dataset("velx", data = vx_block, dtype = np.float32)
  variables.create_dataset("vely", data = vy_block, dtype = np.float32)
  variables.create_dataset("velz", data = vz_block, dtype = np.float32)
  variables.create_dataset("pres", data = P_block, dtype = np.float32)
