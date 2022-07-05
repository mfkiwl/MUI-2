/*****************************************************************************
 * Multiscale Universal Interface Code Coupling Library                       *
 *                                                                            *
 * Copyright (C) 2019 Y. H. Tang, S. Kudo, X. Bian, Z. Li, G. E. Karniadakis, *
 *                    A. Skillen, W. Liu, S. Longshaw                         *
 *                                                                            *
 * This software is jointly licensed under the Apache License, Version 2.0    *
 * and the GNU General Public License version 3, you may use it according     *
 * to either.                                                                 *
 *                                                                            *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 * http://www.apache.org/licenses/LICENSE-2.0                                 *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 * ** GNU General Public License, version 3 **                                *
 *                                                                            *
 * This program is free software: you can redistribute it and/or modify       *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation, either version 3 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.      *
 *****************************************************************************/

/**
 * @file sampler_rbf.h
 * @author A. Skillen, W. Liu, S. Longshaw
 * @date November 2018
 * @brief Spatial sampler using Gaussian Radial Basis Function interpolation.
 */

#ifndef MUI_SAMPLER_RBF_H_
#define MUI_SAMPLER_RBF_H_

#include "../util.h"
#include <Eigen/Sparse>
#include "../uniface.h"
#include <iterator>
#include <ctime>

namespace mui {

template<typename CONFIG = default_config,
typename O_TP = typename CONFIG::REAL, typename I_TP = O_TP>
class sampler_rbf {
#define MINPOINTSWARN 2
#define MAXPOINTSWARN 120
public:
	using OTYPE = O_TP;
	using ITYPE = I_TP;
	using REAL = typename CONFIG::REAL;
	using INT = typename CONFIG::INT;
	using point_type = typename CONFIG::point_type;
	using EXCEPTION = typename CONFIG::EXCEPTION;

	static const bool QUIET = CONFIG::QUIET;
	static const bool DEBUG = CONFIG::DEBUG;

	/**
	 * Input parameters:
	 * 1.  REAL r:
	 *       The search radius used to construct each RBF
	 * 2.  std::vector<point_type>& pts:
	 *       Vector of points that pre-set for RBF interpolation
	 * 3.  INT basisFunc:
	 *       Parameter for basis function selection. Implemented functions are as follows:
	 *       Gaussian(default): basisFunc_=0
	 *       Wendland's C0:     basisFunc_=1
	 *       Wendland's C2:     basisFunc_=2
	 *       Wendland's C4:     basisFunc_=3
	 *       Wendland's C6:     basisFunc_=4
	 * 4.  bool conservative:
	 *       Switch for the mode of RBF interpolation:
	 *       consistebuildMatrixConservativeNoPolynt mode(default): conservative=false
	 *       conservative mode:        conservative=true
	 * 5.  bool polynomial:
	 *       Switch for the polynomial term of the transformation matrix:
	 *       without polynomial terms(default): polynomial=false
	 *       with polynomial terms:             polynomial=true
	 * 6.  bool smoothFunc:
	 *       Switch for the smoothing function of the transformation matrix:
	 *       without smoothing function(default): smoothFunc=false
	 *       with smoothing function:             smoothFunc=true
	 * 7.  bool readMatrix:
	 *       Switch for whether to construct transformation matrix or read matrix from file:
	 *       construct transformation matrix and write it to file(default): readMatrix=false
	 *       read transformation matrix from file:                          readMatrix=true
	 * 8.  bool writeMatrix:
	 *       Switch for whether to write the constructed transformation matrix to file:
	 *       Write the constructed matrix to file:       writeMatrix=false
	 *       Don't write the constructed matrix to file: writeMatrix=true
	 * 9.  const std::string& fileAddress:
	 *       The address that the transformation matrix I/O uses.
	 *       The default value of fileAddress is an empty string.
	 * 10. REAL cutOff:
	 *       Parameter to set the cut-off of the Gaussian basis function (only valid for basisFunc_=0).
	 *       The default value of cutoff is 1e-9
	 * 11. REAL cgSolveTol:
	 *       The tolerance used to determine convergence for the Eigen ConjugateGradient solver
	 *       The default value of cgSolveTol is 1e-6
	 * 12. INT cgMaxIter:
	 *       The maximum number of iterations each Eigen ConjugateGradient solve can take
	 *       The default value of cgMaxIter is 0, which means the solver decides
	 * 13. INT pouSize:
	 *       The size of each partition used within the RBF-POU approach
	 *       The default value of pouSize is 50, setting to 0 disables the partitioned approach
	 */

	sampler_rbf(REAL r, const std::vector<point_type> &pts, INT basisFunc = 0,
		bool conservative = false, bool polynomial = false, bool smoothFunc = false,
		bool readMatrix = false, bool writeMatrix = true,
		const std::string &fileAddress = std::string(), REAL cutOff = 1e-9,
		REAL cgSolveTol = 1e-6, INT cgMaxIter = 0, INT pouSize = 50) :
		r_(r),
		initialised_(false),
		CABrow_(0),
		CABcol_(0),
		Hrow_(0),
		Hcol_(0),
		conservative_(conservative),
		consistent_(!conservative),
		pouEnabled_(true),
		polynomial_(polynomial),
		smoothFunc_(smoothFunc),
		readMatrix_(readMatrix),
		writeMatrix_(writeMatrix),
		fileAddress_(fileAddress),
		cgSolveTol_(cgSolveTol),
		cgMaxIter_(cgMaxIter),
		N_sp_(pouSize),
		M_ap_(pouSize),
		basisFunc_(basisFunc),
		pts_(pts) {
			//set s to give rbf(r)=cutOff (default 1e-9)
			s_ = std::pow(-std::log(cutOff), 0.5) / r_;
			twor_ = r_ * r_;
			// Ensure Eigen solver parameters are sensible
			if ( cgMaxIter_ < 0 )
				cgMaxIter_ = 0;
			if( cgSolveTol_ <= 0 )
				cgSolveTol_ = std::numeric_limits<REAL>::epsilon();
			// Disable Partition of Unity (PoU) approach if parameter set to zero
			if( pouSize == 0 )
				pouEnabled_ = false;
	}

