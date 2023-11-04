// Author: APD team, except where source was noted
// Copyright Maria-Miruna Aldica, 331CA, 2023
#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define CONTOUR_CONFIG_COUNT    16
#define FILENAME_MAX_SIZE       50
#define STEP                    8
#define SIGMA                   200
#define RESCALE_X               2048
#define RESCALE_Y               2048

#define CLAMP(v, min, max) if(v < min) { v = min; } else if(v > max) { v = max; }

// Function defined to find out the start and the end for the parallel implementation
void start_end_params(unsigned int N, unsigned int thread_id, unsigned int NO_THREADS,
    unsigned int * const start, unsigned int * const end)
{
    const unsigned int w = N / NO_THREADS + 1;
    *start = w * thread_id; // Start parameter
    *end = *start + w; // End parameter
    if (*end > N) // Condition for choosing the end value
        *end = N;
}

// Struct defined to hold the data necessary for parallel processing
typedef struct {
   ppm_image *image; // Original image
    ppm_image **map; // Contour map
    ppm_image **scaled_image; // Scaled image
    unsigned char **grid; // Sample grid
    unsigned int num_threads; // Total number of threads
    pthread_barrier_t *barrier; // The barrier
    unsigned int step_x; // Delta(x)
    unsigned int step_y; // Delta(y)
    unsigned int p; // Grid dimension (1st)
    unsigned int q; // Grid dimension (2nd)
    unsigned int id; // Thread's id
} ThreadData;

ppm_image **init_contour_map(ThreadData *argument) {
    unsigned int start, end;
    start_end_params(CONTOUR_CONFIG_COUNT, argument->id, argument->num_threads, &start, &end);
    // Parallelized code structure
    for (int i = start; i < end; i++) {
        char filename[FILENAME_MAX_SIZE];
        sprintf(filename, "./contours/%d.ppm", i);
        argument->map[i] = read_ppm(filename);
    }
    return argument->map; // Returning the current map
}

ppm_image *rescale_image(ThreadData *argument) {
    uint8_t sample[3];
    unsigned int start, end; 
    // The parameters used to make the parallel implememtation on the "for" instruction
    if (argument->image->x <= RESCALE_X && argument->image->y <= RESCALE_Y) {
        (*argument->scaled_image) = argument->image;
    } else {
        start_end_params((*argument->scaled_image)->y, argument->id, argument->num_threads, &start, &end);
        // Parallelized code structure
        for (int i = 0; i < (*argument->scaled_image)->x; i++) {
            for (int j = start; j < end; j++) {
                float u = (float)i / (float)((*argument->scaled_image)->x - 1);
                float v = (float)j / (float)((*argument->scaled_image)->y - 1);
                sample_bicubic(argument->image, u, v, sample);
                (*argument->scaled_image)->data[i * (*argument->scaled_image)->y + j].red = sample[0];
                (*argument->scaled_image)->data[i * (*argument->scaled_image)->y + j].green = sample[1];
                (*argument->scaled_image)->data[i * (*argument->scaled_image)->y + j].blue = sample[2];
            }
        }
    }
    return NULL;
}

unsigned char **sample_grid(ppm_image *image, unsigned char sigma, ThreadData *argument) {
    unsigned int start, end;
    start_end_params(argument->p, argument->id, argument->num_threads, &start, &end);
    // Parallelized code structure
    for (int i = start; i < end; i++) {
        for (int j = 0; j < argument->q; j++) {
            ppm_pixel curr_pixel = (*argument->scaled_image)->data[i * argument->step_x * (*argument->scaled_image)->y + j * argument->step_y];
            unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

            if (curr_color > SIGMA) {
                argument->grid[i][j] = 0;
            } else {
                argument->grid[i][j] = 1;
            }
        }
    }
    (argument->grid)[argument->p][argument->q] = 0;

    // Parallelized code structure
    for (int i = start; i < end; i++) {
        ppm_pixel curr_pixel = (*argument->scaled_image)->data[i * argument->step_x * (*argument->scaled_image)->y + (*argument->scaled_image)->x - 1];
        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > SIGMA) {
            argument->grid[i][argument->q] = 0;
        } else {
            argument->grid[i][argument->q] = 1;
        }
    }

    start_end_params(argument->q, argument->id, argument->num_threads, &start, &end);
    // Parallelized code structure
    for (int j = start; j < end; j++) {
        ppm_pixel curr_pixel = (*argument->scaled_image)->data[((*argument->scaled_image)->x - 1) * (*argument->scaled_image)->y + j * argument->step_y];
        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > SIGMA) {
            argument->grid[argument->p][j] = 0;
        } else {
            argument->grid[argument->p][j] = 1;
        }
    }
    return argument->grid; // Returning the new grid
}

void update_image(ppm_image *image, ppm_image *contour, int x, int y) {
    for (int i = 0; i < contour->x; i++) {
        for (int j = 0; j < contour->y; j++) {
            int contour_pixel_index = contour->x * i + j;
            int image_pixel_index = (x + i) * image->y + y + j;

            image->data[image_pixel_index].red = contour->data[contour_pixel_index].red;
            image->data[image_pixel_index].green = contour->data[contour_pixel_index].green;
            image->data[image_pixel_index].blue = contour->data[contour_pixel_index].blue;
        }
    }
} 

