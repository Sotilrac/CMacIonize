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
 * @file testPhotonSourceSpectrum.cpp
 *
 * @brief Unit test for the PhotonSourceSpectrum interface and its
 * implementations.
 *
 * @author Bert Vandenbroucke (bv7@st-andrews.ac.uk)
 */
#include "Assert.hpp"
#include "Error.hpp"
#include "FaucherGiguereDataLocation.hpp"
#include "FaucherGiguerePhotonSourceSpectrum.hpp"
#include "HeliumLymanContinuumSpectrum.hpp"
#include "HeliumTwoPhotonContinuumSpectrum.hpp"
#include "HydrogenLymanContinuumSpectrum.hpp"
#include "PlanckPhotonSourceSpectrum.hpp"
#include "UnitConverter.hpp"
#include "Utilities.hpp"
#include "VernerCrossSections.hpp"
#include <cmath>
#include <fstream>
#include <vector>
using namespace std;

/**
 * @brief Get the Planck black body luminosity for a given frequency.
 *
 * @param frequency Frequency value.
 * @return Planck luminosity.
 */
double planck_luminosity(double frequency) {
  double min_frequency = 3.289e15;
  double planck_constant = 6.626e-27;
  double boltzmann_constant = 1.38e-16;
  double temperature_star = 40000.;
  return frequency * frequency /
         (exp(planck_constant * frequency * min_frequency /
              (boltzmann_constant * temperature_star)) -
          1.);
}

/**
 * @brief Get the hydrogen Lyman continuum luminosity at the given temperature
 * and for the given frequency.
 *
 * @param cross_sections Photoionization cross sections.
 * @param T Temperature.
 * @param frequency Frequency.
 * @return Hydrogen Lyman continuum luminosity.
 */
double HLyc_luminosity(CrossSections &cross_sections, double T,
                       double frequency) {
  double xsecH = cross_sections.get_cross_section(
      ION_H_n,
      UnitConverter::to_SI< QUANTITY_FREQUENCY >(frequency * 13.6, "eV"));
  return 1.e22 * frequency * frequency * xsecH *
         exp(-157919.667 * (frequency - 1.) / T);
}

/**
 * @brief Get the helium Lyman continuum luminosity at the given temperature
 * and for the given frequency.
 *
 * @param cross_sections Photoionization cross sections.
 * @param T Temperature.
 * @param frequency Frequency.
 * @return Helium Lyman continuum luminosity.
 */
double HeLyc_luminosity(CrossSections &cross_sections, double T,
                        double frequency) {
  double xsecHe = cross_sections.get_cross_section(
      ION_He_n,
      UnitConverter::to_SI< QUANTITY_FREQUENCY >(frequency * 13.6, "eV"));
  return 1.e22 * frequency * frequency * xsecHe *
         exp(-157919.667 * (frequency - 1.81) / T);
}

/**
 * @brief Get the helium 2-photon continuum luminosity.
 *
 * @param yHe2q y values of the spectrum.
 * @param AHe2q A values of the spectrum.
 * @param frequency Frequency value.
 * @return Helium 2-photon continuum luminosity.
 */
double He2pc_luminosity(vector< double > &yHe2q, vector< double > &AHe2q,
                        double frequency) {
  double y = frequency * 3.289e15 / 4.98e15;
  if (y < 1.) {
    unsigned int i = Utilities::locate(y, &yHe2q[0], 41);
    double f = (y - yHe2q[i]) / (yHe2q[i + 1] - yHe2q[i]);
    return AHe2q[i] + f * (AHe2q[i + 1] - AHe2q[i]);
  } else {
    return 0.;
  }
}

/**
 * @brief Interpolate on the given Faucher-Giguere reference spectrum.
 *
 * @param nuarr Frequencies (in Ryd).
 * @param earr Energies (in 10^-21 s^-1 cm^-2 Hz^-1 sr^-1).
 * @param nu Frequency for which we want the value (in Ryd).
 * @return Energy at that frequency (in 10^-21 s^-1 cm^-2 Hz^-1 sr^-1).
 */
double FGspectrum(double *nuarr, double *earr, double nu) {
  unsigned int inu = 0;
  while (nu > nuarr[inu]) {
    ++inu;
  }
  return earr[inu - 1] / nuarr[inu - 1] +
         (earr[inu] / nuarr[inu] - earr[inu - 1] / nuarr[inu - 1]) *
             (nu - nuarr[inu - 1]) / (nuarr[inu] - nuarr[inu - 1]);
}

/**
 * @brief Unit test for the PhotonSourceSpectrum interface and its
 * implementations.
 *
 * @param argc Number of command line arguments.
 * @param argv Command line arguments.
 * @return Exit code: 0 on success.
 */
