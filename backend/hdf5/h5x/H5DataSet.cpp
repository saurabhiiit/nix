// Copyright (c) 2013, German Neuroinformatics Node (G-Node)
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted under the terms of the BSD License. See
// LICENSE file in the root of the Project.

#include "H5DataSet.hpp"
#include "H5Exception.hpp"

#include <iostream>
#include <cmath>

namespace nix {
namespace hdf5 {

DataSet::DataSet(hid_t hid)
        : LocID(hid) {

}

DataSet::DataSet(const DataSet &other)
        : LocID(other)  {

}

void DataSet::read(hid_t memType, void *data) const
{
    HErr res = H5Dread(hid, memType, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);
    res.check("DataSet::read() IO error");
}

void DataSet::write(hid_t memType, const void *data)
{
    HErr res = H5Dwrite(hid, memType, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);
    res.check("DataSet::write() IOError");
}

void DataSet::read(DataType dtype, const NDSize &size, void *data) const
{
    h5x::DataType memType = data_type_to_h5_memtype(dtype);

    if (dtype == DataType::String) {
        StringWriter writer(size, static_cast<std::string *>(data));
        read(memType.h5id(), *writer);
        writer.finish();
        vlenReclaim(memType.h5id(), *writer);
    } else {
        read(memType.h5id(), data);
    }
}


void DataSet::write(DataType dtype, const NDSize &size, const void *data)
{
    h5x::DataType memType = data_type_to_h5_memtype(dtype);
    if (dtype == DataType::String) {
        StringReader reader(size, static_cast<const std::string *>(data));
        write(memType.h5id(), *reader);
    } else {
        write(memType.h5id(), data);
    }
}


void DataSet::read(DataType        dtype,
                   void            *data,
                   const Selection &fileSel,
                   const Selection &memSel) const
{
    h5x::DataType memType = data_type_to_h5_memtype(dtype);

    HErr res;
    if (dtype == DataType::String) {
        NDSize size = memSel.size();
        StringWriter writer(size, static_cast<std::string *>(data));
        res = H5Dread(hid, memType.h5id(), memSel.h5space().h5id(), fileSel.h5space().h5id(), H5P_DEFAULT, *writer);
        writer.finish();
        vlenReclaim(memType.h5id(), *writer);
    } else {
        res = H5Dread(hid, memType.h5id(), memSel.h5space().h5id(), fileSel.h5space().h5id(), H5P_DEFAULT, data);
    }

    res.check("DataSet::read() IO error");
}

void DataSet::write(DataType         dtype,
                    const void      *data,
                    const Selection &fileSel,
                    const Selection &memSel)
{
    h5x::DataType memType = data_type_to_h5_memtype(dtype);
    HErr res;

    if (dtype == DataType::String) {
        NDSize size = memSel.size();
        StringReader reader(size, static_cast<const std::string *>(data));
        res = H5Dwrite(hid, memType.h5id(), memSel.h5space().h5id(), fileSel.h5space().h5id(), H5P_DEFAULT, *reader);
    } else {
        res = H5Dwrite(hid, memType.h5id(), memSel.h5space().h5id(), fileSel.h5space().h5id(), H5P_DEFAULT, data);
    }

    res.check("DataSet::write(): IO error");
}

#define CHUNK_BASE   16*1024
#define CHUNK_MIN     8*1024
#define CHUNK_MAX  1024*1024

/**
 * Infer the chunk size from the supplied size information
 *
 * @param dims    Size information to base the guessing on
 * @param dtype   The type of the data to guess the chunks for
 *
 * Internally uses guessChunking(NDSize, size_t) for calculations.
 *
 * @return An (maybe not at all optimal) guess for chunk size
 */
NDSize DataSet::guessChunking(NDSize dims, DataType dtype)
{
    const size_t type_size = data_type_to_size(dtype);
    NDSize chunks = guessChunking(dims, type_size);
    return chunks;
}

/**
 * Infer the chunk size from the supplied size information
 *
 * @param chunks        Size information to base the guessing on
 * @param elementSize   The size of a single element in bytes
 *
 * This function is a port of the guess_chunk() function from h5py
 * low-level Python interface to the HDF5 library.\n
 * http://h5py.alfven.org\n
 *
 * @copyright Copyright 2008 - 2013 Andrew Collette & contributers\n
 * License: BSD 3-clause (see LICENSE.h5py)\n
 *
 * @return An (maybe not at all optimal) guess for chunk size
 */
NDSize DataSet::guessChunking(NDSize chunks, size_t element_size)
{
    // original source:
    //    https://github.com/h5py/h5py/blob/2.1.3/h5py/_hl/filters.py

    if (chunks.size() == 0) {
        throw InvalidRank("Cannot guess chunks for 0-dimensional data");
    }

    double product = 1;
    std::for_each(chunks.begin(), chunks.end(), [&](hsize_t &val) {
        //todo: check for +infinity
        if (val == 0)
            val = 1024;

        product *= val;
    });

    product *= element_size;
    double target_size = CHUNK_BASE * pow(2, log10(product/(1024.0 * 1024.0)));
    if (target_size > CHUNK_MAX)
        target_size = CHUNK_MAX;
    else if (target_size < CHUNK_MIN)
        target_size = CHUNK_MIN;

    size_t i = 0;
    while (true) {
        double csize = static_cast<double>(chunks.nelms());
        if (csize == 1.0) {
            break;
        }

        double cbytes = csize * element_size;
        if ((cbytes < target_size || (std::abs(cbytes - target_size) / target_size) < 0.5)
                && cbytes < CHUNK_MAX) {
            break;
        }

        //not done yet, one more iteration
        size_t idx = i % chunks.size();
        if (chunks[idx] > 1) {
            chunks[idx] = chunks[idx] >> 1; //divide by two
        }
        i++;
    }
    return chunks;
}

void DataSet::setExtent(const NDSize &dims)
{
    DataSpace space = getSpace();

    if (space.extent().size() != dims.size()) {
        throw InvalidRank("Cannot change the dimensionality via setExtent()");
    }

    HErr res = H5Dset_extent(hid, dims.data());
    res.check("DataSet::setExtent(): Could not set the extent of the DataSet.");

}

Selection DataSet::createSelection() const
{
    DataSpace space = getSpace();
    return Selection(space);
}


NDSize DataSet::size() const
{
    return getSpace().extent();
}

void DataSet::vlenReclaim(h5x::DataType mem_type, void *data, DataSpace *dspace) const
{
    HErr res;
    if (dspace != nullptr) {
        res = H5Dvlen_reclaim(mem_type.h5id(), dspace->h5id(), H5P_DEFAULT, data);
    } else {
        DataSpace space = getSpace();
        res = H5Dvlen_reclaim(mem_type.h5id(), space.h5id(), H5P_DEFAULT, data);
    }

    res.check("DataSet::vlenReclaim(): could not reclaim dynamic bufferes");
}


h5x::DataType DataSet::dataType(void) const
{
    h5x::DataType ftype = H5Dget_type(hid);
    ftype.check("DataSet::dataType(): H5Dget_type failed");
    return ftype;
}

DataSpace DataSet::getSpace() const {
    DataSpace space = H5Dget_space(hid);
    space.check("DataSet::getSpace(): Could not obtain dataspace");
    return space;
}


} // namespace hdf5
} // namespace nix
