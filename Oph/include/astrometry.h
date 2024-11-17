#ifndef ASTROMETRY_H
#define ASTROMETRY_H

int initAstrometry(FILE* log);
void closeAstrometry();
int lostInSpace(FILE* logfile, double * star_x, double * star_y, double * star_mags, 
                unsigned num_blobs, struct tm * tm_info, char * datafile);

/* Astrometry parameters and solutions struct */
#pragma pack(push, 1)
struct astrometry {
    double timelimit;
    double rawtime;
    // for solving altaz
    double logodds;
    double latitude;
    double longitude;
    double hm;
    // astrometry solution
    double ra;
    double dec;
    double fr;
    double ps;
    double ir;
    double alt;
    double az;
};
#pragma pack(pop)

extern struct astrometry all_astro_params;
extern struct conf_params config;

#endif 
