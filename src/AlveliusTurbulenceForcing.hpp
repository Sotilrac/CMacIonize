/*******************************************************************************
 * This file is part of CMacIonize
 * Copyright (C) 2019 Bert Vandenbroucke (bert.vandenbroucke@gmail.com)
 *                    Nina Sartorio (sartorio.nina@gmail.com)
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
 * @file AlveliusTurbulenceForcing.hpp
 *
 * @brief Turbulence forcing using the method of Alvelius (1999).
 *
 * @author Bert Vandenbroucke (bv7@st-andrews.ac.uk)
 * @author Nina Sartorio (sartorio.nina@gmail.com)
 */
#ifndef ALVELIUSTURBULENCEFORCING_HPP
#define ALVELIUSTURBULENCEFORCING_HPP

#include "Box.hpp"
#include "HydroDensitySubGrid.hpp"
#include "Log.hpp"
#include "ParameterFile.hpp"
#include "RandomGenerator.hpp"
#include "RestartReader.hpp"
#include "RestartWriter.hpp"

/**
 * @brief Turbulence forcing using the method of Alvelius (1999).
 */
class AlveliusTurbulenceForcing {
private:
  /*! @brief Number of subgrids in each coordinate direction. */
  const CoordinateVector< int_fast32_t > _number_of_subgrids;

  /*! @brief Number of cells per coordinate direction for a single subgrid. */
  const CoordinateVector< int_fast32_t > _number_of_cells;

  /*! @brief Real amplitudes of the forcing (in m s^-2). */
  std::vector< CoordinateVector<> > _amplitudes_real;

  /*! @brief Imaginary amplitudes of the forcing (in m s^-2). */
  std::vector< CoordinateVector<> > _amplitudes_imaginary;

  /*! @brief Direction unit vectors describing the direction of the first force
   *  term for every mode. */
  std::vector< CoordinateVector<> > _e1;

  /*! @brief Direction unit vectors describing the direction of the second force
   *  term for every mode. */
  std::vector< CoordinateVector<> > _e2;

  /*! @brief The forcing for each mode (in m s^-2). */
  std::vector< double > _kforce;

  /*! @brief Precomputed sine waves in the x direction. */
  std::vector< double > _sin_x;

  /*! @brief Precomputed sine waves in the y direction. */
  std::vector< double > _sin_y;

  /*! @brief Precomputed sine waves in the z direction. */
  std::vector< double > _sin_z;

  /*! @brief Precomputed cosine waves in the x direction. */
  std::vector< double > _cos_x;

  /*! @brief Precomputed cosine waves in the y direction. */
  std::vector< double > _cos_y;

  /*! @brief Precomputed cosine waves in the z direction. */
  std::vector< double > _cos_z;

  /*! @brief Random Generator for turbulence used to generate random forces. */
  RandomGenerator _random_generator;

  /*! @brief Driving time step (in s). */
  const double _time_step;

  /*! @brief Number of driving steps since the start of the simulation. */
  uint_fast32_t _number_of_driving_steps;

