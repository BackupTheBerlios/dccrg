/*
Cell indexing related parameters and functions of dccrg.

Copyright 2009, 2010, 2011, 2012 Finnish Meteorological Institute

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License version 3
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DCCRG_INDEX_HPP
#define DCCRG_INDEX_HPP

#include "cmath"
#include "iostream"
#include "stdint.h"

#include "dccrg_types.hpp"

namespace dccrg {

// Indicates a non-existing cell or an error when dealing with cells
static const uint64_t error_cell = 0;

// Indicates a non-existing index or an error when dealing with indices
static const uint64_t error_index = std::numeric_limits<uint64_t>::max();


class Index
{

public:

	/*!
	Creates and sets the grid to a default size of 1 cell with maximum refinement level 0.
	*/
	Index()
	{
		this->max_refinement_level = 0;
		this->x_length = this->y_length = this->z_length = this->grid_length = 1;
		this->last_cell = 1;
	}

	/*!
	Sets the grid to a default size of 1 cell with maximum refinement level 0.
	*/
	~Index()
	{
		this->max_refinement_level = 0;
		this->x_length = this->y_length = this->z_length = this->grid_length = 1;
		this->last_cell = 1;
	}

	/*!
	\brief Sets the size of the grid in unrefined cells.

	Returns true if successful, probably invalidating all previous cell information (cell numbers, indices, etc.)
	Returns false if unsuccessful and in that case has no effect.
	Automatically maximizes the maximum refinement level of the grid.
	*/
	bool set_length(
		const uint64_t given_x_length,
		const uint64_t given_y_length,
		const uint64_t given_z_length
	) {
		// TODO: switch to boost::array<uint64_t, Dimensions> given_length
		if (given_x_length == 0
		|| given_y_length == 0
		|| given_z_length == 0) {
			std::cerr << "All lengths given must be > 0 but are "
				<< given_x_length << " "
				<< given_y_length << " "
				<< given_z_length << std::endl;
			return false;
		}

		uint64_t old_x_length = this->x_length;
		uint64_t old_y_length = this->y_length;
		uint64_t old_z_length = this->z_length;

		this->x_length = given_x_length;
		this->y_length = given_y_length;
		this->z_length = given_z_length;

		if (double(this->x_length) + double(this->x_length) + double(this->x_length) > double(~uint64_t(0))) {
			std::cerr << "Grid would have too many unrefined cells for uint64_t (x_length, y_length, z_length): "
				<< this->x_length << " " << this->y_length << " " << this->z_length
				<< std::endl;

			this->x_length = old_x_length;
			this->y_length = old_y_length;
			this->z_length = old_z_length;
			return false;
		}

		uint64_t old_grid_length = this->grid_length;
		this->grid_length = this->x_length * this->y_length * this->z_length;

		if (this->max_refinement_level > this->get_maximum_possible_refinement_level()) {
			std::cerr << "Grid would have too many cells for an uint64_t with current refinement level" << std::endl;
			this->x_length = old_x_length;
			this->y_length = old_y_length;
			this->z_length = old_z_length;
			this->grid_length = old_grid_length;
			return false;
		} else {
			this->update_last_cell();
			return true;
		}
	}


	/*!
	Returns length of the grid in unrefined cells in x direction.
	*/
	uint64_t get_x_length(void) const
	{
		return this->x_length;
	}

	/*!
	Returns length of the grid in unrefined cells in y direction.
	*/
	uint64_t get_y_length(void) const
	{
		return this->y_length;
	}

	/*!
	Returns length of the grid in unrefined cells in z direction.
	*/
	uint64_t get_z_length(void) const
	{
		return this->z_length;
	}


	/*!
	Returns the maximum refinement level of any cell in the grid (0 means unrefined).
	*/
	int get_maximum_refinement_level(void) const
	{
		return this->max_refinement_level;
	}

	/*!
	Sets the maximum refinement level of the grid (0 means unrefined), probably invalidating all previous cell indices.
	Returns true if successful, otherwise does nothing and returns false.
	*/
	bool set_maximum_refinement_level(const int given_refinement_level)
	{
		if (given_refinement_level <= this->get_maximum_possible_refinement_level()) {
			this->max_refinement_level = given_refinement_level;
			this->update_last_cell();
			return true;
		} else {
			return false;
		}
	}


	/*!
	Returns the cell of given refinement level at given indices.

	Returns error_cell if any of given indices is invalid.
	*/
	uint64_t get_cell_from_indices(const Types<3>::indices_t& indices, const int refinement_level) const
	{
		if (indices[0] >= this->x_length * (uint64_t(1) << this->max_refinement_level)) {
			return error_cell;
		}

		if (indices[1] >= this->y_length * (uint64_t(1) << this->max_refinement_level)) {
			return error_cell;
		}

		if (indices[2] >= this->z_length * (uint64_t(1) << this->max_refinement_level)) {
			return error_cell;
		}

		if (refinement_level < 0) {
			return error_cell;
		}

		if (refinement_level > this->max_refinement_level) {
			return error_cell;
		}

		// cell numbering starts at 1
		uint64_t cell = 1;

		// add ids of larger cells
		for (int i = 0; i < refinement_level; i++) {
			cell += this->x_length * this->y_length * this->z_length * (uint64_t(1) << (i * 3));
		}

		// convert to indices of this cell's refinement level
		const Types<3>::indices_t this_level_indices = {{
			indices[0] / (uint64_t(1) << (this->max_refinement_level - refinement_level)),
			indices[1] / (uint64_t(1) << (this->max_refinement_level - refinement_level)),
			indices[2] / (uint64_t(1) << (this->max_refinement_level - refinement_level))
		}};

		// get the size of the grid in terms of cells of this refinement level
		const uint64_t this_level_x_length = this->x_length * (uint64_t(1) << refinement_level);
		const uint64_t this_level_y_length = this->y_length * (uint64_t(1) << refinement_level);

		cell += this_level_indices[0] + this_level_indices[1] * this_level_x_length + this_level_indices[2] * this_level_x_length * this_level_y_length;

		return cell;
	}

	/*!
	Same as the version taking indices_t as an argument.
	*/
	uint64_t get_cell_from_indices(
		const uint64_t x_index,
		const uint64_t y_index,
		const uint64_t z_index,
		const int refinement_level
	) const
	{
		const Types<3>::indices_t indices = {{x_index, y_index, z_index}};
		return this->get_cell_from_indices(indices, refinement_level);
	}


	/*!
	Returns the indices of given cell.

	See the definition of indices_t for an explanation about them.
	Returned indices are invalid if given a cell outside the valid range.
	*/
	Types<3>::indices_t get_indices(uint64_t cell) const
	{
		if (cell == error_cell || cell > this->last_cell) {
			const Types<3>::indices_t error_indices = {{error_index, error_index, error_index}};
			return error_indices;
		}

		// substract ids of larger cells
		const int refinement_level = this->get_refinement_level(cell);
		for (int i = 0; i < refinement_level; i++) {
			cell -= this->x_length * this->y_length * this->z_length * (uint64_t(1) << (i * 3));
		}

		cell -= 1;	// cell numbering starts from 1
		const Types<3>::indices_t indices = {{
			(cell % (this->x_length * (uint64_t(1) << refinement_level))) * (uint64_t(1) << (max_refinement_level - refinement_level)),
			((cell / (this->x_length * (uint64_t(1) << refinement_level))) % (this->y_length * (uint64_t(1) << refinement_level))) * (uint64_t(1) << (max_refinement_level - refinement_level)),
			(cell / (this->x_length * this->y_length * (uint64_t(1) << (2 * refinement_level)))) * (uint64_t(1) << (max_refinement_level - refinement_level))
		}};

		return indices;
	}


	/*!
	Returns the refinement level of given cell (0 means unrefined).

	Returns -1 if given an invalid cell.
	*/
	int get_refinement_level(const uint64_t cell) const
	{
		if (cell == error_cell || cell > this->last_cell) {
			return -1;
		}

		int refinement_level = 0;
		uint64_t current_last = 0;

		while (refinement_level <= this->max_refinement_level) {
			current_last += this->x_length * this->y_length * this->z_length * (uint64_t(1) << 3 * refinement_level);

			if (cell <= current_last) {
				break;
			}

			refinement_level++;
		}

		if (refinement_level > this->max_refinement_level) {
			return -1;
		}

		return refinement_level;
	}


	/*!
	Returns the lengths of given cell in indices in every direction.

	Returns error_index if given an invalid cell.
	*/
	uint64_t get_cell_size_in_indices(const uint64_t cell) const
	{
		if (cell == error_cell) {
			return error_index;
		}

		const int refinement_level = this->get_refinement_level(cell);

		if (refinement_level < 0) {
			return error_index;
		}

		return uint64_t(1) << (this->max_refinement_level - refinement_level);
	}


	/*!
	Returns the maximum possible refinement level for a cell in the grid (0 means unrefined).

	Assumes up to date grid_length.
	*/
	int get_maximum_possible_refinement_level(void) const
	{
		int refinement_level = 0;
		double current_last = 0;
		while (current_last <= double(~uint64_t(0))) {
			// TODO: don't assume 3 dimensions
			current_last += double(this->grid_length) * pow(double(8), double(refinement_level));
			refinement_level++;
		}

		return refinement_level - 2;
	}


	/*!
	Returns the parent of given cell.

	Returns the given cell if its refinement level == 0.
	Returns error_cell if given cell's refinement level > maximum refinement level or < 0.
	*/
	uint64_t get_parent_for_removed(const uint64_t cell) const
	{
		const int refinement_level = this->get_refinement_level(cell);

		if (refinement_level < 0
		|| refinement_level > this->max_refinement_level) {
			return error_cell;
		}

		if (refinement_level == 0) {
			return cell;
		}

		return this->get_cell_from_indices(this->get_indices(cell), refinement_level - 1);
	}



protected:

	// size of the grid in unrefined cells
	// TODO: switch to boost::array<uint64_t, Dimensions> length
	uint64_t x_length, y_length, z_length;

	// x_length * y_length * z_length
	uint64_t grid_length;

	// maximum refinemet level of any cell in the grid, 0 means unrefined
	int max_refinement_level;

	// last valid cell with these lengths and maximum_refinement_level
	uint64_t last_cell;


private:

	/*!
	Set the value of last_cell based on current grid lengths and max_refinement_level.

	Assumes up to date grid_length.
	*/
	void update_last_cell(void)
	{
		this->last_cell = 0;
		for (int i = 0; i <= this->max_refinement_level; i++) {
			this->last_cell += this->grid_length * (uint64_t(1) << (i * 3));
		}
	}


};	// class

}	// namespace

#endif

