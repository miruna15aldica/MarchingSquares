# tema1

# Tema 1 APD
# Aldica Maria-Miruna, 331CA

To parallelize the code, I have created a structure (ThreadData) that
stores and transmits data to each thread. 
The structure contains:
- Original Image
- Scaled Image
- Contour map
- delta(x), delta(y)
- The grid's dimensions
- Sample grid
- Thread's id and total number of threads
- The barrier

## 'main'
I allocated memory for creating the map, the
scaled image (including its data) and the grid, as well. I have included
error messages in case the allocation didin't proceed as expected.

I allocated the memory for the thread and also for the 'argument' array,
which is an array of pointers to ThreadData structure that stores
the data to be passed to the threads in the parallel programs.
Subsequnetly, I created the threads (they execute the 'final_function' function, 
that I am going to explain below). After that, I joined the threds, followed
by displaying the result, destroying the used barrier and releasing (free) the
allocated resources from the memory. 

## 'final_funtion'
I considered the next process: I almost followed
the same order of function calls as in the sequential version. I 
started by calling 'init_contour_map', 'rescale_image', 'sample_grid' 
and finally 'march'. Do not forget to mention that *between each 2*
*consecutive functions, I placed a barrier* because I wanted to make sure
that we wouldn't proceed to the next step until all threads had 
completed the current call. 

For making the parallelism, I used the next formula:
*int start = ID * (double)N / P;*
*int end = min((ID + 1) * (double)N / P, N);* 
(https://mobylab.docs.crescdi.pub.ro/docs/parallelAndDistributed/laboratory1/exercises)

However, in order to avoid calculating my start and my
end value everytime, I creted a function called *'start_end_params'*,
that automatically determines the start and the end values, if we know the 
end point, the total numer of threads and the current thread's ID.

## 'init_contour_map' 
It calculates the start and the end value for the iteration based on
the total contour images, the thread's id and the total number of
threads, each thread being reponsible for loading a portion for the
contour map image.

Inside the parallelized loop, the function constructs the filename for 
the current contour map and reads the image.

## 'rescale_image'
It calculates the start and the end value for the iteration
determined by (*argument->scaled_image)->x, the thread's id and the total number of threads. 

It performs a bicubic interpolation to rescale the original image
to a new size (scaled_image).

## 'sample_grid'
In the 'sample_grid' function, i set the values of the 'grid' variable
depending on wether a certain condition was met and I parallelized all
my 3 loops.


## 'update_image'
I haven't modified the implementation on this function!

## 'march'
In the 'march' function, in order to parallelize the code, I proceeed as
follows: Instead of using 2 loops, I transformed them into a single 
'for' loop with 's' ranging from 0 to 'p * q'. I applied the 
parallelization on this form. However, I still needed the initial 'i' 
and 'j' values for calculating 'k' so using 'q' and the current index I
was able to determine my needed values. 

'K' was used to update the current scaled image, recoloring the pixels
in red, green or blue in a way that fits our algorithm.