	template<template<typename, typename > class CONTAINER>
	inline OTYPE filter(point_type focus, const CONTAINER<ITYPE, CONFIG> &data_points) {
		if ( !initialised_ ) {
			const clock_t begin_time = clock();
			computeRBFtransformation(data_points);
			if (!QUIET) {
				std::cout << "MUI [sampler_rbf.h]: Matrices generated in "
						<< static_cast<double>(clock() - begin_time) / CLOCKS_PER_SEC << "s"
						<< std::endl;
			}
		}

		auto p = std::find_if(pts_.begin(), pts_.end(), [focus](point_type b) {
			return normsq(focus - b) < std::numeric_limits<REAL>::epsilon();
		});

		if ( p == std::end(pts_) )
			EXCEPTION(std::runtime_error("Point not found. Must pre-set points for RBF interpolation"));

		auto i = std::distance(pts_.begin(), p);

		OTYPE sum = 0.;
		for (size_t j = 0; j < data_points.size(); j++) {
			sum += H_(i, j) * data_points[j].second;
		}

		return sum;
	}

	inline geometry::any_shape<CONFIG> support(point_type focus, REAL domain_mag) const {
		return geometry::any_shape<CONFIG>();
	}

	void preSetFetchPoints(std::vector<point_type> &pts) {
		pts_ = pts;
		initialised_ = false;
	}

	void addFetchPoint(point_type pt) {
		pts_.emplace_back(pt);
		initialised_ = false;
	}

private:
	template<template<typename, typename > class CONTAINER>
	void computeRBFtransformation(const CONTAINER<ITYPE, CONFIG> &data_points) {
		// If PoU enabled and problem size is smaller than initial PoU values
		// then refine to actual problem set
		if ( pouEnabled_ ) {
			if ( conservative_ ) {
				if ( pts_.size() <= static_cast<size_t>(N_sp_) )
					N_sp_ = pts_.size();
			}
			else {
				if (data_points.size() <= static_cast<size_t>(N_sp_))
					N_sp_ = data_points.size();
			}

			if ( smoothFunc_ ) {
				if ( pts_.size() <= static_cast<size_t>(M_ap_) )
					M_ap_ = (pts_.size() - 1);
			}
		}
		else { //PoU disabled so set to global sizes
			if ( conservative_ )

		}

		// Reading matrix
		if ( readMatrix_ )
			readMatrix();
		else { // Generating matrix
			if( consistent_ )
				buildConnectivityConsistent(data_points, N_sp_);
			else
				buildConnectivityConservative(data_points, N_sp_);

			H_.resize(pts_.size(), data_points.size());
			H_.setZero();

			if ( smoothFunc_ ) {
				buildConnectivityAA (M_ap_);
				H_toSmooth_.resize(pts_.size(), data_points.size());
				H_toSmooth_.setZero();
			}

			if ( writeMatrix_ ) {
				std::ofstream outputFileMatrixSize(fileAddress_ + "/matrixSize.dat");

				if ( !outputFileMatrixSize ) {
					std::cerr << "Could not locate the file address of matrixSize.dat!"
							<< std::endl;
				}
				else {
					outputFileMatrixSize
							<< "// *********************************************************************************************************************************************";
					outputFileMatrixSize << "\n";
					outputFileMatrixSize
							<< "// **** This is the 'matrixSize.dat' file of the RBF spatial sampler of the MUI library";
					outputFileMatrixSize << "\n";
					outputFileMatrixSize
							<< "// **** This file contains the size (number of rows and number of columns) of the Point Connectivity Matrix (N) and the Coupling Matrix (H).";
					outputFileMatrixSize << "\n";
					outputFileMatrixSize
							<< "// **** The file uses the Comma-Separated Values (CSV) format and the ASCII format with the meanings as follows: ";
					outputFileMatrixSize << "\n";
					outputFileMatrixSize
							<< "// ****			The number of rows of the Point Connectivity Matrix (N), ";
					outputFileMatrixSize << "\n";
					outputFileMatrixSize
							<< "// ****			The number of columns of the Point Connectivity Matrix (N),";
					outputFileMatrixSize << "\n";
					outputFileMatrixSize
							<< "// ****			The number of rows of the Point Connectivity Matrix (M) (for smoothing), ";
					outputFileMatrixSize << "\n";
					outputFileMatrixSize
							<< "// ****			The number of columns of the Point Connectivity Matrix (M) (for smoothing),";
					outputFileMatrixSize << "\n";
					outputFileMatrixSize
							<< "// ****			The number of rows of the Coupling Matrix (H),";
					outputFileMatrixSize << "\n";
					outputFileMatrixSize
							<< "// ****			The number of columns of the Coupling Matrix (H)";
					outputFileMatrixSize << "\n";
					outputFileMatrixSize
							<< "// *********************************************************************************************************************************************";
					outputFileMatrixSize << "\n";
					outputFileMatrixSize << "//  ";
					outputFileMatrixSize << "\n";
					outputFileMatrixSize << connectivityAB_.size();
					outputFileMatrixSize << ",";
					outputFileMatrixSize << connectivityAB_[0].size();
					outputFileMatrixSize << ",";
					if ( smoothFunc_ ) {
						outputFileMatrixSize << connectivityAA_.size();
						outputFileMatrixSize << ",";
						outputFileMatrixSize << connectivityAA_[0].size();
						outputFileMatrixSize << ",";
					}
					else {
						outputFileMatrixSize << "0";
						outputFileMatrixSize << ",";
						outputFileMatrixSize << "0";
						outputFileMatrixSize << ",";
					}
					outputFileMatrixSize << H_.rows();
					outputFileMatrixSize << ",";
					outputFileMatrixSize << H_.cols();
					outputFileMatrixSize << "\n";
				}
			}

			if ( consistent_ && polynomial_ ) {
				if ( polynomial_ )
					buildMatrixConsistentPoly(data_points, smoothFunc_, pouEnabled_);
				else
					buildMatrixConsistentNoPoly(data_points, smoothFunc_, pouEnabled_);
			}
			else {
				if ( polynomial_ )
					buildMatrixConservativePoly(data_points, smoothFunc_, pouEnabled_);
				else
					buildMatrixConservativeNoPoly(data_points, smoothFunc_, pouEnabled_);
			}

			if ( writeMatrix_ ) {
				const static Eigen::IOFormat CSVFormat(Eigen::StreamPrecision, Eigen::DontAlignCols, ", ", "\n");

				std::ofstream outputFileHMatrix(fileAddress_ + "/Hmatrix.dat");

				if ( !outputFileHMatrix )
					std::cerr << "Could not locate the file address of Hmatrix.dat!" << std::endl;
				else {
					outputFileHMatrix
							<< "// ************************************************************************************************";
					outputFileHMatrix << "\n";
					outputFileHMatrix
							<< "// **** This is the 'Hmatrix.dat' file of the RBF spatial sampler of the MUI library";
					outputFileHMatrix << "\n";
					outputFileHMatrix
							<< "// **** This file contains the entire matrix of the Coupling Matrix (H).";
					outputFileHMatrix << "\n";
					outputFileHMatrix
							<< "// **** The file uses the Comma-Separated Values (CSV) format with ASCII for the entire H matrix";
					outputFileHMatrix << "\n";
					outputFileHMatrix
							<< "// ************************************************************************************************";
					outputFileHMatrix << "\n";
					outputFileHMatrix << "// ";
					outputFileHMatrix << "\n";
					outputFileHMatrix << H_.format(CSVFormat);
				}
			}
		}
		initialised_ = true;
	}

