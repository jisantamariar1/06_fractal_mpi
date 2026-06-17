#include <iostream>
#include <mpi.h>
#include <complex>
#include <vector>
#include <fmt/core.h>
#include <cstring> // Para memset
#include <SFML/Graphics.hpp>
#include "fractal_mpi.h"
//#include "arial_ttf.h"
#include "draw_text.h"

#ifdef _WIN32
    #include <windows.h>
#endif

namespace arial_ttf 
{
    extern size_t data_len;
    extern unsigned char data[];
}


double x_min = -1.5;
double x_max = 1.5;
double y_min = -1.0;
double y_max = 1.0;

int max_iteraciones = 10;

std::complex<double> c(-0.7, 0.27015);

#define WIDTH 1600
#define HEIGHT 900
uint32_t* pixel_buffer = nullptr;
uint32_t* texture_buffer = nullptr;

int running = 1;
int row_start;
int row_end;
int padding;
int delta;
int nprocs;
int rank;

std::string machine_name(){
    std::string mname = "";
#ifdef _WIN32
    char hostname[256];
    DWORD size = sizeof(hostname);
    GetComputerNameA(hostname, &size);
    mname = hostname;
#endif
    return mname;
}

void dibujar_texto(int rank){
    std::string pc = machine_name();
    auto texto = fmt::format("RANK_{} - PC: {}", rank, pc);

    draw_text_to_texture(
        (unsigned char*)pixel_buffer,
        WIDTH,delta,
        texto.c_str(),
        10,25,20);
}


