#include <stdio.h>
#include <stdlib.h>

#define HEADER_SIZE 44

/*Code written by Kanupreet Arora.*/
int main(int argc, char *argv[]){
    // this is the name of the input and output file as mentioned in the handout
    char *sourcewave_name, *destwav_name; 
    // storing in this specific wav files (input and output)
    FILE *sourcewav, *destwav;
    // to sample sound
    short left, right, combined; 
    // header 
    int header[HEADER_SIZE];
    // to state any errors
    int errors;

    if (argc != 3){
        fprintf(stderr, "Usage: %s inputfile outputfile\n", argv[0]);
        return 1;
    }
    sourcewave_name = argv[1]; // existing wavefile to read
    destwav_name = argv[2]; // new stereo output file. This will store the removed vocals

    // opening files
    sourcewav = fopen(sourcewave_name, "rb");
    if (sourcewav == NULL) {
        fprintf(stderr, "Error: could not open input file\n");
        return 1;
    }
    destwav = fopen(destwav_name, "wb");
    if (destwav == NULL) {
        fprintf(stderr, "Error: could not open stereo output file\n");
        return 1;
    }

    // copy 44 bytes 
    errors = fread(header, HEADER_SIZE, 1, sourcewav);
    if (errors != 1) {
        fprintf(stderr, "Error: could not copy\n");
        return 1;
    }
    errors = fwrite(header, HEADER_SIZE, 1, destwav);
    if (errors != 1) {
        fprintf(stderr, "Error: could not write a full audio header\n");
        return 1;
    }
    // removing the vocals from left, right then combined following the algorithm
    while (fread(&left, sizeof(short), 1, sourcewav) == 1 && fread(&right, sizeof(short), 1, sourcewav) == 1) {
        combined = (left - right) / 2;

        //printing two copies of combined file 
        if (fwrite(&combined, sizeof(short), 1, destwav) != 1) {
            fprintf(stderr, "Error: could not write to stereo output WAV file\n");
            fclose(sourcewav);
            fclose(destwav);
            return 1;    
        }
        if (fwrite(&combined, sizeof(short), 1, destwav) != 1) {
            fprintf(stderr, "Error: could not write to stereo output WAV file\n");
            fclose(sourcewav);
            fclose(destwav);
            return 1;
        }
    }
    
    if (fclose(sourcewav) != 0) {
        fprintf(stderr, "Error: fclose failed on source file\n");
        return 1;
    }
    if (fclose(destwav) != 0) {
        fprintf(stderr, "Error: fclose failed on stereo output file\n");
        return 1;
    }

    return 0;

    

}