	template<template<typename, typename > class CONTAINER>
	inline void buildMatrixConsistentPoly(const CONTAINER<ITYPE, CONFIG> &data_points, bool smoothing, bool pouEnabled) {
		// Set problem set size depending on whether Partition of Unity approach enabled
		size_t pts_N_Remote = (pouEnabled? N_sp_: data_points.size());

		for ( size_t row = 0; row < pts_.size(); row++ ) {
			Eigen::SparseMatrix<REAL> Css; //< Matrix of radial basis function evaluations between prescribed points
			Eigen::Matrix<REAL, Eigen::Dynamic, 1> Aas; //< Matrix of RBF evaluations between prescribed and interpolation points

			Css.resize((1 + pts_N_Remote + CONFIG::D), (1 + pts_N_Remote + CONFIG::D));
			Css.setZero();
			Aas.resize((1 + pts_N_Remote + CONFIG::D), 1);
			Aas.setZero();

			//set Css
			std::vector<Eigen::Triplet<REAL>> coefsC;
			for ( INT i = 0; i < pts_N_Remote; i++ ) {
				for ( INT j = i; j < pts_N_Remote; j++ ) {
					int glob_i = connectivityAB_[row][i];
					int glob_j = connectivityAB_[row][j];

					auto d = norm(data_points[glob_i].first - data_points[glob_j].first);

					if ( d < r_ ) {
						REAL w = rbf(d);
						coefsC.emplace_back(Eigen::Triplet<REAL> (i, j, w));
						if ( i != j )
							coefsC.emplace_back(Eigen::Triplet<REAL> (j, i, w));
					}
				}
			}

			for ( INT i = 0; i < pts_N_Remote; i++ ) {
				coefsC.emplace_back(Eigen::Triplet < REAL > (i, pts_N_Remote, 1));
				coefsC.emplace_back(Eigen::Triplet < REAL > (pts_N_Remote, i, 1));

				int glob_i = connectivityAB_[row][i];

				for ( INT dim = 0; dim < CONFIG::D; dim++ ) {
					coefsC.emplace_back(Eigen::Triplet<REAL> (i, (pts_N_Remote + dim + 1), data_points[glob_i].first[dim]));
					coefsC.emplace_back(Eigen::Triplet<REAL> ((pts_N_Remote + dim + 1), i, data_points[glob_i].first[dim]));
				}
			}

			Css.reserve(coefsC.size());
			Css.setFromTriplets(coefsC.begin(), coefsC.end());

			for ( INT j = 0; j < pts_N_Remote; j++ ) {
				int glob_j = connectivityAB_[row][j];

				auto d = norm(pts_[row] - data_points[glob_j].first);

				if ( d < r_ )
					Aas(j, 0) = rbf(d);
			}

			Aas(pts_N_Remote, 0) = 1;
			for (int dim = 0; dim < CONFIG::D; dim++) {
				Aas(pts_N_Remote + dim + 1, 0) = pts_[row][dim];
			}

			Eigen::ConjugateGradient<Eigen::SparseMatrix<REAL>,
					Eigen::Lower | Eigen::Upper,
					Eigen::DiagonalPreconditioner<REAL>> solver(Css);
			if ( cgMaxIter_ != 0 )
				solver.setMaxIterations(cgMaxIter_);
			solver.setTolerance(cgSolveTol_);

			Eigen::Matrix<REAL, Eigen::Dynamic, Eigen::Dynamic> H_i = solver.solve(Aas);

			if ( DEBUG ) {
				std::cout << "#iterations of H_i:     "
					<< solver.iterations()
					<< ". Error of H_i: " << solver.error()
					<< std::endl;
			}

			for ( INT j = 0; j < pts_N_Remote; j++ ) {
				int glob_j = connectivityAB_[row][j];
				H_(row, glob_j) = H_i(j);
			}
			if ( smoothing ) {
				for ( INT j = 0; j < pts_N_Remote; j++ ) {
					int glob_j = connectivityAB_[row][j];
					H_toSmooth_(row, glob_j) = H_i(j);
				}
			}
			else {
				for ( INT j = 0; j < pts_N_Remote; j++ ) {
					int glob_j = connectivityAB_[row][j];
					H_(row, glob_j) = H_i( j);
				}
			}
		}

		if ( smoothing ) {
			for ( size_t row = 0; row < pts_.size(); row++ ) {
				for ( INT j = 0; j < pts_N_Remote; j++ ) {
					int glob_j = connectivityAB_[row][j];
					REAL h_j_sum = 0.;
					REAL f_sum = 0.;

					for ( INT k = 0; k < M_ap_; k++ ) {
						INT row_k = connectivityAA_[row][k];
						if ( row_k == static_cast<INT>(row) ) {
							std::cerr << "Invalid row_k value: "
									<< row_k << std::endl;
						}
						else
							h_j_sum += std::pow(dist_h_i(row, row_k), -2.);
					}

					for ( INT k = 0; k < M_ap_; k++ ) {
						INT row_k = connectivityAA_[row][k];
						if ( row_k == static_cast<INT>(row) ) {
							std::cerr << "Invalid row_k value: "
									<< row_k << std::endl;
						}
						else {
							REAL w_i = ((std::pow(dist_h_i(row, row_k), -2.)) / (h_j_sum));
							f_sum += w_i * H_toSmooth_(row_k, glob_j);
						}
					}

					H_(row, glob_j) = 0.5 * (f_sum + H_toSmooth_(row, glob_j));
				}
			}
		}
	}

