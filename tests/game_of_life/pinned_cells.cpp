/*
As unrefined2d.cpp but pinns cells to particular processes.
*/

#include "algorithm"
#include "boost/mpi.hpp"
#include "boost/unordered_set.hpp"
#include "cstdlib"
#include "fstream"
#include "iostream"
#include "zoltan.h"

#include "../../dccrg_arbitrary_geometry.hpp"
#include "../../dccrg.hpp"


struct game_of_life_cell {

	template<typename Archiver> void serialize(Archiver& ar, const unsigned int /*version*/) {
		ar & is_alive;
		ar & live_unrefined_neighbors[0] & live_unrefined_neighbors[1] & live_unrefined_neighbors[2];
	}

	bool is_alive;
	// total live neighbor count for all siblings
	unsigned int total_live_neighbor_count;
	// record live neighbors of refined cells to calculate the above
	uint64_t live_unrefined_neighbors[3];
	// only count one sibling of an unrefined cell (their states of life should be identical)
	uint64_t child_of_processed[8];
};


using namespace std;
using namespace boost;
using namespace boost::mpi;
using namespace dccrg;

int main(int argc, char* argv[])
{
	environment env(argc, argv);
	communicator comm;

	float zoltan_version;
	if (Zoltan_Initialize(argc, argv, &zoltan_version) != ZOLTAN_OK) {
	    cout << "Zoltan_Initialize failed" << endl;
	    exit(EXIT_FAILURE);
	}
	if (comm.rank() == 0) {
		cout << "Using Zoltan version " << zoltan_version << endl;
	}

	Dccrg<game_of_life_cell, ArbitraryGeometry> game_grid;

	#define STARTING_CORNER 0.0
	#define GRID_SIZE 15
	#define CELL_SIZE (1.0 / GRID_SIZE)
	vector<double> x_coordinates, y_coordinates, z_coordinates;
	for (int i = 0; i <= GRID_SIZE; i++) {
		x_coordinates.push_back(i * CELL_SIZE);
		y_coordinates.push_back(i * CELL_SIZE);
	}
	z_coordinates.push_back(0);
	z_coordinates.push_back(1);
	game_grid.set_geometry(x_coordinates, y_coordinates, z_coordinates);

	#define NEIGHBORHOOD_SIZE 1
	game_grid.initialize(comm, "RANDOM", NEIGHBORHOOD_SIZE);

	// create a blinker
	#define BLINKER_START 198
	uint64_t tmp1[] = {BLINKER_START, BLINKER_START + 1, BLINKER_START + 2};
	vector<uint64_t> blinker_cells(tmp1, tmp1 + sizeof(tmp1) / sizeof(uint64_t));
	for (vector<uint64_t>::const_iterator cell = blinker_cells.begin(); cell != blinker_cells.end(); cell++) {
		game_of_life_cell* cell_data = game_grid[*cell];
		if (cell_data == NULL) {
			continue;
		}
		cell_data->is_alive = true;
	}

	// create a toad
	#define TOAD_START 188
	uint64_t tmp2[] = {TOAD_START, TOAD_START + 1, TOAD_START + 2, TOAD_START + 1 + GRID_SIZE, TOAD_START + 2 + GRID_SIZE, TOAD_START + 3 + GRID_SIZE};
	vector<uint64_t> toad_cells(tmp2, tmp2 + sizeof(tmp2) / sizeof(uint64_t));
	for (vector<uint64_t>::const_iterator cell = toad_cells.begin(); cell != toad_cells.end(); cell++) {
		game_of_life_cell* cell_data = game_grid[*cell];
		if (cell_data == NULL) {
			continue;
		}
		cell_data->is_alive = true;
	}

	// create a beacon
	#define BEACON_START 137
	uint64_t tmp3[] = {BEACON_START, BEACON_START + 1, BEACON_START - GRID_SIZE, BEACON_START + 1 - GRID_SIZE, BEACON_START + 2 - 2 * GRID_SIZE, BEACON_START + 3 - 2 * GRID_SIZE, BEACON_START + 2 - 3 * GRID_SIZE, BEACON_START + 3 - 3 * GRID_SIZE};
	vector<uint64_t> beacon_cells(tmp3, tmp3 + sizeof(tmp3) / sizeof(uint64_t));
	for (vector<uint64_t>::const_iterator cell = beacon_cells.begin(); cell != beacon_cells.end(); cell++) {
		game_of_life_cell* cell_data = game_grid[*cell];
		if (cell_data == NULL) {
			continue;
		}
		cell_data->is_alive = true;
	}

	// create a glider
	#define GLIDER_START 143
	uint64_t tmp4[] = {GLIDER_START + 1, GLIDER_START + 2 - GRID_SIZE, GLIDER_START - 2 * GRID_SIZE, GLIDER_START + 1 - 2 * GRID_SIZE, GLIDER_START + 2 - 2 * GRID_SIZE};
	vector<uint64_t> glider_cells(tmp4, tmp4 + sizeof(tmp4) / sizeof(uint64_t));
	for (vector<uint64_t>::const_iterator cell = glider_cells.begin(); cell != glider_cells.end(); cell++) {
		game_of_life_cell* cell_data = game_grid[*cell];
		if (cell_data == NULL) {
			continue;
		}
		cell_data->is_alive = true;
	}

	// create a block
	#define BLOCK_START 47
	uint64_t tmp5[] = {BLOCK_START, BLOCK_START + 1, BLOCK_START - GRID_SIZE, BLOCK_START + 1 - GRID_SIZE};
	vector<uint64_t> block_cells(tmp5, tmp5 + sizeof(tmp5) / sizeof(uint64_t));
	for (vector<uint64_t>::const_iterator cell = block_cells.begin(); cell != block_cells.end(); cell++) {
		game_of_life_cell* cell_data = game_grid[*cell];
		if (cell_data == NULL) {
			continue;
		}
		cell_data->is_alive = true;
	}

	// create a beehive
	#define BEEHIVE_START 51
	uint64_t tmp6[] = {BEEHIVE_START - GRID_SIZE, BEEHIVE_START + 1, BEEHIVE_START + 2, BEEHIVE_START + 1 - 2 * GRID_SIZE, BEEHIVE_START + 2 - 2 * GRID_SIZE, BEEHIVE_START + 3 - GRID_SIZE};
	vector<uint64_t> beehive_cells(tmp6, tmp6 + sizeof(tmp6) / sizeof(uint64_t));
	for (vector<uint64_t>::const_iterator cell = beehive_cells.begin(); cell != beehive_cells.end(); cell++) {
		game_of_life_cell* cell_data = game_grid[*cell];
		if (cell_data == NULL) {
			continue;
		}
		cell_data->is_alive = true;
	}

	// every process outputs the game state into its own file
	ostringstream basename, suffix(".vtk");
	basename << "pinned_cells_" << comm.rank() << "_";
	ofstream outfile, visit_file;

	// visualize the game with visit -o game_of_life_test.visit
	if (comm.rank() == 0) {
		visit_file.open("pinned_cells.visit");
		visit_file << "!NBLOCKS " << comm.size() << endl;
		cout << "step: ";
		cout.flush();
	}

	#define TIME_STEPS 25
	for (int step = 0; step < TIME_STEPS; step++) {

		// refine random unrefined cells and unrefine random refined cells
		vector<uint64_t> cells = game_grid.get_cells();
		random_shuffle(cells.begin(), cells.end());

		if (step % 2 == 0) {

			for (int i = 0, refined = 0; i < int(cells.size()) && refined <= GRID_SIZE * GRID_SIZE / (5 * comm.size()); i++) {
				if (game_grid.get_refinement_level(cells[i]) == 0) {
					game_grid.refine_completely(cells[i]);
					refined++;
				}
			}

		} else {

			for (int i = 0, unrefined = 0; i < int(cells.size()) && unrefined <= GRID_SIZE * GRID_SIZE / (4 * comm.size()); i++) {
				if (game_grid.get_refinement_level(cells[i]) > 0) {
					game_grid.unrefine_completely(cells[i]);
					unrefined++;
				}
			}
		}

		vector<uint64_t> new_cells = game_grid.stop_refining();

		// assign parents' state to children
		for (vector<uint64_t>::const_iterator new_cell = new_cells.begin(); new_cell != new_cells.end(); new_cell++) {
			game_of_life_cell* new_cell_data = game_grid[*new_cell];
			if (new_cell_data == NULL) {
				cout << __FILE__ << ":" << __LINE__ << " no data for created cell " << *new_cell << endl;
				exit(EXIT_FAILURE);
			}
			game_of_life_cell* parent_data = game_grid[game_grid.get_parent(*new_cell)];
			if (parent_data == NULL) {
				cout << __FILE__ << ":" << __LINE__ << " no data for parent cell " << game_grid.get_parent(*new_cell) << endl;
				exit(EXIT_FAILURE);
			}
			new_cell_data->is_alive = parent_data->is_alive;
		}

		// "interpolate" parent cell's value from unrefined children
		vector<uint64_t> removed_cells = game_grid.get_removed_cells();
		for (vector<uint64_t>::const_iterator removed_cell = removed_cells.begin(); removed_cell != removed_cells.end(); removed_cell++) {
			game_of_life_cell* removed_cell_data = game_grid[*removed_cell];
			if (removed_cell_data == NULL) {
				cout << __FILE__ << ":" << __LINE__
					<< " no data for removed cell after unrefining: " << *removed_cell
					<< endl;
				abort();
			}
			game_of_life_cell* parent_data = game_grid[game_grid.get_parent_for_removed(*removed_cell)];
			if (parent_data == NULL) {
				cout << __FILE__ << ":" << __LINE__
					<< " no data for parent cell after unrefining: " << game_grid.get_parent_for_removed(*removed_cell)
					<< endl;
				abort();
			}
			parent_data->is_alive = removed_cell_data->is_alive;
		}
		game_grid.clear_refined_unrefined_data();

		// pin cells to process 0 either inside or outside the circle
		cells = game_grid.get_cells();
		for (vector<uint64_t>::const_iterator
			cell = cells.begin();
			cell != cells.end();
			cell++
		) {
			const double x = game_grid.get_cell_x(*cell);
			const double y = game_grid.get_cell_y(*cell);
			const double distance = (x - 0.5) * (x - 0.5) + (y - 0.5) * (y - 0.5);

			if (step % 2 == 0) {
				if (distance <= 0.3 * 0.3) {
					game_grid.pin(*cell, 0);
				} else {
					game_grid.unpin(*cell);
				}
			} else {
				if (distance <= 0.3 * 0.3) {
					game_grid.unpin(*cell);
				} else {
					game_grid.pin(*cell, 0);
				}
			}
		}

		game_grid.balance_load();
		game_grid.update_remote_neighbor_data();
		cells = game_grid.get_cells();
		// the library writes the grid into a file in ascending cell order, do the same for the grid data at every time step
		sort(cells.begin(), cells.end());

		if (comm.rank() == 0) {
			cout << step << " ";
			cout.flush();
		}

		// write the game state into a file named according to the current time step
		string current_output_name("");
		ostringstream step_string;
		step_string.fill('0');
		step_string.width(5);
		step_string << step;
		current_output_name += basename.str();
		current_output_name += step_string.str();
		current_output_name += suffix.str();

		// visualize the game with visit -o game_of_life_test.visit
		if (comm.rank() == 0) {
			for (int process = 0; process < comm.size(); process++) {
				visit_file << "pinned_cells_" << process << "_" << step_string.str() << suffix.str() << endl;
			}
		}

		// write the grid into a file
		game_grid.write_vtk_file(current_output_name.c_str());
		// prepare to write the game data into the same file
		outfile.open(current_output_name.c_str(), ofstream::app);
		outfile << "CELL_DATA " << cells.size() << endl;

		// go through the grids cells and write their state into the file
		outfile << "SCALARS is_alive float 1" << endl;
		outfile << "LOOKUP_TABLE default" << endl;
		for (vector<uint64_t>::const_iterator cell = cells.begin(); cell != cells.end(); cell++) {

			game_of_life_cell* cell_data = game_grid[*cell];

			if (cell_data->is_alive == true) {
				outfile << "1";
			} else {
				outfile << "0";
			}
			outfile << endl;

		}

		// write each cells total live neighbor count
		outfile << "SCALARS live_neighbor_count float 1" << endl;
		outfile << "LOOKUP_TABLE default" << endl;
		for (vector<uint64_t>::const_iterator cell = cells.begin(); cell != cells.end(); cell++) {

			game_of_life_cell* cell_data = game_grid[*cell];

			outfile << cell_data->total_live_neighbor_count << endl;

		}

		// write each cells neighbor count
		outfile << "SCALARS neighbors int 1" << endl;
		outfile << "LOOKUP_TABLE default" << endl;
		for (vector<uint64_t>::const_iterator cell = cells.begin(); cell != cells.end(); cell++) {
			const vector<uint64_t>* neighbors = game_grid.get_neighbors(*cell);
			outfile << neighbors->size() << endl;
		}

		// write each cells process
		outfile << "SCALARS process int 1" << endl;
		outfile << "LOOKUP_TABLE default" << endl;
		for (vector<uint64_t>::const_iterator cell = cells.begin(); cell != cells.end(); cell++) {
			outfile << comm.rank() << endl;
		}

		// write each cells id
		outfile << "SCALARS id int 1" << endl;
		outfile << "LOOKUP_TABLE default" << endl;
		for (vector<uint64_t>::const_iterator cell = cells.begin(); cell != cells.end(); cell++) {
			outfile << *cell << endl;
		}
		outfile.close();

		// get the neighbor counts of every cell
		for (vector<uint64_t>::const_iterator cell = cells.begin(); cell != cells.end(); cell++) {

			game_of_life_cell* cell_data = game_grid[*cell];
			if (cell_data == NULL) {
				cout << __FILE__ << ":" << __LINE__ << " no data for cell: " << *cell << endl;
				exit(EXIT_FAILURE);
			}

			cell_data->total_live_neighbor_count = 0;

			for (int i = 0; i < 3; i++) {
				cell_data->live_unrefined_neighbors[i] = 0;
			}

			for (int i = 0; i < 8; i++) {
				cell_data->child_of_processed[i] = 0;
			}

			const vector<uint64_t>* neighbors = game_grid.get_neighbors(*cell);
			// unrefined cells just consider neighbor counts at the level of unrefined cells
			if (game_grid.get_refinement_level(*cell) == 0) {

				for (vector<uint64_t>::const_iterator neighbor = neighbors->begin(); neighbor != neighbors->end(); neighbor++) {

					if (*neighbor == 0) {
						continue;
					}

					game_of_life_cell* neighbor_data = game_grid[*neighbor];
					if (neighbor_data == NULL) {
						cout << __FILE__ << ":" << __LINE__ << " no data for neighbor of cell " << *cell << ": " << *neighbor << endl;
						exit(EXIT_FAILURE);
					}

					if (game_grid.get_refinement_level(*neighbor) == 0) {
						if (neighbor_data->is_alive) {
							cell_data->total_live_neighbor_count++;
						}
					// consider only one sibling...
					} else {

						bool sibling_processed = false;
						uint64_t parent_of_neighbor = game_grid.get_parent(*neighbor);
						for (int i = 0; i < 8; i++) {
							if (cell_data->child_of_processed[i] == parent_of_neighbor) {
								sibling_processed = true;
								break;
							}
						}

						// ...by recording its parent
						if (sibling_processed) {
							continue;
						} else {
							for (int i = 0; i < 8; i++) {
								if (cell_data->child_of_processed[i] == 0) {
									cell_data->child_of_processed[i] = parent_of_neighbor;
									break;
								}
							}
						}

						if (neighbor_data->is_alive) {
							cell_data->total_live_neighbor_count++;
						}
					}
				}

			// refined cells total the neighbor counts of siblings
			} else {

				for (vector<uint64_t>::const_iterator neighbor = neighbors->begin(); neighbor != neighbors->end(); neighbor++) {

					if (*neighbor == 0) {
						continue;
					}

					game_of_life_cell* neighbor_data = game_grid[*neighbor];
					if (neighbor_data == NULL) {
						cout << __FILE__ << ":" << __LINE__ << " no data for neighbor of refined cell " << *cell << ": " << *neighbor << endl;
						exit(EXIT_FAILURE);
					}

					if (game_grid.get_refinement_level(*neighbor) == 0) {
						// TODO merge solver with the one in unrefined2d, etc
						// larger neighbors appear several times in the neighbor list
						bool neighbor_processed = false;
						for (int i = 0; i < 8; i++) {
							if (cell_data->child_of_processed[i] == *neighbor) {
								neighbor_processed = true;
								break;
							}
						}

						if (neighbor_processed) {
							continue;
						} else {
							for (int i = 0; i < 8; i++) {
								if (cell_data->child_of_processed[i] == 0) {
									cell_data->child_of_processed[i] = *neighbor;
									break;
								}
							}
						}

						if (neighbor_data->is_alive) {
							for (int i = 0; i < 3; i++) {
								if (cell_data->live_unrefined_neighbors[i] == 0) {
									cell_data->live_unrefined_neighbors[i] = *neighbor;
									break;
								}
							}
						}

					// consider only one sibling of all parents of neighboring cells...
					} else {

						// ignore own siblings
						if (game_grid.get_parent(*cell) == game_grid.get_parent(*neighbor)) {
							continue;
						}

						bool sibling_processed = false;
						uint64_t parent_of_neighbor = game_grid.get_parent(*neighbor);
						for (int i = 0; i < 8; i++) {
							if (cell_data->child_of_processed[i] == parent_of_neighbor) {
								sibling_processed = true;
								break;
							}
						}

						if (sibling_processed) {
							continue;
						} else {
							for (int i = 0; i < 8; i++) {
								if (cell_data->child_of_processed[i] == 0) {
									cell_data->child_of_processed[i] = parent_of_neighbor;
									break;
								}
							}
						}

						// ...by recording which parents have been considered
						if (neighbor_data->is_alive) {
							for (int i = 0; i < 3; i++) {
								if (cell_data->live_unrefined_neighbors[i] == 0) {
									cell_data->live_unrefined_neighbors[i] = parent_of_neighbor;
									break;
								}
							}
						}
					}
				}
			}

		}
		game_grid.update_remote_neighbor_data();

		// get the total neighbor counts of refined cells
		for (vector<uint64_t>::const_iterator cell = cells.begin(); cell != cells.end(); cell++) {

			if (game_grid.get_refinement_level(*cell) == 0) {
				continue;
			}
			game_of_life_cell* cell_data = game_grid[*cell];

			unordered_set<uint64_t> current_live_unrefined_neighbors;
			for (int i = 0; i < 3; i++) {
				current_live_unrefined_neighbors.insert(cell_data->live_unrefined_neighbors[i]);
			}

			const vector<uint64_t>* neighbors = game_grid.get_neighbors(*cell);
			for (vector<uint64_t>::const_iterator neighbor = neighbors->begin(); neighbor != neighbors->end(); neighbor++) {

				if (*neighbor == 0) {
					continue;
				}

				if (game_grid.get_refinement_level(*neighbor) == 0) {
					continue;
				}

				// total live neighbors counts only between siblings
				if (game_grid.get_parent(*cell) != game_grid.get_parent(*neighbor)) {
					continue;
				}

				game_of_life_cell* neighbor_data = game_grid[*neighbor];
				for (int i = 0; i < 3; i++) {
					current_live_unrefined_neighbors.insert(neighbor_data->live_unrefined_neighbors[i]);
				}
			}

			current_live_unrefined_neighbors.erase(0);
			cell_data->total_live_neighbor_count += current_live_unrefined_neighbors.size();
		}

		// calculate the next turn
		for (vector<uint64_t>::const_iterator cell = cells.begin(); cell != cells.end(); cell++) {

			game_of_life_cell* cell_data = game_grid[*cell];

			if (cell_data->total_live_neighbor_count == 3) {
				cell_data->is_alive = true;
			} else if (cell_data->total_live_neighbor_count != 2) {
				cell_data->is_alive = false;
			}
		}
	}

	if (comm.rank() == 0) {
		visit_file.close();
		cout << endl;
	}

	return EXIT_SUCCESS;
}
