/*
Tests the scalability of the grid in 3 D with refined grid
*/

#include "algorithm"
#include "boost/mpi.hpp"
#include "boost/unordered_set.hpp"
#include "cstdlib"
#include "ctime"
#include "../../dccrg.hpp"
#include "fstream"
#include "functional"
#include "iostream"
#include "zoltan.h"


struct game_of_life_cell {

	template<typename Archiver> void serialize(Archiver& ar, const unsigned int /*version*/) {
		ar & is_alive;
	}

	bool is_alive;
	unsigned int live_neighbour_count;
};


using namespace std;
using namespace boost::mpi;

int main(int argc, char* argv[])
{
	environment env(argc, argv);
	communicator comm;

	clock_t before, after, total = 0;

	float zoltan_version;
	if (Zoltan_Initialize(argc, argv, &zoltan_version) != ZOLTAN_OK) {
	    cout << "Zoltan_Initialize failed" << endl;
	    exit(EXIT_FAILURE);
	}

	#define STARTING_CORNER 0.0
	#define GRID_SIZE 21
	#define CELL_SIZE (1.0 / GRID_SIZE)
	#define STENCIL_SIZE 1
	dccrg<game_of_life_cell> game_grid(comm, "RCB", STARTING_CORNER, STARTING_CORNER, STARTING_CORNER, CELL_SIZE, GRID_SIZE, GRID_SIZE, GRID_SIZE, STENCIL_SIZE);
	if (comm.rank() == 0) {
		cout << "Maximum refinement level of the grid: " << game_grid.get_max_refinement_level() << endl;
	}
	game_grid.balance_load();

	vector<uint64_t> cells = game_grid.get_cells();

	// refine random cells until every process has enough cells
	#define MAX_CELLS (100 * GRID_SIZE * GRID_SIZE * GRID_SIZE)
	vector<uint64_t> new_cells;
	do {
		random_shuffle(cells.begin(), cells.end());

		// refine a fraction of all cells each round
		for (int i = 0; i < int(cells.size() / 15); i++) {
			game_grid.refine_completely(cells[i]);
		}
		new_cells = game_grid.stop_refining();
		cells.insert(cells.end(), new_cells.begin(), new_cells.end());
		new_cells.clear();
	} while (all_reduce(comm, int(cells.size()), plus<int>()) < MAX_CELLS);
	game_grid.balance_load();


	vector<uint64_t> cells_with_local_neighbours = game_grid.get_cells_with_local_neighbours();
	vector<uint64_t> cells_with_remote_neighbour = game_grid.get_cells_with_remote_neighbour();
	cout << "Process " << comm.rank() << ": number of cells with local neighbours: " << cells_with_local_neighbours.size() << ", number of cells with a remote neighbour: " << cells_with_remote_neighbour.size() << endl;

	// initialize the game with a line of living cells in the x direction in the middle
	for (vector<uint64_t>::const_iterator cell = cells_with_local_neighbours.begin(); cell != cells_with_local_neighbours.end(); cell++) {

		game_of_life_cell* cell_data = game_grid[*cell];
		cell_data->live_neighbour_count = 0;

		double y = game_grid.get_cell_y(*cell);
		if (fabs(0.5 + 0.1 * game_grid.get_cell_size(*cell) - y) < 0.5 * game_grid.get_cell_size(*cell)) {
			cell_data->is_alive = true;
		} else {
			cell_data->is_alive = false;
		}
	}
	for (vector<uint64_t>::const_iterator cell = cells_with_remote_neighbour.begin(); cell != cells_with_remote_neighbour.end(); cell++) {

		game_of_life_cell* cell_data = game_grid[*cell];
		cell_data->live_neighbour_count = 0;

		double y = game_grid.get_cell_y(*cell);
		if (fabs(0.5 + 0.1 * game_grid.get_cell_size(*cell) - y) < 0.5 * game_grid.get_cell_size(*cell)) {
			cell_data->is_alive = true;
		} else {
			cell_data->is_alive = false;
		}
	}

	// get average number of neighbours
	double avg_neighbours = 0.0;
	uint64_t number_of_cells = cells_with_local_neighbours.size() + cells_with_remote_neighbour.size();
	for (vector<uint64_t>::const_iterator cell = cells_with_local_neighbours.begin(); cell != cells_with_local_neighbours.end(); cell++) {
			const vector<uint64_t>* neighbours = game_grid.get_neighbours(*cell);
			avg_neighbours += double(neighbours->size()) / number_of_cells;
	}
	for (vector<uint64_t>::const_iterator cell = cells_with_remote_neighbour.begin(); cell != cells_with_remote_neighbour.end(); cell++) {
			const vector<uint64_t>* neighbours = game_grid.get_neighbours(*cell);
			avg_neighbours += double(neighbours->size()) / number_of_cells;
	}

	if (comm.rank() == 0) {
		cout << "step: ";
	}

	#define TIME_STEPS 100
	before = clock();
	for (int step = 0; step < TIME_STEPS; step++) {

		if (comm.rank() == 0) {
			cout << step << " ";
			cout.flush();
		}

		game_grid.start_remote_neighbour_data_update();
		// get the neighbour counts of every cell, starting with the cells whose neighbour data doesn't come from other processes
		for (vector<uint64_t>::const_iterator cell = cells_with_local_neighbours.begin(); cell != cells_with_local_neighbours.end(); cell++) {

			game_of_life_cell* cell_data = game_grid[*cell];
			cell_data->live_neighbour_count = 0;

			const vector<uint64_t>* neighbours = game_grid.get_neighbours(*cell);
			for (vector<uint64_t>::const_iterator neighbour = neighbours->begin(); neighbour != neighbours->end(); neighbour++) {

				game_of_life_cell* neighbour_data = game_grid[*neighbour];
				if (neighbour_data->is_alive) {
					cell_data->live_neighbour_count++;
				}
			}
		}

		// wait for neighbour data updates to finish and go through the rest of the cells
		game_grid.wait_neighbour_data_update();
		for (vector<uint64_t>::const_iterator cell = cells_with_remote_neighbour.begin(); cell != cells_with_remote_neighbour.end(); cell++) {

			game_of_life_cell* cell_data = game_grid[*cell];
			cell_data->live_neighbour_count = 0;

			const vector<uint64_t>* neighbours = game_grid.get_neighbours(*cell);
			for (vector<uint64_t>::const_iterator neighbour = neighbours->begin(); neighbour != neighbours->end(); neighbour++) {

				game_of_life_cell* neighbour_data = game_grid[*neighbour];
				if (neighbour_data->is_alive) {
					cell_data->live_neighbour_count++;
				}
			}
		}

		// calculate the next turn
		for (vector<uint64_t>::const_iterator cell = cells_with_local_neighbours.begin(); cell != cells_with_local_neighbours.end(); cell++) {

			game_of_life_cell* cell_data = game_grid[*cell];

			if (cell_data->live_neighbour_count == 3) {
				cell_data->is_alive = true;
			} else if (cell_data->live_neighbour_count != 2) {
				cell_data->is_alive = false;
			}
		}
		for (vector<uint64_t>::const_iterator cell = cells_with_remote_neighbour.begin(); cell != cells_with_remote_neighbour.end(); cell++) {

			game_of_life_cell* cell_data = game_grid[*cell];

			if (cell_data->live_neighbour_count == 3) {
				cell_data->is_alive = true;
			} else if (cell_data->live_neighbour_count != 2) {
				cell_data->is_alive = false;
			}
		}
	}
	after = clock();
	total += after - before;
	if (comm.rank() == 0) {
		cout << endl;
	}
	comm.barrier();

	cout << "Process " << comm.rank() << ": " << number_of_cells * TIME_STEPS << " cells processed at the speed of " << double(number_of_cells * TIME_STEPS) * CLOCKS_PER_SEC / total << " cells / second"<< endl;

	return EXIT_SUCCESS;
}