	template<template<typename, typename > class CONTAINER>
	inline void buildMatrixConsistentNoPoly(const CONTAINER<ITYPE, CONFIG> &data_points, bool smoothing, bool pouEnabled) {
		// Set problem set size depending on whether Partition of Unity approach enabled
		size_t pts_N_Remote = (pouEnabled? N_sp_: data_points.size());

		for ( size_t row = 0; row < pts_.size(); row++ ) {
			Eigen::SparseMatrix<REAL> AA; //< Matrix of radial basis function evaluations between prescribed points
			Eigen::Matrix<REAL, Eigen::Dynamic, 1> AB;

			AA.resize(pts_N_Remote, pts_N_Remote);
			AA.setZero();
			AB.resize(pts_N_Remote, 1);
			AB.setZero();

			std::vector<Eigen::Triplet<REAL> > coefs;

			//set AA
			for ( INT i = 0; i < pts_N_Remote; i++ ) {
				for (INT j = i; j < pts_N_Remote; j++ ) {
					int glob_i = connectivityAB_[row][i];
					int glob_j = connectivityAB_[row][j];

					auto d = norm(data_points[glob_i].first - data_points[glob_j].first);

					if ( d < r_ ) {
						REAL w = rbf(d);
						coefs.emplace_back(Eigen::Triplet<REAL> (i, j, w));
						if (i != j)
							coefs.emplace_back(Eigen::Triplet<REAL> (j, i, w));
					}
				}
			}

			AA.reserve(coefs.size());
			AA.setFromTriplets(coefs.begin(), coefs.end());

			//set AB
			for ( INT j = 0; j < pts_N_Remote; j++ ) {
				int glob_j = connectivityAB_[row][j];

				auto d = norm(pts_[row] - data_points[glob_j].first);

				if ( d < r_ ) {
					AB(j, 0) = rbf(d);
				}
			}

			Eigen::ConjugateGradient<Eigen::SparseMatrix<REAL>,
					Eigen::Lower | Eigen::Upper,
					Eigen::DiagonalPreconditioner<REAL>> solver(AA);
			if ( cgMaxIter_ != 0 )
				solver.setMaxIterations(cgMaxIter_);
			solver.setTolerance(cgSolveTol_);

			Eigen::Matrix<REAL, Eigen::Dynamic, Eigen::Dynamic> H_i = solver.solve(AB);

			if (DEBUG) {
				std::cout
						<< "MUI [sampler_rbf.h]: H_i iteration count: "
						<< solver.iterations()
						<< "                          H_i error: "
						<< solver.error() << std::endl;
			}

			if ( smoothing ) {
				for ( INT j = 0; j < pts_N_Remote; j++ ) {
					int glob_j = connectivityAB_[row][j];
					H_toSmooth_(row, glob_j) = H_i(0, j);
				}
			}
			else {
				for ( INT j = 0; j < pts_N_Remote; j++ ) {
					int glob_j = connectivityAB_[row][j];
					H_(row, glob_j) = H_i(j);
				}
			}
		}

		if ( smoothing ) {
			for ( size_t row = 0; row < pts_.size(); row++ ) {
				for ( INT j = 0; j < pts_N_Remote; j++ ) {
					int glob_j = connectivityAB_[row][j];
					REAL h_j_sum = 0.;
					REAL f_sum = 0.;

					for ( INT k = 0; k < M_ap_; k++ ) {
						INT row_k = connectivityAA_[row][k];
						if ( row_k == static_cast<INT>(row) )
							std::cerr << "Invalid row_k value: " << row_k << std::endl;
						else
							h_j_sum += std::pow(dist_h_i(row, row_k), -2.);
					}

					for ( INT k = 0; k < M_ap_; k++ ) {
						INT row_k = connectivityAA_[row][k];
						if ( row_k == static_cast<INT>(row) )
							std::cerr << "Invalid row_k value: " << row_k << std::endl;
						else {
							REAL w_i = ((std::pow(dist_h_i(row, row_k), -2.)) / (h_j_sum));
							f_sum += w_i * H_toSmooth_(row_k, glob_j);
						}
					}

					H_(row, glob_j) = 0.5 * (f_sum + H_toSmooth_(row, glob_j));
				}
			}
		}
	}