void setup_ui(){
    texture_buffer = new uint32_t[WIDTH * HEIGHT];
    std::memset(texture_buffer, 0, WIDTH * HEIGHT * sizeof(uint32_t));

    //inicializar la UI
    sf::RenderWindow window(sf::VideoMode({WIDTH, HEIGHT}), "Fractal MPI");

    // Si es Windows, forzamos que la ventana se abra maximizada usando el handle nativo.
#ifdef _WIN32
    HWND hwnd = window.getNativeHandle(); 
    ShowWindow(hwnd, SW_MAXIMIZE);        
#endif
    sf::Texture texture({WIDTH, HEIGHT});
    texture.update((const uint8_t *)texture_buffer); 
    // El 'sprite' es el objeto que permite "dibujar" la textura en la ventana.
    sf::Sprite sprite(texture); 

    //textos
    const sf::Font font(arial_ttf::data, arial_ttf::data_len);

    sf::Text text(font, "Fractal", 24); 
    text.setFillColor(sf::Color::White); 
    text.setPosition({10, 10}); 
    text.setStyle(sf::Text::Bold); 

    std::string options = "Up/Down: Change iterations";
    sf::Text textOptions(font, options, 18);
    textOptions.setFillColor(sf::Color::White);
    textOptions.setStyle(sf::Text::Bold);
    textOptions.setPosition({10, window.getSize().y - 40}); // Posicionar en la parte inferior de la ventana.


    //fps
    int frames = 0;
    int fps = 0;
    sf::Clock clock; 
    while (window.isOpen())
    {
        // A. PROCESAR EVENTOS: Entrada del usuario.
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()){
                running = 0;
                window.close();
            }
            else if(event->is<sf::Event::KeyReleased>()) {
                auto evt = event->getIf<sf::Event::KeyReleased>();
                // Controlamos las iteraciones con las flechas del teclado.
                switch(evt->scancode) {
                    case sf::Keyboard::Scan::Up:
                        max_iteraciones += 10; // Más detalle.
                        break;
                    case sf::Keyboard::Scan::Down:
                        max_iteraciones -= 10; // Menos detalle (más rápido).
                        if(max_iteraciones < 10) max_iteraciones = 10;
                        break;
                    default:
                        break;
                }

                std::memset(texture_buffer, 0, WIDTH * HEIGHT * sizeof(uint32_t));
            }
        }
        //notificar a los otros ranks que la app se esta cerrando
        std::vector<int> dummy = {max_iteraciones,running};
        MPI_Bcast(dummy.data(), 2, MPI_INT, 0, MPI_COMM_WORLD);

        if(running == 0) {
            break;
        }
        //dibujar la porcion del rank 0
        julia_mpi(x_min, y_min, x_max, y_max, WIDTH, HEIGHT, row_start, row_end, pixel_buffer);
        //de momento no funciona mañana
        dibujar_texto(rank);
        //copiar el pixelbuffer a la textura
        std::memcpy(texture_buffer, pixel_buffer,WIDTH * delta * sizeof(uint32_t));
        
        //recibir las imagenes pariales de los otros tanks
        for(int i=1; i<nprocs; i++) {
            int new_delta = delta;
            if(i == nprocs-1) {
                new_delta = delta - padding;
            }
            MPI_Recv(
                pixel_buffer, 
                WIDTH * new_delta, 
                MPI_UNSIGNED, 
                i, 
                0, 
                MPI_COMM_WORLD, 
                MPI_STATUS_IGNORE
            );
            std::memcpy(texture_buffer + (i*new_delta*WIDTH), pixel_buffer, WIDTH * new_delta * sizeof(uint32_t));
        }

        //actualizar la textura
        texture.update((const uint8_t *)texture_buffer);
        frames++;

        // D. CÁLCULO DE FPS: Cada vez que pase 1 segundo, actualizamos el contador.
        if (clock.getElapsedTime().asSeconds() >= 1.0f){
            fps = frames;
            frames = 0;
            clock.restart();
        }

        // E. ACTUALIZAR HUD: Formateamos el mensaje de texto.
        //auto msg = fmt::format("Julia Set: Iterations: {}, FPS: {}, Mode: {}", max_iteraciones, fps, mode);
        //text.setString(msg);

        // F. RENDERIZADO:
        window.clear();      // Limpiar la pantalla (borrar el frame anterior).
        window.draw(sprite); // Dibujar el fractal (la textura).
        //window.draw(text);   // Dibujar el contador de FPS encima.
        //window.draw(textOptions); // Dibujar las opciones de control.
        window.display();    // Intercambiar buffers para mostrar el dibujo en el monitor.
    }

    // 5. LIMPIEZA:
    // Muy importante para evitar fugas de memoria al cerrar el programa.
    //delete[] pixel_buffer;
    

}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);
    //int nprocs;
    //int rank;

    //ranks
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    init_freetype();

    delta = std::ceil(HEIGHT*1.0 / nprocs); // 1600/4 = 400
    row_start = rank * delta; // 0, 400, 800, 1200
    row_end = row_start + delta; // 400, 800, 1200, 1600
    padding = delta*nprocs - HEIGHT; // 1600 - 1600 = 0

    if(row_end > HEIGHT) {
        row_end = HEIGHT;
    }

    pixel_buffer = new uint32_t[WIDTH * delta];
    std::memset(pixel_buffer, 0, WIDTH * delta * sizeof(uint32_t)); 

    fmt::println("RANK_{}: rows {} to {}", rank, row_start, row_end);

    if(rank==0){
        setup_ui();
        

    }
    else{
        //dibujar
        while(true){
            std::vector<int> dummy = {max_iteraciones,0};
             MPI_Bcast(dummy.data(), 2, MPI_INT, 0, MPI_COMM_WORLD);

            max_iteraciones = dummy[0];
            running = dummy[1];

            if(running == 0) {
                fmt::println("RANK_{}: received shutdown signal. Exiting.", rank);
                break;
            }
            
            julia_mpi(x_min, y_min, x_max, y_max, WIDTH, HEIGHT, row_start, row_end, pixel_buffer);
            dibujar_texto(rank);
            //enviar la porcion de la imagen
            MPI_Send(
                pixel_buffer, 
                WIDTH * delta, 
                MPI_UNSIGNED, 
                0, 
                0, 
                MPI_COMM_WORLD);
            if(rank == 1) {
                //fmt::println("RANK_{}: max_iteraciones{}", rank, max_iteraciones);
            }

             
        }


    }

    MPI_Finalize();
    return 0;
}