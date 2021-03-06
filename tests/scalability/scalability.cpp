/*
A program for general scalability testing of dccrg.

Copyright 2011, 2012 Finnish Meteorological Institute

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

#include "boost/foreach.hpp"
#include "boost/mpi.hpp"
#include "boost/program_options.hpp"
#include "cstdlib"
#include "ctime"
#include "fstream"
#include "functional"
#include "iostream"
#include "string"
#include "vector"
#include "zoltan.h"

#include "../../dccrg.hpp"


using namespace std;
using namespace boost::mpi;
using namespace dccrg;


class Cell
{
public:

	vector<uint8_t> data;

	static size_t data_size;

	Cell()
	{
		this->data.resize(Cell::data_size);
	}

	// use boost::mpi for data transfers over MPI
	#ifndef DCCRG_CELL_DATA_SIZE_FROM_USER

	template<typename Archiver> void serialize(
		Archiver& ar,
		const unsigned int /*version*/
	) {
		ar & data;
	}

	// use MPI directly for data transfers
	#else

	void* at(void)
	{
		return this;
	}

	#ifdef DCCRG_USER_MPI_DATA_TYPE
	MPI_Datatype mpi_datatype(void) const
	{
		MPI_Datatype type;
		MPI_Type_contiguous(sizeof(uint8_t) * this->data.size(), MPI_BYTE, &type);
		return type;
	}
	#else
	static size_t size(void)
	{
		// processes don't need other processes' live neighbor info
		return sizeof(uint8_t) * Cell::data_size;
	}
	#endif

	#endif	// ifndef DCCRG_CELL_DATA_SIZE_FROM_USER
};

size_t Cell::data_size = 0;


/*!
Returns the number of bytes that have to be transmitted in given list to other processes.
*/
size_t get_traffic_size(const boost::unordered_map<int, std::vector<uint64_t> >* lists) {
	double communication_size = 0;
	for (boost::unordered_map<int, std::vector<uint64_t> >::const_iterator
		list = lists->begin();
		list != lists->end();
		list++
	) {
		communication_size += sizeof(uint8_t) * Cell::data_size * list->second.size();
	}

	return communication_size;
}


/*!
Returns the amount of time in seconds spent "solving" given cells.
*/
template<class CellData> double solve(
	const double solution_time,
	const vector<uint64_t>& cells,
	Dccrg<CellData>& grid
) {
	timer t;
	const double start_time = t.elapsed();

	BOOST_FOREACH(uint64_t cell, cells) {
		// get a pointer to current cell's data
		CellData* cell_data = grid[cell];
		if (cell_data == NULL) {
			cerr << __FILE__ << ":" << __LINE__
				<< "No data for cell " << cell << endl;
			abort();
		}

		// "solve" for given amount of time
		const double end_time = t.elapsed() + solution_time;
		double elapsed_time = t.elapsed();
		while (elapsed_time < end_time) {
			elapsed_time = t.elapsed();
		}
	}

	return t.elapsed() - start_time;
}