	template<template<typename, typename > class CONTAINER>
	inline void buildMatrixConservativePoly(const CONTAINER<ITYPE, CONFIG> &data_points, bool smoothing, bool pouEnabled) {
		// Set problem set size depending on whether Partition of Unity approach enabled
		size_t pts_N_Local = (pouEnabled? N_sp_: pts_.size());
		size_t pts_N_Remote = (pouEnabled? N_sp_: data_points.size());

		Eigen::SparseMatrix<REAL> Css; //< Matrix of radial basis function evaluations between prescribed points
		Css.resize((1 + pts_N_Local + CONFIG::D), (1 + pts_N_Local + CONFIG::D));
		Css.setZero();

		Eigen::SparseMatrix<REAL> Aas; //< Matrix of RBF evaluations between prescribed and interpolation points
		Aas.resize(pts_N_Remote, (1 + pts_N_Local + CONFIG::D));
		Aas.setZero();

		std::vector<Eigen::Triplet<REAL> > coefsC;

		//set Css
		for ( size_t i = 0; i < pts_N_Local; i++ ) {
			for ( size_t j = i; j < pts_N_Local; j++ ) {
				auto d = norm(pts_[i] - pts_[j]);

				if ( d < r_ ) {
					REAL w = rbf(d);
					coefsC.emplace_back(Eigen::Triplet<REAL> ((i + CONFIG::D + 1), (j + CONFIG::D + 1), w));

					if ( i != j )
						coefsC.emplace_back(Eigen::Triplet<REAL> ((j + CONFIG::D + 1), (i + CONFIG::D + 1), w));
				}
			}
		}

		Css.reserve(coefsC.size());
		Css.setFromTriplets(coefsC.begin(), coefsC.end());

		//set Aas
		std::vector<Eigen::Triplet<REAL> > coefs;

		for ( size_t i = 0; i < pts_N_Remote; i++ ) {
			for ( size_t j = 0; j < pts_N_Local; j++ ) {
				auto d = norm(data_points[i].first - pts_[j]);

				if ( d < r_ ) {
					coefs.emplace_back(Eigen::Triplet<REAL> (i, (j + CONFIG::D + 1), rbf(d)));
				}
			}
		}

		Aas.reserve(coefs.size());
		Aas.setFromTriplets(coefs.begin(), coefs.end());

		//invert Css
		Eigen::ConjugateGradient<Eigen::SparseMatrix<REAL>,
				Eigen::Lower | Eigen::Upper,
				Eigen::DiagonalPreconditioner<REAL>> solver(Css);
		if ( cgMaxIter_ != 0 )
			solver.setMaxIterations(cgMaxIter_);
		solver.setTolerance(cgSolveTol_);

		Eigen::SparseMatrix<REAL> AasTrans = Aas.transpose();
		Eigen::Matrix<REAL, Eigen::Dynamic, Eigen::Dynamic> H_more = solver.solve(AasTrans);

		if (DEBUG) {
			std::cout << "#iterations of H_more:     "
					<< solver.iterations()
					<< ". Error of H_more: " << solver.error()
					<< std::endl;
		}

		if ( smoothing ) {
			for ( size_t i = 0; i < pts_.size(); i++ ) {
				for (size_t j = 0; j < pts_N_Remote; j++ ) {
					H_toSmooth_(i, j) = H_more((i + CONFIG::D + 1), j);
				}
			}
		}
		else {
			for ( size_t i = 0; i < pts_.size(); i++ ) {
				for ( size_t j = 0; j < pts_N_Remote; j++ ) {
					H_(i, j) = H_more((i + CONFIG::D + 1), j);
				}
			}
		}

		if ( smoothing ) {
			for ( size_t row = 0; row < pts_.size(); row++ ) {
				for ( size_t j = 0; j < pts_N_Remote; j++ ) {
					REAL h_j_sum = 0.;
					REAL f_sum = 0.;
					for ( INT k = 0; k < M_ap_; k++ ) {
						INT row_k = connectivityAA_[row][k];
						if ( row_k == static_cast<INT>(row) )
							std::cerr << "Invalid row_k value: " << row_k << std::endl;
						else
							h_j_sum += std::pow(dist_h_i(row, row_k), -2.);
					}

					for ( INT k = 0; k < M_ap_; k++ ) {
						INT row_k = connectivityAA_[row][k];
						if ( row_k == static_cast<INT>(row) )
							std::cerr << "Invalid row_k value: " << row_k << std::endl;
						else {
							REAL w_i = ((std::pow(dist_h_i(row, row_k), -2.)) / (h_j_sum));
							f_sum += w_i * H_toSmooth_(row_k, j);
						}
					}

					H_(row, j) = 0.5 * (f_sum + H_toSmooth_(row, j));
				}
			}
		}
	}

	template<template<typename, typename > class CONTAINER>
	inline void buildMatrixConservativeNoPoly(const CONTAINER<ITYPE, CONFIG> &data_points, bool smoothing, bool pouEnabled) {
		// Set problem set size depending on whether Partition of Unity approach enabled
		size_t pts_N_Local = (pouEnabled? N_sp_: pts_.size());

		for ( size_t row = 0; row < data_points.size(); row++ ) {
			Eigen::SparseMatrix<REAL> AA; //< Matrix of radial basis function evaluations between prescribed points
			AA.resize(pts_N_Local, pts_N_Local);
			AA.setZero();

			Eigen::Matrix<REAL, Eigen::Dynamic, 1> AB; //< Matrix of RBF evaluations between prescribed and interpolation points
			AB.resize(pts_N_Local, 1);
			AB.setZero();

			std::vector<Eigen::Triplet<REAL> > coefs;

			//set AA
			for ( size_t i = 0; i < pts_N_Local; i++ ) {
				for ( size_t j = i; j < pts_N_Local; j++ ) {
					int glob_i = connectivityAB_[row][i];
					int glob_j = connectivityAB_[row][j];

					auto d = norm(pts_[glob_i] - pts_[glob_j]);

					if ( d < r_ ) {
						REAL w = rbf(d);
						coefs.emplace_back(Eigen::Triplet<REAL> (i, j, w));
						if ( i != j )
							coefs.emplace_back(Eigen::Triplet<REAL> (j, i, w));
					}
				}
			}

			AA.reserve(coefs.size());
			AA.setFromTriplets(coefs.begin(), coefs.end());

			//set AB
			for ( size_t j = 0; j < pts_N_Local; j++ ) {
				int glob_j = connectivityAB_[row][j];

				auto d = norm(data_points[row].first - pts_[glob_j]);

				if ( d < r_ )
					AB(j,0) = rbf(d);
			}

			Eigen::ConjugateGradient<Eigen::SparseMatrix<REAL>,
					Eigen::Lower | Eigen::Upper,
					Eigen::DiagonalPreconditioner<REAL>> solver(AA);
			if ( cgMaxIter_ != 0 )
				solver.setMaxIterations(cgMaxIter_);
			solver.setTolerance(cgSolveTol_);

			Eigen::Matrix<REAL, Eigen::Dynamic, Eigen::Dynamic> H_j = solver.solve(AB);

			if (DEBUG) {
				std::cout << "#iterations of H_j:     "
						<< solver.iterations()
						<< ". Error of H_j: " << solver.error()
						<< std::endl;
			}

			if ( smoothing ) {
				for ( size_t i = 0; i < pts_N_Local; i++ ) {
					int glob_i = connectivityAB_[row][i];
					H_toSmooth_(glob_i, row) = H_j(i, 0);
				}
			}
			else {
				for ( size_t i = 0; i < pts_N_Local; i++ ) {
					int glob_i = connectivityAB_[row][i];
					H_(glob_i, row) = H_j(i, 0);
				}
			}
		}

		if ( smoothing ) {
			for ( size_t row = 0; row < data_points.size(); row++ ) {
				for ( size_t i = 0; i < pts_N_Local; i++ ) {
					int glob_i = connectivityAB_[row][i];
					REAL h_j_sum = 0.;
					REAL f_sum = 0.;

					for ( INT k = 0; k < M_ap_; k++ ) {
						INT global_k = connectivityAA_[glob_i][k];
						if ( global_k == glob_i )
							std::cerr << "Invalid global_k value: " << global_k << std::endl;
						else
							h_j_sum += std::pow(dist_h_i(glob_i, global_k), -2.);
					}

					for ( INT k = 0; k < M_ap_; k++ ) {
						INT global_k = connectivityAA_[glob_i][k];
						if ( global_k == glob_i )
							std::cerr << "Invalid global_k value: " << global_k << std::endl;
						else {
							REAL w_i = ((std::pow(dist_h_i(glob_i, global_k), -2.))	/ (h_j_sum));
							f_sum += w_i * H_toSmooth_(global_k, row);
						}
					}

					H_(glob_i, row) = 0.5 * (f_sum + H_toSmooth_(glob_i, row));
				}
			}
		}
	}

