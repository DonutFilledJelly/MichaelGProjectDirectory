#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <string>
using namespace std;


int main(int argc, char *argv[]){
        int block_size = 16;//size of each block
        int num_of_blocks = 128;//number of total blocks
        int associative = 1;
        float hit_time = 1;//time it takes for a hit
        float miss_time = 1;//time it takes for a miss

        switch (argc){//take in arguments if available
            case 6:
                associative = atoi (argv[5]);
            case 5:
                miss_time = atoi (argv[4]);
            case 4:
                hit_time = atoi (argv[3]);
            case 3:
                num_of_blocks = atoi (argv[2]);
            case 2:
                block_size = atoi(argv[1]);
        }
        int block_array[num_of_blocks][block_size];//create cache
        if(associative != 1){//Direct map only
            cout << "Please make sure associative is 1 (The 5th variable on command line)" << endl;
            return 0;
        }
        fstream newfile;
        newfile.open("addresses.txt", fstream::in);//open address file
        string line;
        float hit, total;
        total = 0;
        hit = 0;
        while(getline(newfile, line)){//each line/address
            total++;
            line.erase(0, 2);
            int value;
            value = stoi(line, 0, 16);
            int block_add, block_num;
            block_add = value/block_size;//find address in block
            int block_rem = value % block_size;
            block_num = block_add % num_of_blocks;//find which block
            int current_address = block_array[block_num][block_rem];
            if(current_address == value){//check to see if block already has address
                hit++;
            }
            else{
                block_array[block_num][block_rem] = value;//if not switch it out
            }
        }
        
        float hitmisspercent = (hit/total) * 100;//cal hitmisspercent
        float miss = total - hit;
        float missrate = miss/total;//miss rate
        cout << "Hit rate: " << hit << "/" << total << endl;
        cout << "Miss rate: " << miss << "/" << total << endl;
        cout << "Hit miss rate: " << hitmisspercent << "%" << endl;
        float AMAT = hit_time + missrate * miss_time;
        cout << "AMAT: " << AMAT << " ns" << endl;

    return 0;
}