int main(int argc, char* argv[])
{
	environment env(argc, argv);
	communicator comm;

	float zoltan_version;
	if (Zoltan_Initialize(argc, argv, &zoltan_version) != ZOLTAN_OK) {
	    cout << "Zoltan_Initialize failed" << endl;
	    exit(EXIT_FAILURE);
	}

	/*
	Options
	*/
	size_t data_size;
	double solution_time;
	string load_balancer;
	int timesteps, maximum_refinement_level, neighborhood_size;
	uint64_t x_length, y_length, z_length;
	boost::program_options::options_description options("Usage: program_name [options], where options are:");
	options.add_options()
		("help", "print this help message")
		("data_size",
			boost::program_options::value<size_t>(&data_size)->default_value(1),
			"Amount of data in bytes in every cell of the grid")
		("solution_time",
			boost::program_options::value<double>(&solution_time)->default_value(0.001),
			"Amount of time in secods that it takes to \"solve\" one cell")
		("load_balancer",
			boost::program_options::value<string>(&load_balancer)->default_value("HYPERGRAPH"),
			"Load balancing function to use")
		("timesteps",
			boost::program_options::value<int>(&timesteps)->default_value(10),
			"Number of times to \"solve\" cells")
		("x_length",
			boost::program_options::value<uint64_t>(&x_length)->default_value(10),
			"Create a grid with arg number of unrefined cells in the x direction")
		("y_length",
			boost::program_options::value<uint64_t>(&y_length)->default_value(10),
			"Create a grid with arg number of unrefined cells in the y direction")
		("z_length",
			boost::program_options::value<uint64_t>(&z_length)->default_value(10),
			"Create a grid with arg number of unrefined cells in the z direction")
		("maximum_refinement_level",
			boost::program_options::value<int>(&maximum_refinement_level)->default_value(-1),
			"Maximum refinement level of the grid (0 == not refined, -1 == maximum possible for given lengths)")
		("neighborhood_size",
			boost::program_options::value<int>(&neighborhood_size)->default_value(1),
			"Size of a cell's neighborhood in cells of equal size (0 means only cells sharing a face are neighbors)");

	// read options from command line
	boost::program_options::variables_map option_variables;
	boost::program_options::store(boost::program_options::parse_command_line(argc, argv, options), option_variables);
	boost::program_options::notify(option_variables);

	// print a help message if asked
	if (option_variables.count("help") > 0) {
		if (comm.rank() == 0) {
			cout << options << endl;
		}
		comm.barrier();
		return EXIT_SUCCESS;
	}

	// warn if requested solution time too small
	timer t;
	if (solution_time < t.elapsed_min()) {
		cout << "Warning: requested solution time is less than MPI_Wtime resolution, setting solution_time to: " << t.elapsed_min() << endl;
		solution_time = t.elapsed_min();
	}

	Cell::data_size = data_size;

	// initialize
	Dccrg<Cell> grid;

	if (!grid.set_geometry(
		x_length, y_length, z_length,
		0.0, 0.0, 0.0,
		1.0 / x_length, 1.0 / y_length, 1.0 / z_length
	)) {
		cerr << "Couldn't set grid geometry" << endl;
		return EXIT_FAILURE;
	}

	grid.initialize(comm, load_balancer.c_str(), neighborhood_size, maximum_refinement_level);
	grid.balance_load();

	vector<uint64_t> inner_cells = grid.get_cells_with_local_neighbors();
	vector<uint64_t> outer_cells = grid.get_cells_with_remote_neighbor();

	double total_solution_time = 0;
	double sends_size = 0, receives_size = 0;
	for (int timestep = 0; timestep < timesteps; timestep++) {

		const boost::unordered_map<int, std::vector<uint64_t> >* send_lists = grid.get_send_lists();
		sends_size += get_traffic_size(send_lists);

		const boost::unordered_map<int, std::vector<uint64_t> >* receive_lists = grid.get_receive_lists();
		receives_size += get_traffic_size(receive_lists);

		grid.start_remote_neighbor_data_update();

		total_solution_time += solve<Cell>(solution_time, inner_cells, grid);
		grid.wait_neighbor_data_update_receives();

		total_solution_time += solve<Cell>(solution_time, outer_cells, grid);
		grid.wait_neighbor_data_update_sends();
	}

	for (int process = 0; process < comm.size(); process++) {
		comm.barrier();
		if (comm.rank() == process) {
			cout << "Process " << comm.rank()
				<< ": total solution time per timestep " << total_solution_time / timesteps
				<< ", total bytes sent per timestep " << sends_size / timesteps
				<< ", total bytes received per timestep " << receives_size / timesteps
				<< endl;
		}
		comm.barrier();
	}

	double total_transferred_bytes = all_reduce(comm, sends_size, plus<double>());
	if (comm.rank() == 0) {
		cout << "Total transferred bytes per timestep: " << total_transferred_bytes / timesteps << endl;
	}

	return EXIT_SUCCESS;
}