	template<template<typename, typename > class CONTAINER>
	inline void buildConnectivityConsistent(const CONTAINER<ITYPE, CONFIG> &data_points, const INT NP) {
		std::ofstream outputFileCAB;
		if ( writeMatrix_ ) {
			outputFileCAB.open(fileAddress_ + "/connectivityAB.dat");

			if ( !outputFileCAB ) {
				std::cerr << "Could not locate the file address on the connectivityAB.dat"
						<< std::endl;
			}
			else {
				outputFileCAB
						<< "// ************************************************************************************************";
				outputFileCAB << "\n";
				outputFileCAB
						<< "// **** This is the 'connectivityAB.dat' file of the RBF spatial sampler of the MUI library";
				outputFileCAB << "\n";
				outputFileCAB
						<< "// **** This file contains the entire matrix of the Point Connectivity Matrix (N).";
				outputFileCAB << "\n";
				outputFileCAB
						<< "// **** The file uses the Comma-Separated Values (CSV) format with ASCII for the entire N matrix";
				outputFileCAB << "\n";
				outputFileCAB
						<< "// ************************************************************************************************";
				outputFileCAB << "\n";
				outputFileCAB << "// ";
				outputFileCAB << "\n";
			}
		}

		INT pointsCountGlobalMax = std::numeric_limits<INT>::min();
		INT pointsCountGlobalMin = std::numeric_limits<INT>::max();

		connectivityAB_.resize(pts_.size());

		for ( size_t i = 0; i < pts_.size(); i++ ) {
			INT pointsCount = 0;
			for ( INT n = 0; n < NP; n++ ) {
				REAL cur = std::numeric_limits<REAL>::max();
				INT bestj = -1;
				for ( size_t j = 0; j < data_points.size(); j++ ) {
					auto added = std::find_if(connectivityAB_[i].begin(), connectivityAB_[i].end(), [j](INT k) {
						return static_cast<size_t>(k) == j;
					});

					if ( added != connectivityAB_[i].end() )
						continue;

					auto d = normsq(pts_[i] - data_points[j].first);
					if ( d < cur ) {
						cur = d;
						bestj = j;
					}

					if ( n == 0 && d < twor_ )
						pointsCount++;
				}

				connectivityAB_[i].emplace_back(bestj);

				if ( writeMatrix_ && (n < NP - 1) )
					outputFileCAB << bestj << ",";
				else if ( writeMatrix_ )
					outputFileCAB << bestj;
			}

			if ( writeMatrix_ && i < pts_.size() - 1 )
				outputFileCAB << '\n';
			if ( pointsCount < pointsCountGlobalMin )
				pointsCountGlobalMin = pointsCount;
			if ( pointsCount > pointsCountGlobalMax )
				pointsCountGlobalMax = pointsCount;
		}
		if ( !QUIET &&
			 (pointsCountGlobalMin < MINPOINTSWARN || pointsCountGlobalMax > MAXPOINTSWARN)) {
			std::cout << "MUI Warning [sampler_rbf.h]: RBF search radius not producing optimal point patches ("
					<< MINPOINTSWARN << "-" << MAXPOINTSWARN << "), found ("
					<< pointsCountGlobalMin << "-" << pointsCountGlobalMax
					<< ")" << std::endl;
		}

		if ( writeMatrix_ )
			outputFileCAB.close();
	}

