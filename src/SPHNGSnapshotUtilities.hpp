/*******************************************************************************
 * This file is part of CMacIonize
 * Copyright (C) 2020 Bert Vandenbroucke (bert.vandenbroucke@gmail.com)
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
 * @file SPHNGSnapshotUtilities.hpp
 *
 * @brief Utility functions used when reading SPHNG binary snapshot dumps.
 *
 * @author Bert Vandenbroucke (bert.vandenbroucke@ugent.be)
 */
#ifndef SPHNGSNAPSHOTUTILITIES_HPP
#define SPHNGSNAPSHOTUTILITIES_HPP

#include "Error.hpp"

#include <cinttypes>
#include <fstream>
#include <sstream>

/**
 * @brief Utility functions used when reading SPHNG binary snapshot dumps.
 */
class SPHNGSnapshotUtilities {
public:
  /**
   * @brief Skip a block from the given Fortran unformatted binary file.
   *
   * @param ifile Reference to an open Fortran unformatted binary file.
   */
  inline static void skip_block(std::ifstream &ifile) {
    uint32_t length1, length2;
    ifile.read(reinterpret_cast< char * >(&length1), sizeof(uint32_t));
    ifile.seekg(length1, std::ios_base::cur);
    ifile.read(reinterpret_cast< char * >(&length2), sizeof(uint32_t));
    if (length1 != length2) {
      cmac_error("Wrong block size!");
    }
  }

  /**
   * @brief Get the size of the given template datatype.
   *
   * @param value Reference to a value of the template datatype.
   * @return Size of the template datatype.
   */
  template < typename _datatype_ >
  inline static uint_fast32_t get_size(_datatype_ &value) {
    return sizeof(_datatype_);
  }

  /**
   * @brief Get the size of the given template datatypes (recursively).
   *
   * @param value Next value in the list.
   * @param args Other values in the list.
   * @return Total size of all values in the list.
   */
  template < typename _datatype_, typename... _arguments_ >
  inline static uint_fast32_t get_size(_datatype_ &value,
                                       _arguments_ &... args) {
    return sizeof(_datatype_) + get_size(args...);
  }

  /**
   * @brief Fill the given referenced parameter by reading from the given
   * Fortran unformatted binary file.
   *
   * @param ifile Reference to an open Fortran unformatted binary file.
   * @param value Next (and last) value to read from the file.
   */
  template < typename _datatype_ >
  inline static void read_value(std::ifstream &ifile, _datatype_ &value) {
    ifile.read(reinterpret_cast< char * >(&value), sizeof(value));
  }

  /**
   * @brief Fill the given referenced template parameters by reading from the
   * given Fortran unformatted binary file.
   *
   * @param ifile Reference to an open Fortran unformatted binary file.
   * @param value Next value to read from the file.
   * @param args Other values to read from the file.
   */
  template < typename _datatype_, typename... _arguments_ >
  inline static void read_value(std::ifstream &ifile, _datatype_ &value,
                                _arguments_ &... args) {
    ifile.read(reinterpret_cast< char * >(&value), sizeof(value));
    read_value(ifile, args...);
  }

  /**
   * @brief Read a block  from a Fortran unformatted binary file and fill the
   * given referenced template parameters with its contents.
   *
   * An error will be thrown if the size (in bytes) of all parameters does not
   * match the size of the block.
   *
   * @param ifile Reference to an open Fortran unformatted binary file.
   * @param args References to variables that should be filled with the contents
   * of the block (in the order they are passed to this routine).
   */
  template < typename... _arguments_ >
  inline static void read_block(std::ifstream &ifile, _arguments_ &... args) {
    uint32_t length1, length2;
    ifile.read(reinterpret_cast< char * >(&length1), sizeof(uint32_t));
    uint_fast32_t blocksize = get_size(args...);
    if (length1 != blocksize) {
      cmac_error("Wrong number of variables passed on to read_block()! Block "
                 "size is %u, but size of variables is %" PRIuFAST32 ".",
                 length1, blocksize);
    }
    read_value(ifile, args...);
    ifile.read(reinterpret_cast< char * >(&length2), sizeof(uint32_t));
    if (length1 != length2) {
      cmac_error("Wrong block size!");
    }
  }