  /**
   * @brief Function gets the real and imaginary parts of the amplitudes
   * Aran and Bran of the unit vector e1 and e2, respectively, as in Eq. 11.
   * Alvelius (1999).
   *
   * @param RandGen Random number generator.
   * @param RealRand Output array to store the real parts of the amplitudes of
   * the forcing.
   * @param ImRand Output array to store the imaginary parts of the amplitudes
   * of the forcing.
   */
  static void get_random_factors(RandomGenerator &RandGen, double *RealRand,
                                 double *ImRand) {

    const double phi = 2. * M_PI * RandGen.get_uniform_random_double();
    const double ga = std::sin(phi);
    const double gb = std::cos(phi);
    const double theta1 = 2. * M_PI * RandGen.get_uniform_random_double();
    const double theta2 = 2. * M_PI * RandGen.get_uniform_random_double();
    RealRand[0] = std::cos(theta1) * ga;
    ImRand[0] = std::sin(theta1) * ga;
    RealRand[1] = std::cos(theta2) * gb;
    ImRand[1] = std::sin(theta2) * gb;
  }

public:
  /**
   * @brief Constructor.
   *
   * @param number_of_subgrids Number of subgrids in each coordinate direction.
   * @param number_of_cells Number of cells per coordinate direction for a
   * single subgrid.
   * @param box Dimensions of the simulation box, L (in m).
   * @param kmin Minimum wave number (in L^-1).
   * @param kmax Maximum wave number (in L^-1).
   * @param kforcing Wave number of highest forcing (in L^-1).
   * @param concentration_factor Width of the spectral function (in L^-2).
   * @param power_forcing Input power (in m^2 s^-3).
   * @param seed Seed for the random generator.
   * @param dtfor Forcing time step (in s).
   * @param starting_time Starting time of the simulation (in s).
   * @param log Log to write logging info to.
   */
  AlveliusTurbulenceForcing(
      const CoordinateVector< int_fast32_t > number_of_subgrids,
      const CoordinateVector< int_fast32_t > number_of_cells, const Box<> box,
      const double kmin, const double kmax, const double kforcing,
      const double concentration_factor, const double power_forcing,
      const int_fast32_t seed, const double dtfor, const double starting_time,
      Log *log = nullptr)
      : _number_of_subgrids(number_of_subgrids),
        _number_of_cells(number_of_cells), _random_generator(seed),
        _time_step(dtfor), _number_of_driving_steps(0) {

    /* The force spectrum here prescribed is  Gaussian in shape:
     * F(k) = amplitude*exp^((k-kforcing)^2/concentration_factor)
     *        amplitude*gaussian_exponential
     */
    double spectra_sum = 0.;
    const double cinv = 1. / (concentration_factor * concentration_factor);

    const double Linv = 1. / box.get_sides().x();
    std::vector< CoordinateVector<> > ktable;

    /*
     * Iterate over all possible wavenumbers for
     * x, y and z to obtain all the modes for the forcing.
     * Obtain all the spectra for te forcing as well as the direction
     * in which the forcing is going to be applied (given by unit vectors e1 and
     * e2)
     */
    for (double k1 = 0.; k1 <= kmax; k1 += 1.) {
      double k2start;
      if (k1 == 0.) {
        k2start = 0.;
      } else {
        k2start = -kmax;
      }
      for (double k2 = k2start; k2 <= kmax; k2 += 1.) {
        double k3start;
        if (k1 == 0. && k2 == 0.) {
          k3start = 0.;
        } else {
          k3start = -kmax;
        }
        for (double k3 = k3start; k3 <= kmax; k3 += 1.) {
          const double pwrk1 = k1 * k1;
          const double pwrk2 = k2 * k2;
          const double pwrk3 = k3 * k3;
          const double kk = pwrk1 + pwrk2 + pwrk3;
          const double k = std::sqrt(kk);
          if (k <= kmax && k >= kmin) {
            const double kdiff = (k - kforcing);
            const double sqrtk12 = std::sqrt(pwrk1 + pwrk2);
            const double invkk = 1. / kk;
            const double invk = 1. / k;
            if (sqrtk12 > 0.) {
              const double invsqrtk12 = 1. / sqrtk12;
              _e1.push_back(
                  CoordinateVector<>(k2 * invsqrtk12, -k1 * invsqrtk12, 0.));
              _e2.push_back(CoordinateVector<>(k1 * k3 * invsqrtk12 * invk,
                                               k2 * k3 * invsqrtk12 * invk,
                                               -sqrtk12 * invk));
            } else {
              const double sqrtk13 = std::sqrt(pwrk1 + pwrk3);
              const double invsqrtk13 = 1. / sqrtk13;
              _e1.push_back(
                  CoordinateVector<>(-k3 * invsqrtk13, 0., k1 * invsqrtk13));
              _e2.push_back(CoordinateVector<>(k1 * k2 * invsqrtk13 * invk,
                                               -sqrtk13 * invk,
                                               k2 * k3 * invsqrtk13 * invk));
            }

            cmac_assert(_e1.back().norm2() <= 1.1);
            cmac_assert(_e2.back().norm2() <= 1.1);

            ktable.push_back(CoordinateVector<>(k1, k2, k3) * Linv);
            const double gaussian_spectra =
                std::exp(-kdiff * kdiff * cinv) * invkk;
            spectra_sum += gaussian_spectra;
            _kforce.push_back(gaussian_spectra);
          }
        }
      }
    }

    const uint_fast32_t number_of_modes = ktable.size();

    // Initialize the amplitude vectors to the right size
    _amplitudes_real.resize(number_of_modes);
    _amplitudes_imaginary.resize(number_of_modes);

    // Obtain full expression for the forcing amplitude
    const double norm = power_forcing / (spectra_sum * dtfor);
    for (uint_fast32_t i = 0; i < number_of_modes; ++i) {
      _kforce[i] *= norm;
      _kforce[i] = std::sqrt(_kforce[i]);
    }

    // precompute the sine and cosine waves for faster Fourier transforms
    const CoordinateVector< int_fast32_t > ntot(
        number_of_subgrids.x() * number_of_cells.x(),
        number_of_subgrids.y() * number_of_cells.y(),
        number_of_subgrids.z() * number_of_cells.z());
    const CoordinateVector<> dx(box.get_sides().x() / ntot.x(),
                                box.get_sides().y() / ntot.y(),
                                box.get_sides().z() / ntot.z());
    const CoordinateVector<> anchor = box.get_anchor();
    _sin_x.resize(number_of_modes * ntot.x());
    _sin_y.resize(number_of_modes * ntot.y());
    _sin_z.resize(number_of_modes * ntot.z());
    _cos_x.resize(number_of_modes * ntot.x());
    _cos_y.resize(number_of_modes * ntot.y());
    _cos_z.resize(number_of_modes * ntot.z());
    for (uint_fast32_t ik = 0; ik < number_of_modes; ++ik) {
      for (int_fast32_t ix = 0; ix < ntot.x(); ++ix) {
        const double x = anchor.x() + (ix + 0.5) * dx.x();
        const int_fast32_t index = ix * number_of_modes + ik;
        const double angle = 2. * M_PI * ktable[ik].x() * x;
        _sin_x[index] = std::sin(angle);
        _cos_x[index] = std::cos(angle);
      }
      for (int_fast32_t iy = 0; iy < ntot.y(); ++iy) {
        const double y = anchor.y() + (iy + 0.5) * dx.y();
        const int_fast32_t index = iy * number_of_modes + ik;
        const double angle = 2. * M_PI * ktable[ik].y() * y;
        _sin_y[index] = std::sin(angle);
        _cos_y[index] = std::cos(angle);
      }
      for (int_fast32_t iz = 0; iz < ntot.z(); ++iz) {
        const double z = anchor.z() + (iz + 0.5) * dx.z();
        const int_fast32_t index = iz * number_of_modes + ik;
        const double angle = 2. * M_PI * ktable[ik].z() * z;
        _sin_z[index] = std::sin(angle);
        _cos_z[index] = std::cos(angle);
      }
    }

    // evolve the simulation forward in time until the starting time
    while (_number_of_driving_steps * _time_step < starting_time) {
      for (uint_fast32_t i = 0; i < 3 * number_of_modes; ++i) {
        // we draw 3 random numbers per mode per evolution step
        _random_generator.get_uniform_random_double();
      }
      ++_number_of_driving_steps;
    }
    // reset the number of driving steps to make sure update() works from time
    // 0
    _number_of_driving_steps = 0;

    if (log) {
      log->write_status("Number of turbulent modes: ", number_of_modes);
      log->write_status("Modes:");
      for (uint_fast32_t i = 0; i < number_of_modes; ++i) {
        const CoordinateVector<> k = ktable[i] * box.get_sides().x();
        log->write_status("mode ", i, ": ", k.x(), " ", k.y(), " ", k.z(),
                          " (norm: ", k.norm(), ")");
      }
    }
  }