	template<template<typename, typename > class CONTAINER>
	inline void buildConnectivityConservative(const CONTAINER<ITYPE, CONFIG> &data_points, const INT NP) {
		std::ofstream outputFileCAB;
		if ( writeMatrix_ ) {
			outputFileCAB.open(fileAddress_ + "/connectivityAB.dat");

			if ( !outputFileCAB ) {
				std::cerr << "Could not locate the file address on the connectivityAB.dat"
						<< std::endl;
			}
			else {
				outputFileCAB
						<< "// ************************************************************************************************";
				outputFileCAB << "\n";
				outputFileCAB
						<< "// **** This is the 'connectivityAB.dat' file of the RBF spatial sampler of the MUI library";
				outputFileCAB << "\n";
				outputFileCAB
						<< "// **** This file contains the entire matrix of the Point Connectivity Matrix (N).";
				outputFileCAB << "\n";
				outputFileCAB
						<< "// **** The file uses the Comma-Separated Values (CSV) format with ASCII for the entire N matrix";
				outputFileCAB << "\n";
				outputFileCAB
						<< "// ************************************************************************************************";
				outputFileCAB << "\n";
				outputFileCAB << "// ";
				outputFileCAB << "\n";
			}
		}

		INT pointsCountGlobalMax = std::numeric_limits<INT>::min();
		INT pointsCountGlobalMin = std::numeric_limits<INT>::max();

		connectivityAB_.resize(data_points.size());

		for ( size_t i = 0; i < data_points.size(); i++ ) {
			INT pointsCount = 0;
			for ( size_t n = 0; n < pts_.size(); n++ ) {
				REAL cur = std::numeric_limits<REAL>::max();
				INT bestj = -1;
				for ( size_t j = 0; j < pts_.size(); j++ ) {
					auto added = std::find_if(connectivityAB_[i].begin(), connectivityAB_[i].end(), [j](INT k) {
						return static_cast<size_t>(k) == j;
					});

					if ( added != connectivityAB_[i].end() )
						continue;

					auto d = normsq(data_points[i].first - pts_[j]);
					if ( d < cur ) {
						cur = d;
						bestj = j;
					}

					if ( (n == 0) && (d < twor_) )
						pointsCount++;
				}

				connectivityAB_[i].emplace_back(bestj);

				if ( writeMatrix_ && n < pts_.size() - 1 )
					outputFileCAB << bestj << ",";
				else if ( writeMatrix_ )
					outputFileCAB << bestj;
			}

			if ( writeMatrix_ && i < pts_.size() - 1 )
				outputFileCAB << '\n';
			if ( pointsCount < pointsCountGlobalMin )
				pointsCountGlobalMin = pointsCount;
			if ( pointsCount > pointsCountGlobalMax )
				pointsCountGlobalMax = pointsCount;
		}
		if ( !QUIET &&
			 (pointsCountGlobalMin < MINPOINTSWARN || pointsCountGlobalMax > MAXPOINTSWARN)) {
			std::cout << "MUI Warning [sampler_rbf.h]: RBF search radius not producing optimal point patches ("
					<< MINPOINTSWARN << "-" << MAXPOINTSWARN << "), found ("
					<< pointsCountGlobalMin << "-" << pointsCountGlobalMax
					<< ")" << std::endl;
		}

		if ( writeMatrix_ )
			outputFileCAB.close();
	}

	inline void buildConnectivityAA(const INT MP) {
		std::ofstream outputFileCAA;
		if (writeMatrix_) {
			outputFileCAA.open(fileAddress_ + "/connectivityAA.dat");

			if (!outputFileCAA) {
				std::cerr << "Could not locate the file address on the connectivityAA.dat!"
						<< std::endl;
			}
			else {
				outputFileCAA
						<< "// ************************************************************************************************";
				outputFileCAA << "\n";
				outputFileCAA
						<< "// **** This is the 'connectivityAA.dat' file of the RBF spatial sampler of the MUI library";
				outputFileCAA << "\n";
				outputFileCAA
						<< "// **** This file contains the entire matrix of the Point Connectivity Matrix (M) (for smoothing).";
				outputFileCAA << "\n";
				outputFileCAA
						<< "// **** The file uses the Comma-Separated Values (CSV) format with ASCII for the entire N matrix";
				outputFileCAA << "\n";
				outputFileCAA
						<< "// ************************************************************************************************";
				outputFileCAA << "\n";
				outputFileCAA << "// ";
				outputFileCAA << "\n";
			}
		}

		connectivityAA_.resize(pts_.size());

		for ( size_t i = 0; i < pts_.size(); i++ ) {
			for ( INT n = 0; n < MP; n++ ) {
				REAL cur = std::numeric_limits<REAL>::max();
				INT bestj = -1;
				for ( size_t j = 0; j < pts_.size(); j++ ) {
					if ( i == j )
						continue;

					auto added = std::find_if(connectivityAA_[i].begin(), connectivityAA_[i].end(), [j](INT i) {
						return static_cast<size_t>(i) == j;
					});

					if ( added != connectivityAA_[i].end() )
						continue;

					auto d = normsq(pts_[i] - pts_[j]);
					if ( d < cur ) {
						cur = d;
						bestj = j;
					}
				}

				connectivityAA_[i].emplace_back(bestj);

				if ( writeMatrix_ && n < MP - 1 )
					outputFileCAA << bestj << ",";
				else if ( writeMatrix_ )
					outputFileCAA << bestj;
			}
			if ( writeMatrix_ && i < pts_.size() - 1 )
				outputFileCAA << '\n';
		}
		if ( writeMatrix_ )
			outputFileCAA.close();
	}

	//Radial basis function for two points
	inline REAL rbf(point_type x1, point_type x2) {
		auto d = norm(x1 - x2);
		return rbf(d);
	}

	//Radial basis function for calculated distance
	inline REAL rbf(REAL d) {
		switch ( basisFunc_ ) {
			case 0:
				//Gaussian
				return (d < r_) ? std::exp(-(s_ * s_ * d * d)) : 0.;
			case 1:
				//Wendland's C0
				return std::pow((1. - d), 2.);
			case 2:
				//Wendland's C2
				return (std::pow((1. - d), 4.)) * ((4. * d) + 1.);
			case 3:
				//Wendland's C4
				return (std::pow((1. - d), 6)) * ((35. * d * d) + (18. * d) + 3.);
			case 4:
				//Wendland's C6
				return (std::pow((1. - d), 8.)) * ((32. * d * d * d) + (25. * d * d) + (8. * d) + 1.);
			default:
				std::cerr << "MUI Error [sampler_rbf.h]: invalid RBF basis function number ("
						<< basisFunc_ << ")" << std::endl
						<< "Please set the RBF basis function number (basisFunc_) as: "
						<< std::endl << "basisFunc_=0 (Gaussian); " << std::endl
						<< "basisFunc_=1 (Wendland's C0); " << std::endl
						<< "basisFunc_=2 (Wendland's C2); " << std::endl
						<< "basisFunc_=3 (Wendland's C4); " << std::endl
						<< "basisFunc_=4 (Wendland's C6); " << std::endl;
				return 0;
		}
	}

