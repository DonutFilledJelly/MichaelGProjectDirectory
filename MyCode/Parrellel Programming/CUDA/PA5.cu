#include <cuda.h>
#include <stdio.h>
#include <stdlib.h>
#define N 1024
#define T 1024
//simple variance calculation.

//This esssentially adds all values together by first
//adding 1 and 2 to 1, 3 and 4 to 3,
//then after the first loop, adding 1 and 3 to 1,
//meaning by the end, 1 is the sum of all the array
__global__ void sum(float *a, int count){
    
    int scope;//scope is for checking to see what level of addition we are on
    int blocknum = blockDim.x * blockIdx.x;//find block id
    int threadnum = threadIdx.x;
    for(scope = 1; scope <= T; scope*=2){//Each level of addition
        int threadmax = T/scope;
        if(threadnum < threadmax){
            int first = blocknum*2 + threadnum*scope*2;
            int second = first + scope;
            if(first < count && second < count){
                a[first] += a[second];//adding
            }
        }
        __syncthreads();
        
    }
}

__global__ void sumagain(float *a, int count){
    
    int scope;//scope is for checking to see what level of addition we are on
    int blocknum = blockDim.x * blockIdx.x;//find block id
    int threadnum = threadIdx.x;
    for(scope = 1; scope <= T; scope*=2){//Each level of addition
        int threadmax = T/scope;
        if(threadnum < threadmax){
            int first = blocknum*2 + threadnum*scope*2;
            int second = first + scope;
            if(first < count && second < count){
                a[first] += a[second];//adding
            }
        }
        __syncthreads();
        
    }
}
//adds the last few together bettween blocks.
__global__ void finishUp(float *a, int count){
    for (int i = T*2; i < count; i += T*2)//has to be T*2
    {
        a[0] += a[i];
    }
}

//Varaince equation (Excluding division)
__global__ void variancething(float *a, int count, float average){
    int where = blockDim.x * blockIdx.x + threadIdx.x;
    if(where < count){// avoid segfault
        a[where] = pow(a[where]-average, 2);
    }
    __syncthreads();
}



int main(){
    float *a;//float array for host
	float *d_a;//float array for device
	int count = 0;//amount in the array
	scanf("%d", &count);
	int size = count * sizeof(float);//size of arrays
    
    
    a = (float *)malloc(size); 
	cudaMalloc(&d_a, size);
	
    for(int i = 0; i < count; i++){//reading in array
        scanf("%f", &a[i]);
    }
    
    
    cudaMemcpy(d_a, a, size, cudaMemcpyHostToDevice);//sending array to gpu
    sum<<<N, T>>>(d_a, count);//add all into d_a[0]
    finishUp<<<1,1>>>(d_a, count);//add between blocks
    
    float sum; 
    cudaMemcpy(&sum, d_a, sizeof(float), cudaMemcpyDeviceToHost);
    float average = sum/count;//average is the total sum over the amount of numbers
    printf("The average is %f\n", average);
    
    cudaMemcpy(d_a, a, size, cudaMemcpyHostToDevice);

    variancething<<<N, T>>>(d_a, count, average);//finding variance in next three lines
    sumagain<<<N, T>>>(d_a, count);//need to create a new sum, not sure why but syntax wont let me. Exactly same as last sum
    finishUp<<<1, 1>>>(d_a, count);
    
    float variance = 0;
    cudaMemcpy(&variance, d_a, sizeof(float), cudaMemcpyDeviceToHost);
    variance = variance/(count-1);//finding last bit of variance (division)
    
    
    printf("Variance is %f\n", variance);
    
	
    free(a); 
    cudaFree(d_a);

    return 0;
}
