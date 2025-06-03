#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include "coords.h"

double tel_lat = 44.298302778;
double tel_lon = -76.432297222;

void get_UTC(char *buf,int size){
  struct timeval current_time;
  struct tm ts;
  double seconds;
  char time_buf[80];
  // Get current time
  gettimeofday(&current_time, NULL); 
  // Format time, "yyyy-mm-dd hh:mm:ss"
  ts = *gmtime(&current_time.tv_sec);
  seconds = ts.tm_sec+ current_time.tv_usec/1e6;
  strftime(time_buf,size, "%Y-%m-%d %H:%M", &ts);
  snprintf(buf,size, "%s:%lf", time_buf, seconds);
}


double get_JD(){
  int year,month,day,hour,minute;
  double second;
  char utc_str[80];
  int jdn;
  double jd;
  int m,a,b,c;
  get_UTC(utc_str,80);
  sscanf(utc_str,"%d-%d-%d %d:%d:%lf",&year,&month,&day,&hour,&minute,&second);
  m = (month-14)/12;
  a = 1461*(year+4800+m)/4;
  b = 367*(month-2-12*m)/12;
  c = 3*(year+4900+m)/100;
  jdn = a + b - c/4  + day - 32075;
  jd = jdn + hour/24.0 + minute/1440.0+ second/86400.0-0.5;
  return jd;
 }


double get_GMST(){
  double jd;
  double T_u;
  double D_u;
  double theta;
  double gmst_raw;
  double gmst;
  jd = get_JD();
  D_u = jd - 2451545;
  T_u = D_u/ 36525.0;
  theta = 0.7790572732640 + 0.00273781191135448*D_u + fmod(jd,1.0);
  gmst_raw = 86400*theta + (0.014506 + 4612.156534*T_u + 1.3915817*pow(T_u,2)-0.00000044*pow(T_u,3)-0.000029956*pow(T_u,4)-0.0000000368*pow(T_u,5))/15;
  gmst = fmod(gmst_raw,86400)/3600;
  return gmst;
}

double hour_angle(double lon, double ra){
  double h;
  double lst;
  lst = get_GMST() + lon/15;
  if (lst<0){
    lst+=24;
  }
  h = lst - ra/15;
  if (h<0){
    h+= 24;
  }
  return h;
  
}

double Az_from_RaDec(double ra, double dec, double lat, double lon){
  double h;
  double x;
  double y;
  double az;
  
  h = hour_angle(lon,ra)*15;
  x = (-1)*sin(lat*M_PI/180)*cos(dec*M_PI/180)*cos(h*M_PI/180)+cos(lat*M_PI/180)*sin(dec*M_PI/180);
  y = cos(dec*M_PI/180)*sin(h*M_PI/180);
  az = -atan2(y,x)*180/M_PI;
  
  if(az<0){
    az+=360;
  }
  
  return az;
  
}
double El_from_RaDec(double ra, double dec, double lat, double lon){
  double h;
  double el;
  
  h = hour_angle(lon,ra)*15;
  el = asin(sin(lat*M_PI/180)*sin(dec*M_PI/180)+cos(lat*M_PI/180)*cos(dec*M_PI/180)*cos(h*M_PI/180))*180/M_PI;
  
  return el;
  
}
void AzEl_from_RaDec(SkyCoord *RaDec, SkyCoord *AzEl){
    AzEl->lon = Az_from_RaDec(RaDec->lon,RaDec->lat,tel_lat,tel_lon);
    AzEl->lat = El_from_RaDec(RaDec->lon,RaDec->lat,tel_lat,tel_lon);
    strcpy(AzEl->type,"AzEl");
}