  /**
   * @brief ParameterFile constructor.
   *
   * The following parameters are read:
   *  - minimum wave number: Minimum wave number to track, in units of the
   *    inverse box length (default: 1.)
   *  - maximum wave number: Maximum wave number to track, in units of the
   *    inverse box length (default: 3.)
   *  - peak forcing wave number: Wave number at which the distribution of the
   *    forcing amplitude peaks, in units of the inverse box lenght (default:
   *    2.5)
   *  - concentration factor: measure for the width of the forcing amplitude
   *    distribution, in units of the inverse box length squared (default: 0.2)
   *  - forcing power: Power of the forcing (default: 2.717e-4 m^2 s^-3)
   *  - random seed: Seed for the internal random number generator (default: 42)
   *  - time step: Time step between subsequent applications of the forcing
   *    (default: 1.519e6 s)
   *  - starting time: Starting time of the simulation. The random number
   *    generator will be forwarded to this time to guarantee a consistent
   *    random sequence between runs (default: 0. s)
   *
   * @param number_of_subgrids Number of subgrids in each coordinate direction.
   * @param number_of_cells Number of cells per coordinate direction for a
   * single subgrid.
   * @param box Dimensions of the simulation box (in m).
   * @param params ParameterFile to read from.
   * @param log Log to write logging info to.
   */
  AlveliusTurbulenceForcing(
      const CoordinateVector< int_fast32_t > number_of_subgrids,
      const CoordinateVector< int_fast32_t > number_of_cells, const Box<> box,
      ParameterFile &params, Log *log = nullptr)
      : AlveliusTurbulenceForcing(
            number_of_subgrids, number_of_cells, box,
            params.get_value< double >("TurbulenceForcing:minimum wave number",
                                       1.),
            params.get_value< double >("TurbulenceForcing:maximum wave number",
                                       3.),
            params.get_value< double >(
                "TurbulenceForcing:peak forcing wave number", 2.5),
            params.get_value< double >("TurbulenceForcing:concentration factor",
                                       0.2),
            params.get_physical_value< QUANTITY_FORCING_POWER >(
                "TurbulenceForcing:forcing power", "2.717e-4 m^2 s^-3"),
            params.get_value< int_fast32_t >("TurbulenceForcing:random seed",
                                             42),
            params.get_physical_value< QUANTITY_TIME >(
                "TurbulenceForcing:time step", "1.519e6 s"),
            params.get_physical_value< QUANTITY_TIME >(
                "TurbulenceForcing:starting time", "0. s"),
            log) {

    cmac_assert(box.get_sides().x() == box.get_sides().y());
    cmac_assert(box.get_sides().x() == box.get_sides().z());
  }

