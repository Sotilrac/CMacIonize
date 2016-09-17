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
 * @file CommandLineParser.cpp
 *
 * @brief Parser for command line arguments to a program: implementation
 *
 * @author Bert Vandenbroucke (bv7@st-andrews.ac.uk)
 */
#include "CommandLineParser.hpp"
#include <iostream>
using namespace std;

/**
 * @brief Constructor.
 *
 * Parses the command line arguments and stores them.
 */
CommandLineParser::CommandLineParser(int argc, char **argv) {
  for (int i = 0; i < argc; ++i) {
    cout << argv[i] << endl;
    _commands += argv[i];
  }
}

/**
 * @brief Print the contents of the internal dictionary to the given stream.
 *
 * @param stream std::ostream to write to.
 */
void CommandLineParser::print_contents(std::ostream &stream) {
  stream << _commands << endl;
}