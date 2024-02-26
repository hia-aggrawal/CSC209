#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define HEADER_SIZE 22
#define DEFAULT_DELAY 8000
#define DEFAULT_VSCALE 4
short header[HEADER_SIZE];

/* addecho applies an echo effect to a WAV 
audio file, with adjustable delay and volume scale. 
Code written by: Hia Aggrawal*/ 
/*Kanupreet Arora could not push to the Markus repository
due to techincal difficulties, so Hia Aggrawal pushed her 3 files 
(video, writing, and remvocals.c) to Markus*/
int main(int argc, char *argv[]) {
    int delay = DEFAULT_DELAY, volume_scale = DEFAULT_VSCALE;
    char *source_name, *dest_name;
    FILE *sourcewav, *destwav;
    short sample;
    int error;

    /* referred to man 3 getop, and the following link for the code below
    https://pubs.opengroup.org/onlinepubs/009696799/functions/getopt.html*/
    int c;
    while ((c = getopt(argc, argv, "v:d:")) != -1) {
        switch(c) {
            case 'd':
                delay = strtol(optarg, NULL, 10);
                if (delay <= 0) {
                    fprintf(stderr, "Invalid delay value. Must be greater than 0.\n");
                    return 1;
                }
                break;
            case 'v':
                volume_scale = strtol(optarg, NULL, 10);
                if (volume_scale <= 0) {
                    fprintf(stderr, "Invalid volume scale value. Must be greater than 0.\n");
                    return 1;
                }
                break;
            default:
                fprintf(stderr, "Invalid usage.\n Usage: [-d delay] [-v volume_scale] <sourcewav> <destwav>\n");
                return 1;
        }
    }
    if (argv[optind] == NULL || argv[optind+1] == NULL) {
        fprintf(stderr, "Missing arguments. Please refer to the usage below:\nUsage: [-d delay] [-v volume_scale] <sourcewav> <destwav>\n");
        return 1;
    }

    source_name = argv[optind];
    dest_name = argv[optind+1];
    short *echo_buff = (short *) malloc(delay * sizeof(short));
  
    sourcewav = fopen(source_name, "rb");
    if (sourcewav == NULL) {
        fprintf(stderr, "Error: could not open source file\n");
        return 1;
    }

    destwav = fopen(dest_name, "wb");
    if (destwav == NULL) {
        fprintf(stderr, "Error: could not open destination file\n");
        return 1;
    }

    // read the header from the input file
    fread(&header, HEADER_SIZE*sizeof(short), 1, sourcewav);

    size_t offset_file = 2;
    size_t offset_data = 20;

    unsigned int *sizeptr_file = (unsigned int *)(header + offset_file);
    *sizeptr_file += (delay * 2);

    unsigned int *sizeptr_data = (unsigned int *)(header + offset_data);
    *sizeptr_data += (delay * 2);

    error = fwrite(&header, HEADER_SIZE*sizeof(short), 1, destwav); 
    if (error != 1) {
        fprintf(stderr, "Error: could not write a full audio header\n");
        return 1;
    }
    

    // read all the samples before the first delay
    int i = 0;
    int j = 0;
    while (i < delay && fread(&sample, sizeof(short), 1, sourcewav) >= 1) {
        echo_buff[i] = sample;
        error = fwrite(&sample, sizeof(short), 1, destwav);
        if (error != 1) {
            fprintf(stderr, "Error: could not write a sample.\n");
            return 1;
        }
        i++;
    }

    // read the rest of the samples, while mixing in with the echo
    short scaled;
    while (fread(&sample, sizeof(short), 1, sourcewav) == 1) {
        if (j == delay) {
            j = 0;
        }
        scaled = ((echo_buff[j]/volume_scale) + sample);
        echo_buff[j] = sample;
        j++;
        error = fwrite(&scaled, sizeof(short), 1, destwav);
        if (error != 1) {
            fprintf(stderr, "Error: could not write a sample.\n");
            return 1;
        }
    }

    /* if delay sample is greater than the original number of samples
    add the number of zero samples required*/
    short zero_sample = 0;
    for (int t = i; t < delay; t++) {
        error = fwrite(&zero_sample, sizeof(short), 1, destwav);
        if (error != 1) {
            fprintf(stderr, "Error: could not write a sample.\n");
            return 1;
        }
    }

    // now, mix the delays left (if any) with the zero samples
    if (i < delay) {
        delay = i;
    }
    for (int s=j; s < delay; s++) {
        scaled = echo_buff[s] / volume_scale;
        error = fwrite(&scaled, sizeof(short), 1, destwav);
        if (error != 1) {
            fprintf(stderr, "Error: could not write a sample.\n");
            return 1;
        }
    }

    for (int k=0; k < j; k++) {
        scaled = echo_buff[k] / volume_scale;
        error = fwrite(&scaled, sizeof(short), 1, destwav);
        if (error != 1) {
            fprintf(stderr, "Error: could not write a sample.\n");
            return 1;
        }
    }

    // close input and output files
    error = fclose(sourcewav);
    if (error != 0) {
        fprintf(stderr, "Error: fclose failed on input file\n");
        return 1;
    }

    error = fclose(destwav);
    if (error != 0) {
        fprintf(stderr, "Error: fclose failed on output file\n");
        return 1;
    }

    return 0;
}