  /**
   * @brief Update the turbulent amplitudes for the next time step.
   *
   * @param end_of_timestep End of the current hydro time step (in s).
   */
  inline void update_turbulence(const double end_of_timestep) {

    for (uint_fast32_t i = 0; i < _kforce.size(); ++i) {
      _amplitudes_real[i] = CoordinateVector<>(0.);
      _amplitudes_imaginary[i] = CoordinateVector<>(0.);
    }

    while (_number_of_driving_steps * _time_step < end_of_timestep) {
      for (uint_fast32_t i = 0; i < _kforce.size(); ++i) {

        double RealRand[2];
        double ImRand[2];
        get_random_factors(_random_generator, RealRand, ImRand);

        cmac_assert(std::abs(RealRand[0]) <= 1.);
        cmac_assert(std::abs(RealRand[1]) <= 1.);
        cmac_assert(std::abs(ImRand[0]) <= 1.);
        cmac_assert(std::abs(ImRand[1]) <= 1.);

        _amplitudes_real[i] += _kforce[i] * _e1[i] * RealRand[0] +
                               _kforce[i] * _e2[i] * RealRand[1];
        _amplitudes_imaginary[i] +=
            _kforce[i] * _e1[i] * ImRand[0] + _kforce[i] * _e2[i] * ImRand[1];
      }
      ++_number_of_driving_steps;
    }
  }

  /**
   * @brief Add the turbulent forcing for the given subgrid.
   *
   * @param index Subgrid index.
   * @param subgrid HydroDensitySubGrid to operate on.
   */
  inline void add_turbulent_forcing(const uint_fast32_t index,
                                    HydroDensitySubGrid &subgrid) const {

    const int_fast32_t offset_x =
        index / (_number_of_subgrids.y() * _number_of_subgrids.z());
    const int_fast32_t offset_y =
        (index - offset_x * _number_of_subgrids.y() * _number_of_subgrids.z()) /
        _number_of_subgrids.z();
    const int_fast32_t offset_z =
        index - offset_x * _number_of_subgrids.y() * _number_of_subgrids.z() -
        offset_y * _number_of_subgrids.z();

    const uint_fast32_t nk = _kforce.size();

    auto cellit = subgrid.hydro_begin();
    for (int_fast32_t ix = 0; ix < _number_of_cells.x(); ++ix) {
      const uint_fast32_t oix = (offset_x * _number_of_cells.x() + ix) * nk;
      for (int_fast32_t iy = 0; iy < _number_of_cells.y(); ++iy) {
        const uint_fast32_t oiy = (offset_y * _number_of_cells.y() + iy) * nk;
        for (int_fast32_t iz = 0; iz < _number_of_cells.z(); ++iz) {
          const uint_fast32_t oiz = (offset_z * _number_of_cells.z() + iz) * nk;
          CoordinateVector<> force;
          for (uint_fast32_t ik = 0; ik < nk; ++ik) {
            const CoordinateVector<> fr = _amplitudes_real[ik];
            const CoordinateVector<> fi = _amplitudes_imaginary[ik];

            const double cosx = _cos_x[oix + ik];
            const double cosy = _cos_y[oiy + ik];
            const double cosz = _cos_z[oiz + ik];
            const double sinx = _sin_x[oix + ik];
            const double siny = _sin_y[oiy + ik];
            const double sinz = _sin_z[oiz + ik];

            const double cosyz = cosy * cosz - siny * sinz;
            const double sinyz = siny * cosz + cosy * sinz;

            const double cosxyz = cosx * cosyz - sinx * sinyz;
            const double sinxyz = sinx * cosyz + cosx * sinyz;

            force += fr * cosxyz - fi * sinxyz;
          }

          const double mdt =
              cellit.get_hydro_variables().get_conserved_mass() * _time_step;
          const CoordinateVector<> old_p =
              cellit.get_hydro_variables().get_conserved_momentum();
          cellit.get_hydro_variables().conserved(1) += mdt * force.x();
          cellit.get_hydro_variables().conserved(2) += mdt * force.y();
          cellit.get_hydro_variables().conserved(3) += mdt * force.z();
          cellit.get_hydro_variables().conserved(4) +=
              _time_step * CoordinateVector<>::dot_product(old_p, force);
          cellit.get_hydro_variables().primitives(1) += _time_step * force.x();
          cellit.get_hydro_variables().primitives(2) += _time_step * force.y();
          cellit.get_hydro_variables().primitives(3) += _time_step * force.z();

          ++cellit;
        }
      }
    }
  }