	///Distances function
	inline REAL dist_h_i(INT pts_i, INT pts_j) {
		switch ( CONFIG::D ) {
			case 1:
				return std::sqrt((std::pow((pts_[pts_i][0] - pts_[pts_j][0]), 2.)));
			case 2:
				return std::sqrt((std::pow((pts_[pts_i][0] - pts_[pts_j][0]), 2.))
						+ (std::pow((pts_[pts_i][1] - pts_[pts_j][1]), 2.)));
			case 3:
				return std::sqrt((std::pow((pts_[pts_i][0] - pts_[pts_j][0]), 2.))
						+ (std::pow((pts_[pts_i][1] - pts_[pts_j][1]), 2.))
						+ (std::pow((pts_[pts_i][2] - pts_[pts_j][2]), 2.)));
			default:
				std::cerr << "CONFIG::D must equal 1-3" << std::endl;
				return 0.;
		}
	}

	inline void readMatrix() {
	std::ifstream inputFileMatrixSize(fileAddress_ + "/matrixSize.dat");

	if ( !inputFileMatrixSize ) {
		std::cerr << "Could not locate the file address of matrixSize.dat"
				<< std::endl;
	}
	else {
		std::string tempS;
		std::vector<INT> tempV;
		while ( std::getline(inputFileMatrixSize, tempS) ) {
			// Skips the line if the first two characters are '//'
			if ( tempS[0] == '/' && tempS[1] == '/' ) continue;
			std::stringstream lineStream(tempS);
			std::string tempSS;
			while ( std::getline(lineStream, tempSS, ',') ) {
				tempV.emplace_back(std::stoi(tempSS));
			}
		}
		CABrow_ = tempV[0];
		CABcol_ = tempV[1];
		CAArow_ = tempV[2];
		CAAcol_ = tempV[3];
		Hrow_ = tempV[4];
		Hcol_ = tempV[5];
	}

	std::ifstream inputFileCAB(fileAddress_ + "/connectivityAB.dat");

	if ( !inputFileCAB ) {
		std::cerr << "Could not locate the file address on the connectivityAB.dat"
				<< std::endl;
	}
	else {
		connectivityAB_.resize(CABrow_);
		for ( INT i = 0; i < CABrow_; i++ ) {
			connectivityAB_[i].resize(CABcol_, -1);
			std::string tempS;
			while ( std::getline(inputFileCAB, tempS) ) {
				// Skips the line if the first two characters are '//'
				if ( tempS[0] == '/' && tempS[1] == '/' ) continue;
				std::stringstream lineStream(tempS);
				std::string tempSS;
				std::vector<INT> tempV;
				while (std::getline(lineStream, tempSS, ',')) {
					tempV.emplace_back(std::stoi(tempSS));
				}
				connectivityAB_.emplace_back(tempV);
			}
		}
	}

	if ( smoothFunc_ ) {
		std::ifstream inputFileCAA(fileAddress_ + "/connectivityAA.dat");

		if ( !inputFileCAA ) {
			std::cerr << "Could not locate the file address on the connectivityAA.dat"
					<< std::endl;
		}
		else {
			if ( (CAArow_ == 0) || (CAAcol_ == 0) ) {
				std::cerr << "Error on the size of connectivityAA matrix in matrixSize.dat. Number of rows: "
						<< CAArow_ << " number of columns: " << CAAcol_
						<< ". Make sure matrices were generated with the smoothing function switched on."
						<< std::endl;
			}
			else {
				connectivityAA_.resize(CAArow_);

				for ( INT i = 0; i < CAArow_; i++ ) {
					connectivityAA_[i].resize(CAAcol_, -1);
					std::string tempS;
					while ( std::getline(inputFileCAA, tempS) ) {
						// Skips the line if the first two characters are '//'
						if ( tempS[0] == '/' && tempS[1] == '/' ) continue;
						std::stringstream lineStream(tempS);
						std::string tempSS;
						std::vector<INT> tempV;
						while ( std::getline(lineStream, tempSS, ',') ) {
							tempV.emplace_back(std::stoi(tempSS));
						}
						connectivityAA_.emplace_back(tempV);
					}
				}
			}
		}
	}

	H_.resize(Hrow_, Hcol_);
	H_.setZero();

	std::ifstream inputFileHMatrix(fileAddress_ + "/Hmatrix.dat");

	if (!inputFileHMatrix) {
		std::cerr << "Could not locate the file address on the Hmatrix.dat"
				<< std::endl;
	}
	else {
		std::string tempS;
		int tempRow = 0;
		int tempPoints = 0;
		while ( std::getline(inputFileHMatrix, tempS) ) {
			// Skips the line if the first two characters are '//'
			if ( tempS[0] == '/' && tempS[1] == '/' ) continue;
			std::stringstream lineStream(tempS);
			std::string tempSS;
			int tempCol = 0;
			while ( std::getline(lineStream, tempSS, ',') ) {
				H_(tempRow, tempCol) = std::stod(tempSS);
				tempCol++;
				tempPoints++;
			}
			tempRow++;
		}

		if ( (tempRow != Hrow_) || ((tempPoints / tempRow) != Hcol_) ) {
			std::cerr << "tempRow (" << tempRow
					<< ") is not NOT equal to Hrow_ (" << Hrow_
					<< "), or" << std::endl << "(tempPoints/tempRow) ("
					<< (tempPoints / tempRow)
					<< ") is not NOT equal to Hcol_ (" << Hcol_ << ")"
					<< std::endl;
		}
	}
}

protected:
REAL r_;
REAL twor_;
REAL s_;

bool initialised_;
bool pouEnabled_;
INT CABrow_;
INT CABcol_;
INT CAArow_;
INT CAAcol_;
INT Hrow_;
INT Hcol_;
const bool conservative_;
const bool consistent_;
const bool polynomial_;
const bool smoothFunc_;
const bool readMatrix_;
const bool writeMatrix_;
const INT basisFunc_;
const std::string fileAddress_;

INT N_sp_;
INT M_ap_;

INT cgMaxIter_;
REAL cgSolveTol_;

const std::vector<point_type> pts_;
Eigen::Matrix<REAL, Eigen::Dynamic, Eigen::Dynamic> H_; //< Transformation Matrix
Eigen::Matrix<REAL, Eigen::Dynamic, Eigen::Dynamic> H_toSmooth_;

std::vector<std::vector<INT> > connectivityAB_;
std::vector<std::vector<INT> > connectivityAA_;
}
;
}

#endif /* MUI_SAMPLER_RBF_H_ */
