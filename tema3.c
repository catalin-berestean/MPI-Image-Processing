#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct{				
    unsigned char r, g, b;
} rgb;

rgb **img, **tmp;
int width, height, type; //photo attributes
float kernel[3][3];		//used for filters
int inceput, sfarsit;
int rank, N; //used for MPI


//read image
void readLoc(char *loc){

    FILE *file = fopen(loc,"r"); //read image

    char buf[500];

    fgets(buf, 500, file);  
    type = (buf[1] == '5');	//type of image,assuming it's P5first

    fgets(buf, 500, file); //skip comments      
    fgets(buf, 500, file); //read w&h 

    sscanf(buf, "%d %d", &width, &height);	//store w&h
    fgets(buf, 500, file);
    int garbage;	
    sscanf(buf, "%d", &garbage);	//skip maxval
    
    img = (rgb **)malloc(height * sizeof(rgb *));	//alloc memory for RGB struct
    for(int i = 0; i < height; i++)
        img[i] = (rgb *)malloc(width * sizeof(rgb));
       
    tmp = (rgb **)malloc(height * sizeof(rgb *));
    for(int i = 0; i < height; i++)
        tmp[i] = (rgb *)malloc(width * sizeof(rgb));

    for(int i = 0; i < height; i++){
        for(int j = 0 ; j < width; j++){		//read bw or color data
            fscanf(file,"%c", &img[i][j].r);
            if(!type){							
                fscanf(file,"%c", &img[i][j].g);
                fscanf(file,"%c", &img[i][j].b);
            }
        }
    }
}

//write in output image
void writeLoc(char *loc){

    FILE *file = fopen(loc,"w");	//open file to write image
    
    if(type)
    	//black&white
        fprintf(file, "%s%d %d\n%d\n", "P5\n", width, height, 255);
    else
    	//color
        fprintf(file, "%s%d %d\n%d\n", "P6\n", width, height, 255);

    for(int i = 0; i < height; i++){
        for(int j = 0 ; j < width; j++){	//write  black&white or color data	
            fprintf(file,"%c",img[i][j].r);
            if(!type){
                fprintf(file,"%c",img[i][j].g);
                fprintf(file,"%c",img[i][j].b);
            }
        }
    }
    fclose(file);
}


//aux function for filter operation 
float sumup(int Y, int X, char channel){
    float ans = 0.0f;
    
    for(int y = 0; y < 3; y++){  
        float lineAns = 0.0f;
        for(int x = 0; x < 3; x++){ 
            int newY = y + Y;
            int newX = x + X;
            //shift kernel
            newX--;
            newY--;
            if(newY < height && newX < width && newX >= 0 && newY >= 0){    //do not cross boundaries 
                float pixel = 0.0f;
                if(channel == 'r')
                    pixel =  (float)img[newY][newX].r;
                if(channel == 'g')
                    pixel =  (float)img[newY][newX].g;
                if(channel == 'b')
                    pixel =  (float)img[newY][newX].b;
                ans += kernel[y][x]*pixel;
            }
        }
        ans += lineAns;
    }
    if(ans > 255.0f)	//resolve overflow/underflow
        return 255.0f;

    if(ans < 0.0f)
        return 0.0f;
    return ans;
}

//apply filter on pixel matrix
void applyKernel(){
    int inceput1 = inceput;
    int sfarsit1 = sfarsit;
    inceput1 -= 30;
    sfarsit1 += 30;
    inceput1 = ((inceput1 < 0) ? 0 : inceput1);
    sfarsit1 = (sfarsit1 > height ? height : sfarsit1);

   	//calculate for each channel (r,g,b)
    for(int i = inceput1; i < sfarsit1; i++)
        for(int j = 0 ; j < width; j++)
                if(type){
                    tmp[i][j].r =  sumup(i, j, 'r');
                }else{
                    tmp[i][j].r =  sumup(i, j, 'r');
                    tmp[i][j].g =  sumup(i, j, 'g');
                    tmp[i][j].b =  sumup(i, j, 'b');
                }             

    for(int i = 0; i < height; i++)
        for(int j = 0 ; j < width; j++)
                img[i][j] = tmp[i][j];
}

int main (int argc, char *argv[]){

    int padding = argc + 10;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD,&N);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);    

    readLoc(argv[1]);

    inceput = rank * ceil(height/N);	
    if(rank == N - 1)
        sfarsit = height;
    else
		sfarsit = (rank + 1) * ceil(height/N);
	//applying FILTERS
    for(int x = 3; x < argc; x++){
  		

  		//identify filter and apply with applyKernel
        if(!strcmp("smooth",argv[x])){
            float smooth[3][3] = { {1.0f/9.0f, 1.0f/9.0f, 1.0f/9.0f}, {1.0f/9.0f, 1.0f/9.0f, 1.0f/9.0f}, {1.0f/9.0f, 1.0f/9.0f, 1.0f/9.0f} };
            for(int i = 0; i < 3; i++)
                memcpy(&kernel[i], &smooth[i], sizeof(kernel[0]));
        }
        if(!strcmp("blur",argv[x])){
            float blur[3][3] = { {1.0f/16.0f, 2.0f/16.0f, 1.0f/16.0f}, {2.0f/16.0f, 4.0f/16.0f, 2.0f/16.0f}, {1.0f/16.0f, 2.0f/16.0f, 1.0f/16.0f} };
            for(int i = 0; i < 3; i++) 
                memcpy(&kernel[i], &blur[i], sizeof(kernel[0]));
        }
        
        if(!strcmp("sharpen",argv[x])){
            float sharpen[3][3] = { {0.0f, -2.0f/3.0f, 0.0f}, {-2.0f/3.0f, 11.0f/3.0f, -2.0f/3.0f}, {0.0f, -2.0f/3.0f, 0.0f} };
            for(int i = 0; i < 3; i++) 
                memcpy(&kernel[i], &sharpen[i], sizeof(kernel[0]));
        }

        if(!strcmp("mean",argv[x])){
            float mean[3][3] = { {-1.0f, -1.0f, -1.0f}, {-1.0f, 9.0f, -1.0f}, {-1.0f, -1.0f, -1.0f} };
            for(int i = 0; i < 3; i++)
                memcpy(&kernel[i], &mean[i], sizeof(kernel[0]));
        }

        if(!strcmp("emboss",argv[x])){
            float emboss[3][3] = { {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} };
            for(int i = 0; i < 3; i++) 
                memcpy(&kernel[i], &emboss[i], sizeof(kernel[0]));
        }

        applyKernel();
    }
        
    if(rank){
        for(int i = inceput; i < sfarsit; i++)
            MPI_Send(img[i], width*3, MPI_UNSIGNED_CHAR, 0, i, MPI_COMM_WORLD);
    }else{
        for(int j = 1; j < N; j++){
            inceput = j * ceil(height/N);
            if(rank == N - 1)
                sfarsit = height;
            else
                sfarsit = (j + 1) * ceil(height/N);
            for(int i = inceput; i < sfarsit; i++)
                MPI_Recv(img[i], width*3, MPI_UNSIGNED_CHAR, j, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        //write image after filtering 
        writeLoc(argv[2]);
    }
    MPI_Finalize();
    return 0;
}