  /**
   * @brief Read a dictionary containing tag-value pairs from the given Fortran
   * unformatted binary file.
   *
   * This routine assumes a 3 block structure, whereby the first block contains
   * a single integer giving the number of elements in the second and third
   * block. The second block contains 16-byte tags, while the third block
   * contains a value of the given template type for each tag.
   * If the file is not tagged, the second block is absent. In this case, the
   * tagged flag should be set to false, and the tags will simply be "tag",
   * "tag1"...
   *
   * @param ifile Reference to an open Fortran unformatted binary file to read
   * from.
   * @param tagged Flag indicating whether the file is tagged or not.
   * @return std::map containing the contents of the three blocks as a
   * dictionary.
   */
  template < typename _datatype_ >
  inline static std::map< std::string, _datatype_ >
  read_dict(std::ifstream &ifile, bool tagged = true) {
    uint32_t size;
    read_block(ifile, size);
    std::vector< std::string > tags(size);
    if (tagged) {
      read_block(ifile, tags);
    } else {
      for (uint_fast32_t i = 0; i < size; ++i) {
        tags[i] = "tag";
      }
    }
    std::vector< _datatype_ > vals(size);
    read_block(ifile, vals);
    std::map< std::string, _datatype_ > dict;
    for (uint_fast32_t i = 0; i < size; ++i) {
      // check for duplicates and add numbers to duplicate tag names
      if (dict.count(tags[i]) == 1) {
        uint_fast32_t count = 1;
        std::stringstream newtag;
        newtag << tags[i] << count;
        while (dict.count(newtag.str()) == 1) {
          ++count;
          newtag.str("");
          newtag << tags[i] << count;
        }
        tags[i] = newtag.str();
      }
      dict[tags[i]] = vals[i];
    }
    return dict;
  }
};

/**
 * @brief Fill the given referenced parameter by reading from the given
 * Fortran unformatted binary file.
 *
 * Template specialization for a std::vector of integers.
 *
 * @param ifile Reference to an open Fortran unformatted binary file.
 * @param value Next (and last) value to read from the file.
 */
template <>
inline void SPHNGSnapshotUtilities::read_value< std::vector< int32_t > >(
    std::ifstream &ifile, std::vector< int32_t > &value) {
  ifile.read(reinterpret_cast< char * >(&value[0]),
             value.size() * sizeof(int32_t));
}

/**
 * @brief Fill the given referenced parameter by reading from the given
 * Fortran unformatted binary file.
 *
 * Template specialization for a std::vector of unsigned integers.
 *
 * @param ifile Reference to an open Fortran unformatted binary file.
 * @param value Next (and last) value to read from the file.
 */
template <>
inline void SPHNGSnapshotUtilities::read_value< std::vector< uint32_t > >(
    std::ifstream &ifile, std::vector< uint32_t > &value) {
  ifile.read(reinterpret_cast< char * >(&value[0]),
             value.size() * sizeof(uint32_t));
}

/**
 * @brief Fill the given referenced parameter by reading from the given
 * Fortran unformatted binary file.
 *
 * Template specialization for a std::vector of 64-bit unsigned integers.
 *
 * @param ifile Reference to an open Fortran unformatted binary file.
 * @param value Next (and last) value to read from the file.
 */
template <>
inline void SPHNGSnapshotUtilities::read_value< std::vector< uint64_t > >(
    std::ifstream &ifile, std::vector< uint64_t > &value) {
  ifile.read(reinterpret_cast< char * >(&value[0]),
             value.size() * sizeof(uint64_t));
}

/**
 * @brief Fill the given referenced parameter by reading from the given
 * Fortran unformatted binary file.
 *
 * Template specialization for a std::vector of chars.
 *
 * @param ifile Reference to an open Fortran unformatted binary file.
 * @param value Next (and last) value to read from the file.
 */
template <>
inline void SPHNGSnapshotUtilities::read_value< std::vector< int8_t > >(
    std::ifstream &ifile, std::vector< int8_t > &value) {
  ifile.read(reinterpret_cast< char * >(&value[0]), value.size());
}

/**
 * @brief Fill the given referenced parameter by reading from the given
 * Fortran unformatted binary file.
 *
 * Template specialization for a std::vector of single precision floating point
 * values.
 *
 * @param ifile Reference to an open Fortran unformatted binary file.
 * @param value Next (and last) value to read from the file.
 */
template <>
inline void SPHNGSnapshotUtilities::read_value< std::vector< float > >(
    std::ifstream &ifile, std::vector< float > &value) {
  ifile.read(reinterpret_cast< char * >(&value[0]),
             value.size() * sizeof(float));
}

/**
 * @brief Fill the given referenced parameter by reading from the given
 * Fortran unformatted binary file.
 *
 * Template specialization for a std::vector of double precision floating point
 * values.
 *
 * @param ifile Reference to an open Fortran unformatted binary file.
 * @param value Next (and last) value to read from the file.
 */
template <>
inline void SPHNGSnapshotUtilities::read_value< std::vector< double > >(
    std::ifstream &ifile, std::vector< double > &value) {
  ifile.read(reinterpret_cast< char * >(&value[0]),
             value.size() * sizeof(double));
}

/**
 * @brief Get the size of the given template datatype.
 *
 * Template specialization for a std::vector containing signed integers.
 *
 * @param value Reference to a value of the template datatype.
 * @return Size of the template datatype.
 */
template <>
inline uint_fast32_t SPHNGSnapshotUtilities::get_size< std::vector< int32_t > >(
    std::vector< int32_t > &value) {
  return value.size() * sizeof(int32_t);
}

