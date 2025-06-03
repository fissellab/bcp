#ifndef COORDS_H
#define COORDS_H

typedef struct SkyCoord {
  double lon;
  double lat;
  char type[6];

}SkyCoord;

void AzEl_from_RaDec(SkyCoord *RaDec, SkyCoord *AzEl);

extern double tel_lat;
extern double tel_lon;


#endif





