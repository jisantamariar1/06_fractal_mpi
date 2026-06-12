// 1. LAS GUARDAS DE INCLUSIÓN (Include Guards)
// Si NO está definido el símbolo FRACTAL_MPI_H...
#ifndef FRACTAL_MPI_H 
// ...entonces defínelo ahora mismo.
#define FRACTAL_MPI_H 

// 2. LIBRERÍAS EXTERNAS
// Incluye tipos de enteros con tamaño exacto (como uint32_t para los colores).
#include <cstdint> 


void julia_mpi(double x_min, double y_min, double x_max, double y_max, 
    uint32_t width, uint32_t height, 
    uint32_t row_start, uint32_t row_end,
    uint32_t* pixel_buffer
);

#endif