int main(int argc, char **argv) {
  RandomGenerator random_generator;

  // PlanckPhotonSourceSpectrum
  {
    std::ofstream file("planckphotonsource.txt");
    PlanckPhotonSourceSpectrum spectrum(random_generator, 40000);

    unsigned int counts[100];
    for (unsigned int i = 0; i < 100; ++i) {
      counts[i] = 0;
    }
    unsigned int numsample = 1000000;
    for (unsigned int i = 0; i < numsample; ++i) {
      // we manually convert from Hz to 13.6 eV for efficiency reasons
      double rand_freq = spectrum.get_random_frequency() / 3.288465385e15;
      unsigned int index = (rand_freq - 1.) * 100. / 3.;
      ++counts[index];
    }

    double enorm = planck_luminosity(1.015);
    if (counts[0]) {
      enorm /= counts[0];
    }
    for (unsigned int i = 0; i < 100; ++i) {
      double nu = 1. + (i + 0.5) * 0.03;
      double tval = planck_luminosity(nu);
      double bval = counts[i] * enorm;
      double reldiff = std::abs(tval - bval) / std::abs(tval + bval);
      // we fitted a line in x-log10(y) space to the actual relative difference
      double tolerance = std::pow(10., -2.29 + 0.0239001 * (i - 3.));
      file << nu << "\t" << tval << "\t" << bval << "\t" << reldiff << "\t"
           << tolerance << "\n";
      assert_values_equal_rel(tval, bval, tolerance);
    }
  }

  // HydrogenLymanContinuumSpectrum
  // note that we don't test a range of temperature values, but just test a
  // single temperature that is somewhere in between the temperature bins
  // this should be sufficient to cover all cases
  {
    std::ofstream file("hydrogenlymancontinuum.txt");
    VernerCrossSections cross_sections;
    HydrogenLymanContinuumSpectrum spectrum(cross_sections, random_generator);
    double T = 8888.;
    spectrum.set_temperature(T);

    unsigned int counts[100];
    for (unsigned int i = 0; i < 100; ++i) {
      counts[i] = 0;
    }
    unsigned int numsample = 1000000;
    for (unsigned int i = 0; i < numsample; ++i) {
      // we manually convert from Hz to 13.6 eV for efficiency reasons
      double rand_freq = spectrum.get_random_frequency() / 3.288465385e15;
      unsigned int index = (rand_freq - 1.) * 100. / 3.;
      ++counts[index];
    }

    double enorm = HLyc_luminosity(cross_sections, T, 1.045);
    if (counts[1]) {
      enorm /= counts[1];
    }
    for (unsigned int i = 0; i < 100; ++i) {
      double nu = 1. + (i + 0.5) * 0.03;
      double tval = HLyc_luminosity(cross_sections, T, nu);
      double bval = counts[i] * enorm;
      double reldiff = std::abs(tval - bval) / std::abs(tval + bval);
      // we fitted a line in x-log10(y) space to the actual relative difference
      double tolerance = std::pow(10., -2.12 + 0.121377 * (i - 4.));
      file << nu << "\t" << tval << "\t" << bval << "\t" << reldiff << "\t"
           << tolerance << "\n";
      assert_values_equal_rel(tval, bval, tolerance);
    }
  }

  // HeliumLymanContinuumSpectrum
  {
    std::ofstream file("heliumlymancontinuum.txt");
    VernerCrossSections cross_sections;
    HeliumLymanContinuumSpectrum spectrum(cross_sections, random_generator);
    double T = 8888.;
    spectrum.set_temperature(T);

    unsigned int counts[100];
    for (unsigned int i = 0; i < 100; ++i) {
      counts[i] = 0;
    }
    unsigned int numsample = 1000000;
    for (unsigned int i = 0; i < numsample; ++i) {
      // we manually convert from Hz to 13.6 eV for efficiency reasons
      double rand_freq = spectrum.get_random_frequency() / 3.288465385e15;
      unsigned int index = (rand_freq - 1.81) * 100. / (4. - 1.81);
      ++counts[index];
    }

    double enorm =
        HeLyc_luminosity(cross_sections, T, 1.81 + 0.5 * (4. - 1.81) / 100.);
    if (counts[0]) {
      enorm /= counts[0];
    }
    for (unsigned int i = 0; i < 100; ++i) {
      double nu = 1.81 + (i + 0.5) * (4. - 1.81) / 100.;
      double tval = HeLyc_luminosity(cross_sections, T, nu);
      double bval = counts[i] * enorm;
      double reldiff = std::abs(tval - bval) / std::abs(tval + bval);
      // we fitted a line in x-log10(y) space to the actual relative difference
      double tolerance = std::pow(10., -1.9 + 0.0792572 * (i - 6.));
      file << nu << "\t" << tval << "\t" << bval << "\t" << reldiff << "\t"
           << tolerance << "\n";
      assert_values_equal_rel(tval, bval, tolerance);
    }
  }

  // HeliumTwoPhotonContinuumSpectrum
  {
    std::ofstream file("heliumtwophotoncontinuum.txt");
    HeliumTwoPhotonContinuumSpectrum spectrum(random_generator);
    vector< double > yHe2q;
    vector< double > AHe2q;
    spectrum.get_spectrum(yHe2q, AHe2q);

    unsigned int counts[100];
    for (unsigned int i = 0; i < 100; ++i) {
      counts[i] = 0;
    }
    unsigned int numsample = 1000000;
    for (unsigned int i = 0; i < numsample; ++i) {
      // we manually convert from Hz to 13.6 eV for efficiency reasons
      double rand_freq = spectrum.get_random_frequency() / 3.288465385e15;
      unsigned int index = (rand_freq - 1.) * 100. / 0.6;
      ++counts[index];
    }

    double enorm = spectrum.get_integral(yHe2q, AHe2q) / numsample / 0.006;
    for (unsigned int i = 0; i < 100; ++i) {
      double nu = 1. + (i + 0.5) * 0.006;
      double tval = He2pc_luminosity(yHe2q, AHe2q, nu);
      double bval = counts[i] * enorm;
      double reldiff = std::abs(tval - bval) / std::abs(tval + bval);
      // we fitted a line in x-log10(y) space to the actual relative difference
      double tolerance = std::pow(10., -2.1 + 0.0191911 * (i - 17.));
      file << nu << "\t" << tval << "\t" << bval << "\t" << reldiff << "\t"
           << tolerance << "\n";
      assert_values_equal_rel(tval, bval, tolerance);
    }
  }

  // FaucherGiguerePhotonSourceSpectrum
  {
    // check that all redshifts are correctly mapped to files
    for (unsigned int i = 0; i < 214; ++i) {
      unsigned int iz = i * 5;
      double z = iz * 0.01;
      unsigned int iz100 = iz / 100;
      iz -= 100 * iz100;
      unsigned int iz10 = iz / 10;
      iz -= 10 * iz10;
      std::stringstream namestream;
      namestream << FAUCHERGIGUEREDATALOCATION << "fg_uvb_dec11_z_" << iz100
                 << "." << iz10;
      if (iz > 0) {
        namestream << iz;
      }
      namestream << ".dat";
      assert_condition(FaucherGiguerePhotonSourceSpectrum::get_filename(z) ==
                       namestream.str());
    }

    // read in the test spectrum
    std::ifstream ifile(FaucherGiguerePhotonSourceSpectrum::get_filename(7.));
    std::string line;
    // skip two comment lines
    getline(ifile, line);
    getline(ifile, line);
    double nuarr[261], earr[261];
    for (unsigned int i = 0; i < 261; ++i) {
      getline(ifile, line);
      std::istringstream lstream(line);
      lstream >> nuarr[i] >> earr[i];
    }

    std::ofstream file("fauchergiguere.txt");
    FaucherGiguerePhotonSourceSpectrum spectrum(7., random_generator);

    unsigned int counts[100];
    for (unsigned int i = 0; i < 100; ++i) {
      counts[i] = 0;
    }
    unsigned int numsample = 1000000;
    for (unsigned int i = 0; i < numsample; ++i) {
      // we manually convert from Hz to 13.6 eV for efficiency reasons
      double rand_freq = spectrum.get_random_frequency() / 3.288465385e15;
      unsigned int index = (rand_freq - 1.) * 100. / 3.;
      ++counts[index];
    }

    double enorm = FGspectrum(nuarr, earr, 1.015) / counts[0];
    for (unsigned int i = 0; i < 100; ++i) {
      double nu = 1. + (i + 0.5) * 0.03;
      double tval = FGspectrum(nuarr, earr, nu);
      double bval = counts[i] * enorm;
      double reldiff = std::abs(tval - bval) / std::abs(tval + bval);
      // we fitted a line in x-log10(y) space to the actual relative difference
      double tolerance = std::pow(10., -1.96 + 0.00731539 * (i - 4.));
      file << nu << "\t" << tval << "\t" << bval << "\t" << reldiff << "\t"
           << tolerance << "\n";
      assert_values_equal_rel(tval, bval, tolerance);
    }
  }

  return 0;
}
