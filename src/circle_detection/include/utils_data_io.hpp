#ifndef UTILS_DATAIO_HPP
#define UTILS_DATAIO_HPP

#include <iostream>
#include <Eigen/Eigen>
#include <string>
#include <vector>
#include "utils_eigen.hpp"

namespace common_tools
{

inline void save_matrix_to_txt( std::string file_name, eigen_mat< -1, -1 > mat )
{

    FILE *fp = fopen( file_name.c_str(), "w+" );
    int   cols_size = mat.cols();
    int   rows_size = mat.rows();
    for ( int i = 0; i < rows_size; i++ )
    {
        for ( int j = 0; j < cols_size; j++ )
        {
            fprintf( fp, "%.15f ", mat( i, j ) );
        }
        fprintf( fp, "\r\n" );
    }
    // cout <<"Save matrix to: "  << file_name << endl;
    fclose( fp );
}
} // namespace common_tools

#endif // UTILS_DATAIO_HPP