  /**
   * @brief Dump the forcing object to the given restart file.
   *
   * @param restart_writer RestartWriter to write to.
   */
  void write_restart_file(RestartWriter &restart_writer) const {

    _number_of_subgrids.write_restart_file(restart_writer);
    _number_of_cells.write_restart_file(restart_writer);

    _random_generator.write_restart_file(restart_writer);
    restart_writer.write(_time_step);
    restart_writer.write(_number_of_driving_steps);

    const size_t number_of_modes = _kforce.size();
    restart_writer.write(number_of_modes);
    for (size_t i = 0; i < number_of_modes; ++i) {
      _e1[i].write_restart_file(restart_writer);
      _e2[i].write_restart_file(restart_writer);
      restart_writer.write(_kforce[i]);
    }

    const uint_fast32_t nx =
        _number_of_subgrids.x() * _number_of_cells.x() * number_of_modes;
    for (uint_fast32_t ix = 0; ix < nx; ++ix) {
      restart_writer.write(_sin_x[ix]);
      restart_writer.write(_cos_x[ix]);
    }
    const uint_fast32_t ny =
        _number_of_subgrids.y() * _number_of_cells.y() * number_of_modes;
    for (uint_fast32_t iy = 0; iy < ny; ++iy) {
      restart_writer.write(_sin_y[iy]);
      restart_writer.write(_cos_y[iy]);
    }
    const uint_fast32_t nz =
        _number_of_subgrids.z() * _number_of_cells.z() * number_of_modes;
    for (uint_fast32_t iz = 0; iz < nz; ++iz) {
      restart_writer.write(_sin_z[iz]);
      restart_writer.write(_cos_z[iz]);
    }
  }

  /**
   * @brief Restart constructor.
   *
   * @param restart_reader Restart file to read from.
   */
  inline AlveliusTurbulenceForcing(RestartReader &restart_reader)
      : _number_of_subgrids(restart_reader), _number_of_cells(restart_reader),
        _random_generator(restart_reader),
        _time_step(restart_reader.read< double >()),
        _number_of_driving_steps(restart_reader.read< uint_fast32_t >()) {

    const size_t number_of_modes = restart_reader.read< size_t >();
    _amplitudes_real.resize(number_of_modes);
    _amplitudes_imaginary.resize(number_of_modes);
    _e1.resize(number_of_modes);
    _e2.resize(number_of_modes);
    _kforce.resize(number_of_modes);
    for (size_t i = 0; i < number_of_modes; ++i) {
      _e1[i] = CoordinateVector<>(restart_reader);
      _e2[i] = CoordinateVector<>(restart_reader);
      _kforce[i] = restart_reader.read< double >();
    }

    const uint_fast32_t nx =
        _number_of_subgrids.x() * _number_of_cells.x() * number_of_modes;
    _sin_x.resize(nx);
    _cos_x.resize(nx);
    for (uint_fast32_t ix = 0; ix < nx; ++ix) {
      _sin_x[ix] = restart_reader.read< double >();
      _cos_x[ix] = restart_reader.read< double >();
    }
    const uint_fast32_t ny =
        _number_of_subgrids.y() * _number_of_cells.y() * number_of_modes;
    _sin_y.resize(ny);
    _cos_y.resize(ny);
    for (uint_fast32_t iy = 0; iy < ny; ++iy) {
      _sin_y[iy] = restart_reader.read< double >();
      _cos_y[iy] = restart_reader.read< double >();
    }
    const uint_fast32_t nz =
        _number_of_subgrids.z() * _number_of_cells.z() * number_of_modes;
    _sin_z.resize(nz);
    _cos_z.resize(nz);
    for (uint_fast32_t iz = 0; iz < nz; ++iz) {
      _sin_z[iz] = restart_reader.read< double >();
      _cos_z[iz] = restart_reader.read< double >();
    }
  }
};

#endif // ALVELIUSTURBULENCEFORCING_HPP
