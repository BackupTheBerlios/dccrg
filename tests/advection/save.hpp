/*
Saver class for the advection test program of dccrg.

Copyright 2012 Finnish Meteorological Institute

Dccrg is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License version 3
as published by the Free Software Foundation.

Dccrg is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with dccrg.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SAVE_HPP
#define SAVE_HPP

#include "boost/mpi.hpp"
#include "cstring"
#include "string"
#include "vector"

#define DCCRG_CELL_DATA_SIZE_FROM_USER
#include "../../dccrg.hpp"

/*!
Class for saving advection test data in a file.
*/
template<class CellData> class Save
{

public:

	/*!
	Saves the given simulation as a .dc file of the given name.

	Returns the number of bytes written by this process.
	Must be called simultaneously by all processes.
	*/
	static size_t save(
		const std::string& filename,
		boost::mpi::communicator comm,
		const dccrg::Dccrg<CellData>& grid
	) {
		std::string header;
		header += "2d advection test file\n\n";
		header += "Data after end of header and a line break:\n";
		header += "1 uint64_t 0x1234567890abcdef for checking endiannes of data\n";
		header += "1 double   grid start coordinate in x direction\n";
		header += "1 double   grid start coordinate in y direction\n";
		header += "1 double   grid start coordinate in z direction\n";
		header += "1 double   x size of unrefined spatial cells\n";
		header += "1 double   y size of unrefined spatial cells\n";
		header += "1 double   z size of unrefined spatial cells\n";
		header += "1 uint64_t length of the grid in unrefined cells in x direction\n";
		header += "1 uint64_t length of the grid in unrefined cells in y direction\n";
		header += "1 uint64_t length of the grid in unrefined cells in z direction\n";
		header += "1 uint8_t  maximum refinement level of the grid\n";
		header += "1 uint64_t cell id\n";
		header += "1 uint32_t cell process number\n";
		header += "1 double   density\n";
		header += "1 double   max relative difference in density between this cell and its neighbors\n";
		header += "1 double   vx\n";
		header += "1 double   vy\n";
		header += "1 double   vz\n";
		header += "1 uint64_t cell id\n";
		header += "...\n";
		header += "end of header\n";

		// set output filename
		std::string output_name(filename);
		output_name += ".dc";

		MPI_File outfile;

		// MPI_File_open wants a non-constant string
		char* output_name_c_string = new char [output_name.size() + 1];
		output_name.copy(output_name_c_string, output_name.size());
		output_name_c_string[output_name.size()] = '\0';

		/*
		Contrary to what http://www.open-mpi.org/doc/v1.4/man3/MPI_File_open.3.php writes,
		MPI_File_open doesn't truncate the file with OpenMPI 1.4.1 on Ubuntu, so use a
		fopen call first (http://www.opengroup.org/onlinepubs/009695399/functions/fopen.html)
		*/
		if (comm.rank() == 0) {
			FILE* i = fopen(output_name_c_string, "w");
			fflush(i);
			fclose(i);
		}
		comm.barrier();

		int result = MPI_File_open(
			comm,
			output_name_c_string,
			MPI_MODE_CREATE | MPI_MODE_WRONLY,
			MPI_INFO_NULL,
			&outfile
		);

		if (result != MPI_SUCCESS) {
			char mpi_error_string[MPI_MAX_ERROR_STRING + 1];
			int mpi_error_string_length;
			MPI_Error_string(result, mpi_error_string, &mpi_error_string_length);
			mpi_error_string[mpi_error_string_length + 1] = '\0';
			std::cerr << "Couldn't open file " << output_name_c_string
				<< ": " << mpi_error_string
				<< std::endl;
			// TODO throw an exception instead
			abort();
		}

		delete [] output_name_c_string;

		// figure out how many bytes each process will write and where
		size_t bytes = 0, offset = 0;

		// collect data from this process into one buffer for writing
		uint8_t* buffer = NULL;

		const std::vector<uint64_t> cells = grid.get_cells();

		// header
		if (comm.rank() == 0) {
			bytes += header.size() * sizeof(char)
				+ 6 * sizeof(double)
				+ 4 * sizeof(uint64_t)
				+ sizeof(uint8_t);
		}

		// bytes of cell data
		bytes += cells.size() * (sizeof(uint64_t) + sizeof(uint32_t) + 5 * sizeof(double));

		buffer = new uint8_t [bytes];

		// header
		if (comm.rank() == 0) {
			{
			memcpy(buffer + offset, header.c_str(), header.size() * sizeof(char));
			offset += header.size() * sizeof(char);
			}

			const uint64_t endiannes = 0x1234567890abcdef;
			memcpy(buffer + offset, &endiannes, sizeof(uint64_t));
			offset += sizeof(uint64_t);

			const double x_start = grid.get_x_start();
			memcpy(buffer + offset, &x_start, sizeof(double));
			offset += sizeof(double);

			const double y_start = grid.get_y_start();
			memcpy(buffer + offset, &y_start, sizeof(double));
			offset += sizeof(double);

			const double z_start = grid.get_z_start();
			memcpy(buffer + offset, &z_start, sizeof(double));
			offset += sizeof(double);

			const double cell_x_size = grid.get_cell_x_size(1);
			memcpy(buffer + offset, &cell_x_size, sizeof(double));
			offset += sizeof(double);

			const double cell_y_size = grid.get_cell_y_size(1);
			memcpy(buffer + offset, &cell_y_size, sizeof(double));
			offset += sizeof(double);

			const double cell_z_size = grid.get_cell_z_size(1);
			memcpy(buffer + offset, &cell_z_size, sizeof(double));
			offset += sizeof(double);

			const uint64_t x_length = grid.get_x_length();
			memcpy(buffer + offset, &x_length, sizeof(uint64_t));
			offset += sizeof(uint64_t);

			const uint64_t y_length = grid.get_y_length();
			memcpy(buffer + offset, &y_length, sizeof(uint64_t));
			offset += sizeof(uint64_t);

			const uint64_t z_length = grid.get_z_length();
			memcpy(buffer + offset, &z_length, sizeof(uint64_t));
			offset += sizeof(uint64_t);

			const uint8_t max_ref_lvl = uint8_t(grid.get_maximum_refinement_level());
			memcpy(buffer + offset, &max_ref_lvl, sizeof(uint8_t));
			offset += sizeof(uint8_t);
		}

		// save cell data
		for (uint64_t i = 0; i < cells.size(); i++) {

			// cell id
			const uint64_t cell = cells[i];
			memcpy(buffer + offset, &cell, sizeof(uint64_t));
			offset += sizeof(uint64_t);

			// process number
			const uint32_t process = grid.get_process(cell);
			memcpy(buffer + offset, &process, sizeof(uint32_t));
			offset += sizeof(uint32_t);

			const CellData* const data = grid[cell];

			// density
			const double density = data->density();
			memcpy(buffer + offset, &density, sizeof(double));
			offset += sizeof(double);

			// max relative difference in density
			const double diff = data->max_diff();
			memcpy(buffer + offset, &diff, sizeof(double));
			offset += sizeof(double);

			// vx
			const double vx = data->vx();
			memcpy(buffer + offset, &vx, sizeof(double));
			offset += sizeof(double);

			// vy
			const double vy = data->vy();
			memcpy(buffer + offset, &vy, sizeof(double));
			offset += sizeof(double);

			// vz
			const double vz = data->vz();
			memcpy(buffer + offset, &vz, sizeof(double));
			offset += sizeof(double);
		}

		std::vector<size_t> all_bytes;
		all_gather(comm, bytes, all_bytes);

		// calculate offset of this process in the file
		MPI_Offset mpi_offset = 0;
		for (int i = 0; i < comm.rank(); i++) {
			mpi_offset += all_bytes[i];
		}

		MPI_Status status;
		MPI_File_write_at_all(outfile, mpi_offset, (void*)buffer, bytes, MPI_BYTE, &status);
		//if (status...

		delete [] buffer;

		MPI_File_close(&outfile);

		return offset;
	}

};	// class

#endif