void *march(ThreadData *argument) {

    unsigned int start, end; 
    start_end_params(argument->p * argument->q, argument->id, argument->num_threads, &start, &end);
    int i, j; 
    // Parallelized code structure
    for(int s = start; s < end; s++) {
        i = s /argument->q;
        j = s % argument->q;
        unsigned char k = 8 * argument->grid[i][j] + 4 * argument->grid[i][j + 1] + 2 * argument->grid[i + 1][j + 1] + 1 * argument->grid[i + 1][j];
        update_image(*argument->scaled_image, argument->map[k], i * STEP, j * STEP);
    }
}

// Thread Function
void *final_function(void *arg) {
    ThreadData argument = *(ThreadData *)arg;

    argument.map = init_contour_map(&argument); 
    pthread_barrier_wait(argument.barrier); // Barrier 

    rescale_image(&argument); // The image is going to be rescaled
    pthread_barrier_wait(argument.barrier); // Barrier

    argument.grid = sample_grid(*argument.scaled_image, SIGMA, &argument); // The new grid
    pthread_barrier_wait(argument.barrier); // Barrier

    march(&argument); // Updating the image
    
    return NULL;
}

// Function used to free the resources
void free_resources(int p, ppm_image **map, ppm_image *image, ppm_image **scaled_image, unsigned char **grid, ThreadData **argument) {
    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++) {
        free(map[i]->data);
        free(map[i]); 
    }
    free(map); 

    for (int i = 0; i <= p; i++) {
        free(grid[i]);
    }
    free(grid);

    if(image != *scaled_image) {
        free((*scaled_image)->data);
        free(*scaled_image);
    }
    free(scaled_image);
    free(image->data);
    free(image);

    int n = argument[0]->num_threads;
    for(int i = 0; i < n; i++) {
        free(argument[i]);
    }
    free(argument); // Free the structure that contains all the thread's data
}


int main(int argc, char *argv[]) {
    unsigned int p;
    unsigned int q;
    unsigned int r;
    unsigned int step_x = STEP;
    unsigned int step_y = STEP;
    
    if (argc < 4) {
        fprintf(stderr, "Usage: ./tema1 <in_file> <out_file> <P>\n");
        return 1;
    }

    ppm_image *image = read_ppm(argv[1]); // The initial image
    unsigned int num_threads = atoi(argv[3]); // Number of threads
    ThreadData **argument = (ThreadData **)malloc(num_threads * sizeof(ThreadData)); // The structure 

    pthread_barrier_t barrier; // Barrier
    r = pthread_barrier_init(&barrier, NULL, num_threads); // Barrier's init
    
    ppm_image **map;
    map = malloc(CONTOUR_CONFIG_COUNT * sizeof(ppm_image *));
    ppm_image **scaled_image = (ppm_image *)malloc(sizeof(ppm_image *));

    *scaled_image = (ppm_image *)malloc(sizeof(ppm_image));
    if (!scaled_image) {
        fprintf(stderr, "Unable to allocate memory\n"); // Error message
        exit(1);
    }
    if ((*scaled_image)->x <= RESCALE_X && (*scaled_image)->y <= RESCALE_Y) {
        (*scaled_image)->x = image->x;
        (*scaled_image)->y = image->y;
    } else {
        (*scaled_image)->x = RESCALE_X;
        (*scaled_image)->y = RESCALE_Y;
    }
    (*scaled_image)->data = (ppm_pixel*)malloc((*scaled_image)->x * (*scaled_image)->y * sizeof(ppm_pixel));
    if (!(*scaled_image)->data) {
        fprintf(stderr, "Unable to allocate memory\n"); // Error message
        exit(1);
    }

    p = (*scaled_image)->x / step_x;
    q = (*scaled_image)->y / step_y;
    unsigned char **grid = (unsigned char **)malloc((p + 1) * sizeof(unsigned char*));
    if (!grid) {
        fprintf(stderr, "Unable to allocate memory\n"); // Error message
        exit(1);
    }

    for (int i = 0; i <= p; i++) {
        grid[i] = (unsigned char *)malloc((q + 1) * sizeof(unsigned char));
        if (!grid[i]) {
            fprintf(stderr, "Unable to allocate memory\n"); // Error message
            exit(1);
        }
    }

    pthread_t *threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t)); // The threads
    if(!threads) {
        printf("An error has occured. Please, try again!"); // // Error message
        exit(1);
    }

    for(int i = 0; i < num_threads; i++) {
        argument[i] = (ThreadData *)malloc(sizeof(ThreadData)); // Alloc memory for each argument
        argument[i]->barrier = &barrier; 
        argument[i]->id = i;
        argument[i]->p = p;
        argument[i]->q = q;
        argument[i]->map = map;
        argument[i]->image = image;
        argument[i]->scaled_image = scaled_image;
        argument[i]->grid = grid;
        argument[i]->num_threads = num_threads;
        argument[i]->step_x = step_x;
        argument[i]->step_y = step_y;
        r = pthread_create(&threads[i], NULL, final_function, (void *)argument[i]); // Creating the thread's
        if(r) {
            printf("Unable to create thread %d.\n", i); // Error message
            exit(-1);
        }
    }

    for(int i = 0; i < num_threads; i++) {
        r = pthread_join(threads[i], NULL); // Joining threads
        if(r) {
            printf("Unable to wait thread %d.\n", i); // // Error message
            exit(-1);
        }
    }
  
    write_ppm(*scaled_image, argv[2]); // Writing the result
    r = pthread_barrier_destroy(&barrier); // Destroying the barrier
    if(threads)
        free(threads); // Free the thread's memory
    free_resources(p, map, image, scaled_image, grid, argument); // Free all the resources
    return 0;
}