/**
 * @brief Get the size of the given template datatype.
 *
 * Template specialization for a std::vector containing unsigned integers.
 *
 * @param value Reference to a value of the template datatype.
 * @return Size of the template datatype.
 */
template <>
inline uint_fast32_t
SPHNGSnapshotUtilities::get_size< std::vector< uint32_t > >(
    std::vector< uint32_t > &value) {
  return value.size() * sizeof(uint32_t);
}

/**
 * @brief Get the size of the given template datatype.
 *
 * Template specialization for a std::vector containing 64-bit unsigned
 * integers.
 *
 * @param value Reference to a value of the template datatype.
 * @return Size of the template datatype.
 */
template <>
inline uint_fast32_t
SPHNGSnapshotUtilities::get_size< std::vector< uint64_t > >(
    std::vector< uint64_t > &value) {
  return value.size() * sizeof(uint64_t);
}

/**
 * @brief Get the size of the given template datatype.
 *
 * Template specialization for a std::vector containing signed chars.
 *
 * @param value Reference to a value of the template datatype.
 * @return Size of the template datatype.
 */
template <>
inline uint_fast32_t SPHNGSnapshotUtilities::get_size< std::vector< int8_t > >(
    std::vector< int8_t > &value) {
  return value.size() * sizeof(int8_t);
}

/**
 * @brief Get the size of the given template datatype.
 *
 * Template specialization for a std::vector containing single precision
 * floating point values.
 *
 * @param value Reference to a value of the template datatype.
 * @return Size of the template datatype.
 */
template <>
inline uint_fast32_t SPHNGSnapshotUtilities::get_size< std::vector< float > >(
    std::vector< float > &value) {
  return value.size() * sizeof(float);
}

/**
 * @brief Get the size of the given template datatype.
 *
 * Template specialization for a std::vector containing double precision
 * floating point values.
 *
 * @param value Reference to a value of the template datatype.
 * @return Size of the template datatype.
 */
template <>
inline uint_fast32_t SPHNGSnapshotUtilities::get_size< std::vector< double > >(
    std::vector< double > &value) {
  return value.size() * sizeof(double);
}

/**
 * @brief Read a block  from a Fortran unformatted binary file and fill the
 * given referenced template parameters with its contents.
 *
 * Specialization that reads the entire block into a single string.
 *
 * @param ifile Reference to an open Fortran unformatted binary file.
 * @param value Reference to the std::string parameter that should be filled.
 */
template <>
inline void SPHNGSnapshotUtilities::read_block(std::ifstream &ifile,
                                               std::string &value) {
  uint32_t length1, length2;
  ifile.read(reinterpret_cast< char * >(&length1), sizeof(uint32_t));

  char *cstr = new char[length1 + 1];
  ifile.read(cstr, length1);
  uint_fast32_t i = length1;
  // add string termination character
  cstr[i] = '\0';
  // strip trailing whitespace
  while (i > 0 && cstr[i - 1] == ' ') {
    --i;
    cstr[i] = '\0';
  }
  // fill the given variable
  value = std::string(cstr);
  // free the temporary buffer
  delete[] cstr;

  ifile.read(reinterpret_cast< char * >(&length2), sizeof(uint32_t));
  if (length1 != length2) {
    cmac_error("Wrong block size!");
  }
}

/**
 * @brief Read a block  from a Fortran unformatted binary file and fill the
 * given referenced template parameters with its contents.
 *
 * Specialization that reads the entire block into a std::vector of string,
 * assuming a 16 character tag string for each element.
 *
 * If the total size of the block does not match 16 times the size of the given
 * vector, an error is thrown.
 *
 * @param ifile Reference to an open Fortran unformatted binary file.
 * @param value Reference to the std::string parameter that should be filled.
 */
template <>
inline void
SPHNGSnapshotUtilities::read_block(std::ifstream &ifile,
                                   std::vector< std::string > &value) {
  uint32_t length1, length2;
  ifile.read(reinterpret_cast< char * >(&length1), sizeof(uint32_t));

  if (length1 % 16 != 0) {
    cmac_error("Block has the wrong size to contain a list of tags!");
  }
  if (value.size() * 16 != length1) {
    cmac_error("Vector of wrong size given!");
  }

  // create a temporary read buffer
  char *cstr = new char[17];
  // add string termination character
  cstr[16] = '\0';
  for (size_t i = 0; i < value.size(); ++i) {
    ifile.read(cstr, 16);
    // strip trailing whitespace
    uint_fast8_t j = 16;
    while (j > 0 && cstr[j - 1] == ' ') {
      --j;
      cstr[j] = '\0';
    }
    // fill the given variable
    value[i] = std::string(cstr);
  }
  // free the temporary buffer
  delete[] cstr;

  ifile.read(reinterpret_cast< char * >(&length2), sizeof(uint32_t));
  if (length1 != length2) {
    cmac_error("Wrong block size!");
  }
}

#endif // SPHNGSNAPSHOTUTILITIES_HPP
