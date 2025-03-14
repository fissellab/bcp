#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <ueye.h>
#include <stdbool.h>
#include <pthread.h>  

#include "camera.h"
#include "astrometry.h"
#include "bvexcam.h"
#include "lens_adapter.h"
#include "matrix.h"
#include "file_io_Oph.h"

void merge(double A[], int p, int q, int r, double X[],double Y[]);
void part(double A[], int p, int r, double X[], double Y[]);

// 1-254 are possible IDs. Command-line argument from user with ./commands
HIDS camera_handle;          
// breaking convention of using underscores for struct names because this is how
// iDS does it in their documentation
IMAGE_FILE_PARAMS ImageFileParams;
// same with sensorInfo struct
SENSORINFO sensorInfo;

// global variables
int send_data = 0;
int taking_image = 0;
int default_focus_photos = 3;
int buffer_num, mem_id;
char * memory, * waiting_mem, * mem_starting_ptr;
unsigned char * mask;
// for printing camera errors
const char * cam_error;
// 'curr' = current, 'pc' = pixel clock, 'fps' = frames per sec, 
// 'ag' = auto gain, 'bl' = black level
double curr_exposure, actual_fps, curr_ag, curr_shutter, auto_fr;
int curr_pc, curr_color_mode, curr_ext_trig, curr_trig_delay, curr_master_gain;
int curr_red_gain, curr_green_gain, curr_blue_gain, curr_gamma, curr_gain_boost;
unsigned int curr_timeout;
int bl_offset, bl_mode;
int prev_dynamic_hp;

/* Blob parameters global structure (defined in camera.h) */
struct blob_params all_blob_params = {
    .spike_limit = 3,             
    .dynamic_hot_pixels = 1,       
    .r_smooth = 2,                 
    .high_pass_filter = 0,         
    .r_high_pass_filter = 10,     
    .centroid_search_border = 1,  
    .filter_return_image = 0,      
    .n_sigma = 2.0,               
    .unique_star_spacing = 15,    
    .make_static_hp_mask = 0,     
    .use_static_hp_mask = 1,       
};

/* Helper function to determine if a year is a leap year (2020 is a leap year).
** Input: The year.
** Output: A flag indicating the input year is a leap year or not.
**/
int isLeapYear(int year) {
    if (year % 4 != 0) {
        return false;
    } else if (year % 400 == 0) {
        return true;
    } else if (year % 100 == 0) {
        return false;
    } else {
        return true;
    }
}

/* Helper function to test blob-finding parameters.
** Input: None.
** Output: None (void). Prints current blob-finding parameters to the terminal.
**/
void verifyBlobParams() {
    printf("\n+---------------------------------------------------------+\n");
    printf("|\t\tBlob-finding parameters\t\t\t  |\n");
    printf("|---------------------------------------------------------|\n");
    printf("|\tall_blob_params.spike_limit is: %d\t\t  |\n", 
           all_blob_params.spike_limit);
    printf("|\tall_blob_params.dynamic_hot_pixels is: %d\t  |\n", 
           all_blob_params.dynamic_hot_pixels);
    printf("|\tall_blob_params.centroid_search_border is: %d\t  |\n", 
           all_blob_params.centroid_search_border);
    printf("|\tall_blob_params.high_pass_filter is: %d\t\t  |\n", 
           all_blob_params.high_pass_filter);
    printf("|\tall_blob_params.r_smooth is: %d\t\t\t  |\n", 
           all_blob_params.r_smooth);
    printf("|\tall_blob_params.filter_return_image is: %d\t  |\n", 
           all_blob_params.filter_return_image);
    printf("|\tall_blob_params.r_high_pass_filter is: %d\t  |\n", 
           all_blob_params.r_high_pass_filter);
    printf("|\tall_blob_params.n_sigma is: %.2f\t\t  |\n", 
           all_blob_params.n_sigma);
    printf("|\tall_blob_params.unique_star_spacing is: %d\t  |\n", 
           all_blob_params.unique_star_spacing);
    printf("|\tall_blob_params.make_static_hp_mask is: %i\t  |\n", 
           all_blob_params.make_static_hp_mask);
    printf("|\tall_blob_params.use_static_hp_mask is: %i\t  |\n", 
           all_blob_params.use_static_hp_mask);
    printf("+---------------------------------------------------------+\n\n");
}

/* Helper function to print camera errors.
** Input: None.
** Output: The error from the camera.
*/
const char * printCameraError() {
    char * last_error_str;
    int last_err = 0;

    is_GetError(camera_handle, &last_err, &last_error_str);
    return last_error_str;
}

/* Function to initialize the camera and its various parameters.
** Input: None.
** Output: A flag indicating successful camera initialization or not.
*/
int initCamera(FILE* log) {
    double min_exposure, max_exposure;
    unsigned int enable = 1;   

    // load the camera parameters
    if (loadCamera(log) < 0) {
        return -1;
    }

    // enable long exposure 
    if (is_Exposure(camera_handle, IS_EXPOSURE_CMD_SET_LONG_EXPOSURE_ENABLE, 
                   (void *) &enable, sizeof(unsigned int)) != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][initCamera] Unable to enable long exposure: %s.\n", time(NULL), cam_error);
        return -1; 
    }

    // set exposure time based on struct field
    if (is_Exposure(camera_handle, IS_EXPOSURE_CMD_SET_EXPOSURE, 
                   (void *) &all_camera_params.exposure_time, sizeof(double)) 
                   != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][initCamera] Unable to set default exposure: %s.\n", time(NULL), cam_error);
        return -1;
    }

    // get current exposure, max possible exposure, and min exposure
    if (is_Exposure(camera_handle, IS_EXPOSURE_CMD_GET_EXPOSURE_RANGE_MIN, 
                    &min_exposure, sizeof(double)) != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][initCamera] Unable to get minimum possible exposure: %s.\n", time(NULL), cam_error);
        return -1;
    }

    if (is_Exposure(camera_handle, IS_EXPOSURE_CMD_GET_EXPOSURE, &curr_exposure,
                    sizeof(double)) != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][initCamera] Unable to get current exposure value: %s.\n", time(NULL), cam_error);
        return -1;
    }

    if (is_Exposure(camera_handle, IS_EXPOSURE_CMD_GET_EXPOSURE_RANGE_MAX, 
                    &max_exposure, sizeof(double)) != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][initCamera] Unable to get maximum exposure value: %s.\n", time(NULL), cam_error);
        return -1;
    } else if (verbose) {
        printf("|\tCurrent exposure time: %f msec\t\t  |\n|\tMin possible "
               "exposure: %f msec\t\t  |\n|\tMax possible exposure: %f "
               "msec\t  |\n", curr_exposure, min_exposure, max_exposure);
        printf("+---------------------------------------------------------+\n");
    }

    // initialize astrometry
    if (initAstrometry(log) < 0) {
        return -1;
    }

    // set how images are saved
	setSaveImage();

    return 1;
}

/* Cleaning up function for ctrl+c exception.
** Input: None.
** Output: None (void). Changes the state of the camera to shutting down.
**/
void clean() {
    if (verbose) {
        printf("\n> Entering shutting down state...\n");
    }
    // camera is in shutting down state (stop solving)
    shutting_down = 1;
}

/* Function to close the camera when shutting down.
** Input: None.
** Output: None (void).
*/
void closeCamera() {
    if (verbose) {
        printf("> Closing camera with handle %d...\n", camera_handle);
    }
    
    // don't close a camera that doesn't exist yet!
    if ((mem_starting_ptr != NULL) && (camera_handle <= 254)) { 
        is_FreeImageMem(camera_handle, mem_starting_ptr, mem_id);
        is_ExitCamera(camera_handle);
    }
}

/* Function to set starting values for certain camera atributes.
** Input: None.
** Output: A flag indicating successful setting of the camera parameters.
*/
int setCameraParams(FILE* log) {
    double ag = 0.0, auto_shutter = 0.0, afr = 0.0;        
    int blo, blm;                  

    // set the trigger delay to 0 (microseconds) = deactivates trigger delay
    if (is_SetTriggerDelay(camera_handle, 0) != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][setCameraParmas] Error setting the trigger to 0 microseconds: %s.\n", time(NULL), cam_error);
        return -1;
    }
    // check the trigger delay is 0
    curr_trig_delay = is_SetTriggerDelay(camera_handle, IS_GET_TRIGGER_DELAY); 
    if (verbose) {
        printf("\n+---------------------------------------------------------+\n");
        printf("|\t\tCamera Specifications\t\t\t  |\n");
        printf("|---------------------------------------------------------|\n");
        printf("|\tTrigger delay (should be 0): %i\t\t\t  |\n", curr_trig_delay);
    }

    // set camera integrated amplifier 
    if (is_SetHardwareGain(camera_handle, 0, IS_IGNORE_PARAMETER, 
                           IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) 
                           != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][setCameraParmas] Error setting gain: %s.\n", time(NULL), cam_error);
        return -1;
    }
    // check color gains
    curr_master_gain = is_SetHardwareGain(camera_handle, IS_GET_MASTER_GAIN, 
                                          IS_IGNORE_PARAMETER, 
                                          IS_IGNORE_PARAMETER, 
                                          IS_IGNORE_PARAMETER);
    curr_red_gain = is_SetHardwareGain(camera_handle, IS_IGNORE_PARAMETER, 
                                       IS_GET_RED_GAIN, IS_IGNORE_PARAMETER, 
                                       IS_IGNORE_PARAMETER);
    curr_green_gain = is_SetHardwareGain(camera_handle, IS_IGNORE_PARAMETER, 
                                         IS_IGNORE_PARAMETER, IS_GET_GREEN_GAIN,
                                         IS_IGNORE_PARAMETER);
    curr_blue_gain = is_SetHardwareGain(camera_handle, IS_IGNORE_PARAMETER, 
                                        IS_IGNORE_PARAMETER, 
                                        IS_IGNORE_PARAMETER, IS_GET_BLUE_GAIN);
    if (verbose) {
        printf("|\tGain: master, red, green, blue = %i, %i, %i, %i |\n", 
               curr_master_gain, curr_red_gain, curr_green_gain, curr_blue_gain);
    }

    // disable camera's auto gain (0 = disabled)
    if (is_SetAutoParameter(camera_handle, IS_SET_ENABLE_AUTO_GAIN, &ag, NULL) 
        != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][setCameraParams] Error disabling auto gain: %s.\n", time(NULL), cam_error);
        return -1;
    }
    // check auto gain is off
    is_SetAutoParameter(camera_handle, IS_GET_ENABLE_AUTO_GAIN, &curr_ag, NULL);
    if (verbose) {
        printf("|\tAuto gain (should be disabled): %.1f\t\t  |\n", curr_ag);
    }

    // set camera's gamma value to off 
    if (is_SetHardwareGamma(camera_handle, IS_SET_HW_GAMMA_OFF) != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][setCameraParams] Error disabling hardware gamma correction: %s.\n", time(NULL), cam_error);
        return -1;
    }
    // check hardware gamma is off
    curr_gamma = is_SetHardwareGamma(camera_handle, IS_GET_HW_GAMMA);
    if (verbose) {
        printf("|\tHardware gamma (should be off): %i\t\t  |\n", curr_gamma);
    }

    // disable auto shutter
    if (is_SetAutoParameter(camera_handle, IS_SET_ENABLE_AUTO_SHUTTER, 
                            &auto_shutter, NULL) != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][setCameraParams] Error disabling auto shutter: %s.\n", time(NULL), cam_error);
        return -1;
    }
    // check auto shutter is off
    is_SetAutoParameter(camera_handle, IS_GET_ENABLE_AUTO_SHUTTER, 
                        &curr_shutter, NULL);
    if (verbose) {
        printf("|\tAuto shutter (should be off): %.1f\t\t  |\n", curr_shutter);
    }

    // disable auto frame rate
    if (is_SetAutoParameter(camera_handle, IS_SET_ENABLE_AUTO_FRAMERATE, &afr, 
                            NULL) != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][setCameraParams] Error disabling auto frame rate: %s.\n", time(NULL), cam_error);
        return -1;
    }
    // check auto frame rate is off
    is_SetAutoParameter(camera_handle, IS_GET_ENABLE_AUTO_FRAMERATE, &auto_fr, 
                        NULL);
    if (verbose) {
        printf("|\tAuto frame rate (should be off): %.1f\t\t  |\n", auto_fr);
    }

    // disable gain boost to reduce noise
    if (is_SetGainBoost(camera_handle, IS_SET_GAINBOOST_OFF) != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][setCameraParams] Error disabling gain boost: %s.\n", time(NULL), cam_error);
        return -1;
    }
    // check gain boost is off
    curr_gain_boost = is_SetGainBoost(camera_handle, IS_GET_GAINBOOST);
    if (verbose) {
        printf("|\tGain boost (should be disabled): %i\t\t  |\n", 
               curr_gain_boost);	
    }

    // turn off auto black level
    blm = IS_AUTO_BLACKLEVEL_OFF;
    if (is_Blacklevel(camera_handle, IS_BLACKLEVEL_CMD_SET_MODE, (void *) &blm, 
                      sizeof(blm)) != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][setCameraParams] Error turning off auto black level mode: %s.\n", time(NULL), cam_error);
        return -1;
    }
    // check auto black level is off
    bl_mode = is_Blacklevel(camera_handle, IS_BLACKLEVEL_CMD_GET_MODE, 
                            (void *) &bl_mode, sizeof(bl_mode));
    if (verbose) {
        printf("|\tAuto black level (should be off): %i\t\t  |\n", bl_mode);
    }

    // set black level offset to 50
    blo = 50;
    if (is_Blacklevel(camera_handle, IS_BLACKLEVEL_CMD_SET_OFFSET, 
                      (void *) &blo, sizeof(blo)) != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log, "[%ld][camera.c][setCameraParams] Error setting black level offset to 50: %s.\n", time(NULL), cam_error);
        return -1;
    }
    // check black level offset
    is_Blacklevel(camera_handle, IS_BLACKLEVEL_CMD_GET_OFFSET, 
                  (void *) &bl_offset, sizeof(bl_offset));
    if (verbose) {
        printf("|\tBlack level offset (desired is 50): %i\t\t  |\n", bl_offset);
    }

    // This for some reason actually affects the time it takes to set aois only 
    // on the focal plane camera. Don't use if for the trigger timeout. 
    if (is_SetTimeout(camera_handle, IS_TRIGGER_TIMEOUT, 500) != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log, "[%ld][camera.c][setCameraParams] Error setting trigger timeout: %s.\n", time(NULL), cam_error);
        return -1;
    }
    // check time out
    is_GetTimeout(camera_handle, IS_TRIGGER_TIMEOUT, &curr_timeout);
    if (verbose) {
        printf("|\tCurrent trigger timeout: %i\t\t\t  |\n", curr_timeout);
    }

    return 1;
}

/* Function to load the camera at the beginning of the Star Camera session.
** Input: None.
** Output: A flag indicating successful loading of the camera or not.
*/
int loadCamera(FILE* log) {
    int color_depth, pixelclock;
    void * active_mem_loc;
    double fps;

    // initialize camera
	if (is_InitCamera(&camera_handle, NULL) != IS_SUCCESS) {
        write_to_log(log,"camera.c","loadCamera","Error initializing camera in loadCamera().");
        return -1;
	}
  
    // get sensor info
	if (is_GetSensorInfo(camera_handle, &sensorInfo) != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][loadCamera] Error getting camera sensor information: %s.\n", time(NULL), cam_error);
        return -1;
	} 

    // set various other camera parameters
	if (setCameraParams(log) < 0) {
        return -1;
    }

    // set display mode and then get it to verify
	if (is_SetColorMode(camera_handle, IS_CM_SENSOR_RAW8) != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][loadCamera] Error setting color mode: %s.\n", time(NULL), cam_error);
        return -1;
	}
    curr_color_mode = is_SetColorMode(camera_handle, IS_GET_COLOR_MODE);
    if (verbose) {
        printf("|\tCamera model: %s\t\t\t  |\n", sensorInfo.strSensorName);
        printf("|\tSensor ID/type: %i\t\t\t\t  |\n", sensorInfo.SensorID);
        printf("|\tSensor color mode: %i / %i\t\t\t  |\n", 
               sensorInfo.nColorMode, curr_color_mode);
        printf("|\tMaximum image width & height: %i, %i\t  |\n", 
               sensorInfo.nMaxWidth, sensorInfo.nMaxHeight);
        printf("|\tPixel size (micrometers): %.2f\t\t\t  |\n", 
              ((double) sensorInfo.wPixelSize)/100.0);
    }
 	
    // allocate camera memory
	color_depth = 8; 
	if (is_AllocImageMem(camera_handle, sensorInfo.nMaxWidth, 
                         sensorInfo.nMaxHeight, color_depth, &mem_starting_ptr, 
                         &mem_id) != IS_SUCCESS) {
		cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][loadCamera] Error allocating image memory: %s.\n", time(NULL), cam_error);
        return -1;
	}

    // set memory for image (make memory pointer active)
	if (is_SetImageMem(camera_handle, mem_starting_ptr, mem_id) != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][loadCamera] Error setting image memory: %s.\n", time(NULL), cam_error);
        return -1;
    }

    // get image memory
	if (is_GetImageMem(camera_handle, &active_mem_loc) != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][loadCamera] Error getting image memory: %s.\n", time(NULL), cam_error);
        return -1;
    }

    // how clear images can be is affected by pixelclock and fps 
    pixelclock = 30;
	if (is_PixelClock(camera_handle, IS_PIXELCLOCK_CMD_SET, 
                      (void *) &pixelclock, sizeof(pixelclock)) != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][caemra.c][loadCamera] Error setting pixel clock: %s.\n", time(NULL), cam_error);
        return -1;
    }
    // get current pixel clock to check
    is_PixelClock(camera_handle, IS_PIXELCLOCK_CMD_GET, (void *) &curr_pc, 
                  sizeof(curr_pc));
    if (verbose) {
        printf("|\tPixel clock: %i\t\t\t\t\t  |\n", curr_pc);
    }

    // set frame rate
	fps = 10;
	if (is_SetFrameRate(camera_handle, IS_GET_FRAMERATE, (void *) &fps) 
        != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][loadCamera] Error setting frame rate: %s.\n", time(NULL), cam_error);
        return -1;
    }

    // set trigger to software mode (call is_FreezeVideo to take single picture 
    // in single frame mode)
	if (is_SetExternalTrigger(camera_handle, IS_SET_TRIGGER_SOFTWARE) 
        != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][loadCamera] Error setting external trigger mode: %s.\n", time(NULL), cam_error);
        return -1;
	}
    // get the current trigger setting
    curr_ext_trig = is_SetExternalTrigger(camera_handle, IS_GET_EXTERNALTRIGGER);
    if (verbose) {
        printf("|\tCurrent external trigger mode: %i\t\t  |\n", curr_ext_trig);
    }

    return 1;
}

/* Function to establish the parameters for saving images taken by the camera.
** Input: None.
** Output: None (void).
*/
void setSaveImage() {
    ImageFileParams.pwchFileName = L"save1.bmp";
    ImageFileParams.pnImageID = NULL;
    ImageFileParams.ppcImageMem = NULL;
    ImageFileParams.nQuality = 80;
    ImageFileParams.nFileType = IS_IMG_BMP;
}

/* Function to mask hot pixels accordinging to static and dynamic maps.
** Input: The image bytes (ib), the image border indices (i0, j0, i1, j1), rest 
** are 0.
** Output: None (void). Makes the dynamic and static hot pixel masks for the 
** Star Camera image.
*/
void makeMask(char * ib, int i0, int j0, int i1, int j1, int x0, int y0, 
              bool subframe) {
    static int first_time = 1;
    static int * x_p = NULL, * y_p = NULL;
    static int num_p = 0, num_alloc = 0;

    if (first_time) {
        mask = calloc(CAMERA_WIDTH*CAMERA_HEIGHT, 1);
        x_p = calloc(100, sizeof(int));
        y_p = calloc(100, sizeof(int));
        num_alloc = 100;
    }

    // load static hot pixel mask
    if (first_time || all_blob_params.make_static_hp_mask) {
        // read from file with bright pixel coordinates in it
        FILE * f = fopen(join_path(config.bvexcam.configdir,"static_hp_mask.txt"), "r");
        int i, j;
        num_p = 0;

        if (f) {
            if (verbose) {
                printf("\n+---------------------------------------------------------+\n");
                printf("|\t\tLoading static hot pixel map...\t\t  |\n");
                printf("|---------------------------------------------------------|\n");
            }

            char * line = NULL;
            size_t len = 0;
            int read = 0;

            // read every line in the file
            while ((read = getline(&line, &len, f)) != -1) {
                sscanf(line, "%d,%d\n", &i, &j);

                if (num_p >= num_alloc) {
                    num_alloc += 100;
                    x_p = realloc(x_p, sizeof(int) * num_alloc);
                    y_p = realloc(y_p, sizeof(int) * num_alloc);
                }

                x_p[num_p] = i;
                // map y coordinate to image in memory from Kst blob
                y_p[num_p] = CAMERA_HEIGHT - j; 

                if (verbose) {
                    printf("|\tCoordinates read from hp file: [%4i, %4i]\t  |\n", 
                           i, j);
                }

                num_p++;
            }
            
            if (verbose) {
                printf("+---------------------------------------------------------+\n");
            }

            fflush(f);
            fclose(f);
            free(line);
        }

        // do not want to recreate hp mask automatically, so set field to 0
        all_blob_params.make_static_hp_mask = 0;
        // no longer the first time we are running
        first_time = 0;
    }

    int i, j;
    int p0, p1, p2, p3, p4;
    int a, b;
    
    int cutoff = all_blob_params.spike_limit*100.0;

    for (i = i0; i < i1; i++) {
        mask[i + CAMERA_WIDTH*j0] = mask[i + (j1-1)*CAMERA_WIDTH] = 0;
    }

    for (j = j0; j < j1; j++) {
        mask[i0 + j*CAMERA_WIDTH] = mask[i1 - 1 + j*CAMERA_WIDTH] = 0;
    }

    i0++;
    j0++;
    i1--;
    j1--;
  
    if (all_blob_params.dynamic_hot_pixels) {
        int nhp = 0;

        for (j = j0; j < j1; j++) {
            for (i = i0; i < i1; i++) {
                // pixels left/right, above/below
                p0 = 100*ib[i + j*CAMERA_WIDTH]/cutoff;
                p1 = ib[i - 1 + (j)*CAMERA_WIDTH];
                p2 = ib[i + 1 + (j)*CAMERA_WIDTH];
                p3 = ib[i + (j+1)*CAMERA_WIDTH];
                p4 = ib[i + (j-1)*CAMERA_WIDTH];
                a = p1 + p2 + p3 + p4 + 4;
                // pixels on diagonal (upper left/right, lower left/right)
                p1 = ib[i - 1 + (j-1)*CAMERA_WIDTH];
                p2 = ib[i + 1 + (j+1)*CAMERA_WIDTH];
                p3 = ib[i - 1 + (j+1)*CAMERA_WIDTH];
                p4 = ib[i + 1 + (j-1)*CAMERA_WIDTH];
                b = p1 + p2 + p3 + p4 + 4;
                mask[i + j*CAMERA_WIDTH] = ((p0 < a) && (p0 < b));
                if (p0 > a || p0 > b) nhp++;
            }
        }

        if (verbose) {
            printf("\n(*) Number of hot pixels found: %d.\n\n", nhp);
        }
    } else {
        for (j = j0; j < j1; j++) {
            for (i = i0; i < i1; i++) {
                mask[i + j*CAMERA_WIDTH] = 1;
            }
        }
    }

    if (all_blob_params.use_static_hp_mask) {
        if (verbose) {
            printf("+---------------------------------------------------------+\n");
            printf("|\t\tMasking %d pixels...\t\t\t  |\n", num_p);
            printf("|---------------------------------------------------------|\n");
        }

        for (int i = 0; i < num_p; i++) {
            // set all static hot pixels to 0
            if (verbose) {
                printf("|\tCoordinates going into mask: [%4i, %4i]\t  |\n", 
                       x_p[i], CAMERA_HEIGHT - y_p[i]);
            }

            int ind = CAMERA_WIDTH * y_p[i] + x_p[i];
            mask[ind] = 0;
        }

        if (verbose) {
            printf("+---------------------------------------------------------+\n");
        }
    }
}

/* Function to process the image with a filter to reduce noise.
** Input:
** Output:
*/
void boxcarFilterImage(char * ib, int i0, int j0, int i1, int j1, int r_f, 
                       double * filtered_image) {
    static int first_time = 1;
    static char * nc = NULL;
    static uint64_t * ibc1 = NULL;

    if (first_time) {
        nc = calloc(CAMERA_WIDTH*CAMERA_HEIGHT, 1);
        ibc1 = calloc(CAMERA_WIDTH*CAMERA_HEIGHT, sizeof(uint64_t));
        first_time = 0;
    }

    int b = r_f;
    int64_t isx;
    int s, n;
    double ds, dn;
    double last_ds = 0;

    for (int j = j0; j < j1; j++) {
        n = 0;
        isx = 0;
        for (int i = i0; i < i0 + 2*r_f + 1; i++) {
            n += mask[i + j*CAMERA_WIDTH];
            isx += ib[i + j*CAMERA_WIDTH]*mask[i + j*CAMERA_WIDTH];
        }

        int idx = CAMERA_WIDTH*j + i0 + r_f;

        for (int i = r_f + i0; i < i1 - r_f - 1; i++) {
            ibc1[idx] = isx;
            nc[idx] = n;
            isx = isx + mask[idx + r_f + 1]*ib[idx + r_f + 1] - 
                  mask[idx - r_f]*ib[idx - r_f];
            n = n + mask[idx + r_f + 1] - mask[idx - r_f];
            idx++;
        }

        ibc1[idx] = isx;
        nc[idx] = n;
    }

    for (int j = j0+b; j < j1-b; j++) {
        for (int i = i0+b; i < i1-b; i++) {
            n = s = 0;
            for (int jp =- r_f; jp <= r_f; jp++) {
                int idx = i + (j+jp)*CAMERA_WIDTH;
                s += ibc1[idx];
                n += nc[idx];
            }
            ds = s;
            dn = n;
            if (dn > 0.0) {
                ds /= dn;
                last_ds = ds;
            } else {
                ds = last_ds;
            }
            filtered_image[i + j*CAMERA_WIDTH] = ds;
        }
    }
}

/* Function to find the blobs in an image.
** Inputs: The original image prior to processing (input_biffer), the dimensions
** of the image (w & h) pointers to arrays for the x coordinates, y coordinates,
** and magnitudes (pixel values) of the blobs, and an array for the bytes of the
** image after processing (masking, filtering, et cetera).
** Output: the number of blobs detected in the image.
*/
int findBlobs(char * input_buffer, int w, int h, double ** star_x, 
              double ** star_y, double ** star_mags, char * output_buffer) { 
    static int first_time = 1;
    static double * ic = NULL, * ic2 = NULL;
    static int num_blobs_alloc = 0;

    // allocate the proper amount of storage space to start
    if (first_time) {
        ic = calloc(CAMERA_WIDTH*CAMERA_HEIGHT, sizeof(double));
        ic2 = calloc(CAMERA_WIDTH*CAMERA_HEIGHT, sizeof(double));
        first_time = 0;
    }
  
    // we use half-width internally, but the API gives us full width.
    int x_size = w/2;
    int y_size = h/2;

    int j0, j1, i0, i1;
    // extra image border
    int b = 0;

    j0 = i0 = 0;
    j1 = h;
    i1 = w;
    
    b = all_blob_params.centroid_search_border;

    // if we want to make a new hot pixel mask
    if (all_blob_params.make_static_hp_mask) {
        if (verbose) {
            printf("**************************** Making static hot pixel map..."
                   "****************************\n");
        }

        // make a file and write bright pixel coordinates to it
        FILE * f = fopen(join_path(config.bvexcam.configdir,"/static_hp_mask.txt"), "w"); 

        for (int yp = 0; yp < CAMERA_HEIGHT; yp++) {
            for (int xp = 0; xp < CAMERA_WIDTH; xp++) {
                // index in the array where we are
                int ind = CAMERA_WIDTH*yp + xp; 
                // check pixel value to see if it's above hot pixel threshold
                if (input_buffer[ind] > all_blob_params.make_static_hp_mask) {
                    int i = xp;
                    // make this agree with blob coordinates in Kst
                    int j = CAMERA_HEIGHT - yp; 
                    fprintf(f, "%d,%d\n", i, j);
                    if (verbose) {
                        printf("Value of pixel (should be > %i): %d | "
                               "Coordinates: [%d, %d]\n", 
                               all_blob_params.make_static_hp_mask, 
                               input_buffer[ind], i, j);
                    }
                }
            }
        }

        fflush(f);
        fclose(f);
    }

    makeMask(input_buffer, i0, j0, i1, j1, 0, 0, 0);

    double sx = 0, sx2 = 0;
    // sum of non-highpass filtered field
    double sx_raw = 0;                            
    int num_pix = 0;

    // lowpass filter the image - reduce noise.
    boxcarFilterImage(input_buffer, i0, j0, i1, j1, all_blob_params.r_smooth, 
                      ic);

    // only high-pass filter full frames
    if (all_blob_params.high_pass_filter) {       
        b += all_blob_params.r_high_pass_filter;

        boxcarFilterImage(input_buffer, i0, j0, i1, j1, 
                          all_blob_params.r_high_pass_filter, ic2);

        for (int j = j0+b; j < j1-b; j++) {
            for (int i = i0+b; i < i1-b; i++) {
                int idx = i + j*w;
                sx_raw += ic[idx]*mask[idx];
                ic[idx] -= ic2[idx];
                sx += ic[idx]*mask[idx];
                sx2 += ic[idx]*ic[idx]*mask[idx];
                num_pix += mask[idx];
            }
        }
    } else {
        for (int j = j0+b; j < j1-b; j++) {
            for (int i = i0+b; i < i1-b; i++) {
                int idx = i + j*w;
                sx += ic[idx]*mask[idx];
                sx2 += ic[idx]*ic[idx]*mask[idx];
                num_pix += mask[idx];
            }
        }
        sx_raw = sx;
    }

    double mean = sx/num_pix;
    double mean_raw = sx_raw/num_pix;
    double sigma = sqrt((sx2 - sx*sx/num_pix)/num_pix);
    if (verbose) {
        printf("\n+---------------------------------------------------------+\n");
        printf("|\t\tBlob-finding calculations\t\t  |\n");
        printf("|---------------------------------------------------------|\n");
        printf("|\tMean = %f\t\t\t\t\t  |\n", mean);
        printf("|\tSigma = %f\t\t\t\t  |\n", sigma);
        printf("|\tRaw mean = %f\t\t\t\t  |\n", mean_raw);
        printf("+---------------------------------------------------------+\n");
    }

    // fill output buffer if the variable is defined 
    if (output_buffer) {
        int pixel_offset = 0;

        if (all_blob_params.high_pass_filter) pixel_offset = 50;

        if (all_blob_params.filter_return_image) {
            if (verbose) {
                printf("\nFiltering returned image...\n");
            }

            for (int j = j0 + 1; j < j1 - 1; j++) {
                for (int i = i0 + 1; i < i1 - 1; i++) {
                  output_buffer[i + j*w] = ic[i + j*w]+pixel_offset;
                }
            }

            for (int j = 0; j < b; j++) {
                for (int i = i0; i < i1; i++) {
                  output_buffer[i + (j + j0)*w] = 
                  output_buffer[i + (j1 - j - 1)*w] = mean + pixel_offset;
                }
            }

            for (int j = j0; j < j1; j++) {
                for (int i = 0; i < b; i++) {
                  output_buffer[i + i0 + j*w] = 
                  output_buffer[i1 - i - 1 + j*w] = mean + pixel_offset;
                }
            }
        } else {
            if (verbose) {
                printf("\n> Not filtering the returned image...\n\n");
            }

            for (int j = j0; j < j1; j++) {
                for (int i = i0; i < i1; i++) {
                  int idx = i + j*w;
                  output_buffer[idx] = input_buffer[idx];
                }
            }
        }
    }

    // find the blobs 
    double ic0;
    int blob_count = 0;
    for (int j = j0 + b; j < j1-b-1; j++) {
        for (int i = i0 + b; i < i1-b-1; i++) {
            // if pixel exceeds threshold
            if ((double) ic[i + j*w] > mean + all_blob_params.n_sigma*sigma) {
                ic0 = ic[i + j*w];
                // if pixel is a local maximum or saturated
                if (((ic0 >= ic[i-1 + (j-1)*w]) &&
                     (ic0 >= ic[i   + (j-1)*w]) &&
                     (ic0 >= ic[i+1 + (j-1)*w]) &&
                     (ic0 >= ic[i-1 + (j  )*w]) &&
                     (ic0 >  ic[i+1 + (j  )*w]) &&
                     (ic0 >  ic[i-1 + (j+1)*w]) &&
                     (ic0 >  ic[i   + (j+1)*w]) &&
                     (ic0 >  ic[i+1 + (j+1)*w])) ||
                     (ic0 > 254)) {
                      
                    int unique = 1;

                    // realloc array if necessary (when the camera looks at a 
                    // really bright image, this slows everything down severely)
                    if (blob_count >= num_blobs_alloc) {
                        num_blobs_alloc += 500;
                        *star_x = realloc(*star_x, sizeof(double)*num_blobs_alloc);
                        *star_y = realloc(*star_y, sizeof(double)*num_blobs_alloc);
                        *star_mags = realloc(*star_mags, sizeof(double)*num_blobs_alloc);
                    }

                    (*star_x)[blob_count] = i;
                    (*star_y)[blob_count] = j;
                    (*star_mags)[blob_count] = 100*ic[i + j*CAMERA_WIDTH];

                    // FIXME: not sure why this is necessary..
                    if ((*star_mags)[blob_count] < 0) {
                        (*star_mags)[blob_count] = UINT32_MAX;
                    }

                    // if we already found a blob within SPACING and this one is
                    // bigger, replace it.
                    int spacing = all_blob_params.unique_star_spacing;
                    if ((*star_mags)[blob_count] > 25400) {
                        spacing = spacing * 4;
                    }
                    for (int ib = 0; ib < blob_count; ib++) {
                        if ((abs((*star_x)[blob_count]-(*star_x)[ib]) < spacing) &&
                            (abs((*star_y)[blob_count]-(*star_y)[ib]) < spacing)) {
                            unique = 0;
                            // keep the brighter one
                            if ((*star_mags)[blob_count] > (*star_mags)[ib]) {
                                (*star_x)[ib] = (*star_x)[blob_count];
                                (*star_y)[ib] = (*star_y)[blob_count];
                                (*star_mags)[ib] = (*star_mags)[blob_count];
                            }
                        }
                    }
                    // if we didn't find a close one, it is unique.
                    if (unique) {
                        blob_count++;
                    }
                }
            }
        }
    }
    // this loop flips vertical position of blobs back to their normal location
    for (int ibb = 0; ibb < blob_count; ibb++) {
        (*star_y)[ibb] = CAMERA_HEIGHT - (*star_y)[ibb];
    }

    // merge sort
    part(*star_mags, 0, blob_count - 1, *star_x, *star_y); 
    if (verbose) {
        printf("(*) Number of blobs found in image: %i\n\n", blob_count);
    }

    return blob_count;
}

/* Function for sorting implementation.
** Input: 
** Output:
*/
void merge(double * A, int p, int q, int r, double * X, double * Y){
    int n1 = q - p + 1, n2 = r - q;
    int lin = n1 + 1, rin = n2 + 1;
    double LM[lin], LX[lin], LY[lin], RM[rin], RX[rin], RY[rin];
    int i, j, k;
    LM[n1] = 0;
    RM[n2] = 0;

    for (i = 0; i < n1; i++) {
        LM[i] = A[p+i];
        LX[i] = X[p+i];
        LY[i] = Y[p+i];
    }

    for (j = 0; j < n2; j++) {
        RM[j] = A[q+j+1];
        RX[j] = X[q+j+1];
        RY[j] = Y[q+j+1];
    }

    i = 0; j = 0;
    for (k = p; k <= r; k++) {
        if (LM[i] >= RM[j] ){
            A[k] = LM[i];
            X[k] = LX[i];
            Y[k] = LY[i];
            i++;
        } else {
            A[k] = RM[j];
            X[k] = RX[j];
            Y[k] = RY[j];
            j++;
        }
    }
}

/* Recursive part of sorting.
** Input:
** Output:
*/
void part(double * A, int p, int r, double * X, double * Y) {
    if (p < r) {
        int q = (p + r)/2;
        part(A, p, q, X, Y);
        part(A, q + 1, r, X, Y);
        merge(A, p, q, r, X, Y);
    }
}

/* Function to load saved images (likely from previous observing sessions, but 
** could also be test pictures).
** Inputs: the name of the image to be loaded (filename) and the active image 
** memory (buffer).
** Outputs: A flag indicating successful loading of the image or not.
*/
int loadDummyPicture(wchar_t * filename, char ** buffer) {
    ImageFileParams.ppcImageMem = buffer;
    ImageFileParams.pwchFileName = filename;
    ImageFileParams.ppcImageMem = NULL;
    if (is_ImageFile(camera_handle, IS_IMAGE_FILE_CMD_LOAD, 
                     (void *) &ImageFileParams, sizeof(ImageFileParams)) 
                     != IS_SUCCESS) {
        return -1;
    } else {
        return 1;
    }
    ImageFileParams.ppcImageMem = NULL;
}

/* Function to make table of stars from image for displaying in Kst (mostly for 
** testing).
** Inputs: The name of blob table file, array of blob magnitudes, array of blob 
** x coordinates, array of blob y coordinates, and number of blobs.
** Output: A flag indicating successful writing of the blob table file.
*/
int makeTable(char * filename, double * star_mags, double * star_x, 
              double * star_y, int blob_count) {
    FILE * fp;

    if ((fp = fopen(filename, "w")) == NULL) {
    	fprintf(stderr, "Could not open blob-writing table file: %s.\n", 
                strerror(errno));
    	return -1;
    }

    for (int i = 0; i < blob_count; i++) {
        fprintf(fp, "%f,%f,%f\n", star_mags[i], star_x[i], star_y[i]);
    }

    // close both files and return
    fflush(fp);
    fclose(fp);
    return 1;
}

/* Function to take observing images and solve for pointing using Astrometry.
** Main function for the Astrometry thread in commands.c.
** Input: None.
** Output: A flag indicating successful round of image + solution by the camera 
** (e.g. if the camera can't open the observing file, the function will 
** automatically return with -1).
*/
int doCameraAndAstrometry(FILE* log) {
    // these must be static since this function is called perpetually in 
    // updateAstrometry thread
    static double * star_x = NULL, * star_y = NULL, * star_mags = NULL;
    static char * output_buffer = NULL;
    static int first_time = 1, af_photo = 0;
    static FILE * af_file = NULL;
    static FILE * fptr = NULL;
    static int num_focus_pos;
    static int * blob_mags;
    int blob_count;
    char datafile[100], buff[100], date[256];
    static char af_filename[256];
    wchar_t filename[200] = L"";
    struct timespec camera_tp_beginning, camera_tp_end; 
    time_t seconds = time(NULL);
    struct tm * tm_info;

    // uncomment line below for testing the values of each field in the global 
    // structure for blob_params
    if (verbose) {
        verifyBlobParams();
    }

    tm_info = gmtime(&seconds);
    all_astro_params.rawtime = seconds;
    // if it is a leap year, adjust tm_info accordingly before it is passed to 
    // calculations in lostInSpace
    if (isLeapYear(tm_info->tm_year)) {
        // if we are on Feb 29
        if (tm_info->tm_yday == 59) {  
            // 366 days in a leap year      
            tm_info->tm_yday++;  
            // we are still in February 59 days after January 1st (Feb 29)            
            tm_info->tm_mon -= 1;
            tm_info->tm_mday = 29;
        } else if (tm_info->tm_yday > 59) {
            tm_info->tm_yday++;           
        }
    }

    // data file to pass to lostInSpace
    strftime(datafile, sizeof(datafile), 
             join_path(config.bvexcam.workdir,"/data_%b-%d.txt"), tm_info);
    
    // set file descriptor for observing file to NULL in case of previous bad
    // shutdown or termination of Astrometry
    if (fptr != NULL) {
        fclose(fptr);
        fptr = NULL;
    }
    
    if ((fptr = fopen(datafile, "a")) == NULL) {
        fprintf(log, "[%ld][camera.c][doCameraAndAstrometry] Could not open obs. file: %s.\n",time(NULL),strerror(errno));
        return -1;
    }

    if (first_time) {
        output_buffer = calloc(1, CAMERA_WIDTH*CAMERA_HEIGHT);
        if (output_buffer == NULL) {
            fprintf(log, "[%ld][camera.c][doCameraAndAstrometry] Error allocating output buffer: %s.\n", time(NULL), 
                    strerror(errno));
            return -1;
        }

        blob_mags = calloc(default_focus_photos, sizeof(int));
        if (blob_mags == NULL) {
            fprintf(log, "[%ld][camera.c][doCameraAndAstrometry.c] Error allocating array for blob mags: %s.\n", time(NULL), 
                    strerror(errno));
            return -1;
        }

        // get frame rate again
        is_SetFrameRate(camera_handle, IS_GET_FRAMERATE, (void *) &actual_fps);

        // write observing information to data file
        strftime(buff, sizeof(buff), "%B %d Observing Session - beginning "
                                     "%H:%M:%S GMT", tm_info); 
        fprintf(fptr, "********************* %s *********************\n", buff);
        fprintf(fptr, "Camera model: %s\n", sensorInfo.strSensorName);
        fprintf(fptr, "----------------------------------------------------\n");
        fprintf(fptr, "Exposure: %f milliseconds\n", curr_exposure);
        fprintf(fptr, "Pixel clock: %i\n", curr_pc);
        fprintf(fptr, "Frame rate achieved (desired is 10): %f\n", actual_fps);
        fprintf(fptr, "Trigger delay (microseconds): %i\n", curr_trig_delay);
        fprintf(fptr, "Current trigger mode setting: %i\n", curr_ext_trig);
        fprintf(fptr, "Current trigger timeout: %i\n", curr_timeout);
        fprintf(fptr, "Auto shutter: %.1f\n", curr_shutter);
        fprintf(fptr, "Auto frame rate (should be disabled): %.1f\n", auto_fr);
        fprintf(fptr, "----------------------------------------------------\n");
        fprintf(fptr, "Sensor ID/type: %u\n", sensorInfo.SensorID);
        fprintf(fptr, "Sensor color mode (from is_GetSensorInfo and "
                      "is_SetColorMode): %i | %i\n", 
                sensorInfo.nColorMode, curr_color_mode);
        fprintf(fptr, "Maximum image width and height: %i, %i\n", 
                       sensorInfo.nMaxWidth, sensorInfo.nMaxHeight);
        fprintf(fptr, "Pixel size (micrometers): %.2f\n", 
                      ((double) sensorInfo.wPixelSize)/100.0);
        fprintf(fptr, "Gain settings: %i for master gain, %i for red gain, %i "
                      "for green gain, and %i for blue gain.\n", 
                      curr_master_gain, curr_red_gain, curr_green_gain, 
                      curr_blue_gain);
        fprintf(fptr, "Auto gain (should be disabled): %i\n", (int) curr_ag);
        fprintf(fptr, "Gain boost (should be disabled): %i\n", curr_gain_boost);
        fprintf(fptr, "Hardware gamma (should be disabled): %i\n", curr_gamma);
        fprintf(fptr, "----------------------------------------------------\n");
        fprintf(fptr, "Auto black level (should be off): %i\n", bl_mode);
        fprintf(fptr, "Black level offset (desired is 50): %i\n", bl_offset);

        // write header to data file
        if (fprintf(fptr, "\nC time|GMT|Blob #|RA (deg)|DEC (deg)|FR (deg)|PS|"
                          "ALT (deg)|AZ (deg)|IR (deg)|Astrom. solve time "
                          "(msec)|Camera time (msec)") < 0) {
            fprintf(log, "[%ld][camera.c][doCameraAndAstrometry]Error writing header to observing file: %s.\n", time(NULL),
                    strerror(errno));
        }

        fflush(fptr);
        first_time = 0;
    }

    // if we are at the start of auto-focusing (either when camera first runs or 
    // user re-enters auto-focusing mode)
    if (all_camera_params.begin_auto_focus && all_camera_params.focus_mode) {
        num_focus_pos = 0;
        send_data = 0;

        // check that our blob magnitude array is big enough for number of
        // photos we take per auto-focusing position
        if (all_camera_params.photos_per_focus != default_focus_photos) {
            write_to_log(log,"camera.c","doCameraAndAstrometry.c","Reallocating blob_mags array to allow for different # of "
                   "auto-focusing pictures.");
            default_focus_photos = all_camera_params.photos_per_focus;
            blob_mags = realloc(blob_mags, sizeof(int) * default_focus_photos);
        }

        // check that end focus position is at least 25 less than max focus
        // position
        if (all_camera_params.max_focus_pos - all_camera_params.end_focus_pos 
            < 25) {
            write_to_log(log,"camera.c","doCameraAndAstrometry","Adjusting end focus position to be 25 less than max focus "
                   "position.");
            all_camera_params.end_focus_pos = all_camera_params.max_focus_pos 
                                              - 25;
        }

        // check that beginning focus position is at least 25 above min focus
        // position
        if (all_camera_params.start_focus_pos - all_camera_params.min_focus_pos
            < 25) {
            write_to_log(log,"camera.c","doCameraAndAstrometry","Adjusting beginning focus position to be 25 more than min "
                   "focus position.");
            all_camera_params.start_focus_pos = all_camera_params.min_focus_pos
                                                + 25;
        }

        // get to beginning of auto-focusing range
        if (beginAutoFocus(log) < 1) {
            write_to_log(log,"camera.c","doCameraAndAstrometry","Error beginning auto-focusing process. Skipping to taking "
                   "observing images...");

            // return to default focus position
            if (defaultFocusPosition(log) < 1) {
                write_to_log(log,"camera.c","doCameraAndAstrometry","Error moving to default focus position.");
                closeCamera();
                return -1;
            }

            // abort auto-focusing process
            all_camera_params.focus_mode = 0;
        }
        
        usleep(1000000); 

        if (af_file != NULL) {
            fclose(af_file);
            af_file = NULL;
        }

        // clear previous contents of auto-focusing file (open in write mode)
        strftime(af_filename, sizeof(af_filename), join_path(config.bvexcam.workdir,"/focus_data/auto_focus_starting_%Y-%m-%d_%H:%M:%S.txt"), tm_info);
        if (verbose) {
            printf("Opening auto-focusing text file: %s\n", af_filename);
        }

        if ((af_file = fopen(af_filename, "w")) == NULL) {
    	    fprintf(log, "[%ld][camera.c][doCameraAndAstrometry] Could not open auto-focusing file %s: %s.\n", time(NULL), af_filename, 
                    strerror(errno));
    	    return -1;
    	}

        all_camera_params.begin_auto_focus = 0;

        // turn dynamic hot pixels off to avoid removing blobs during focusing
        prev_dynamic_hp = all_blob_params.dynamic_hot_pixels;
        if (verbose) {
            printf("Turning dynamic hot pixel finder off for auto-focusing.\n");
        }
        all_blob_params.dynamic_hot_pixels = 0;
        
        // link the auto-focusing txt file to Kst for plotting
        unlink(join_path(config.bvexcam.workdir,"/latest_auto_focus_data.txt"));
        symlink(af_filename, join_path(config.bvexcam.workdir,"/latest_auto_focus_data.txt"));
    }

    //take an image
    if (verbose) {
        printf("\n> Taking a new image...\n\n");
    }

    taking_image = 1;
    if (is_FreezeVideo(camera_handle, IS_WAIT) != IS_SUCCESS) {
       const char * last_error_str = printCameraError();
       fprintf(log, "[%ld][camera.c][doCameraAndAstrometry] Failed to capture new image: %s\n", time(NULL), last_error_str);
    } 
    taking_image = 0;

    // get the image from memory
    if (is_GetActSeqBuf(camera_handle, &buffer_num, &waiting_mem, &memory) 
        != IS_SUCCESS) {
        cam_error = printCameraError();
        fprintf(log,"[%ld][camera.c][doCameraAndAstrometry] Error retrieving the active image memory: %s.\n", time(NULL), cam_error);
    }

    // testing pictures that have already been taken 
    /*
    if (loadDummyPicture(L"/home/felix/blastcam/pics/test_2.jpg", 
                         &memory) == 1) {
        if (verbose) {
            printf("Successfully loaded test picture.\n");
        }
    } else {
        fprintf(stderr, "Error loading test picture: %s.\n", strerror(errno));
        // can't solve without a picture to solve on!
        usleep(1000000);
        return -1;
    }
*/
    // find the blobs in the image
    blob_count = findBlobs(memory, CAMERA_WIDTH, CAMERA_HEIGHT, &star_x, 
                           &star_y, &star_mags, output_buffer);

    // make kst display the filtered image 
    memcpy(memory, output_buffer, CAMERA_WIDTH*CAMERA_HEIGHT); 

    // pointer for transmitting to user should point to where image is in memory
    camera_raw = output_buffer;

    // get current time right after exposure
    if (clock_gettime(CLOCK_REALTIME, &camera_tp_beginning) == -1) {
        fprintf(log, "[%ld][camera.c][doCameraAndAstrometry] Error starting camera timer: %s.\n", time(NULL), strerror(errno));
    }

    /*
    if ((seconds-prev_focus)>=config.bvexcam.ttf){
        all_camera_params.focus_mode = 1;
        prev_focus = seconds;
    }
    */

    // now have to distinguish between auto-focusing actions and solving
    if (all_camera_params.focus_mode && !all_camera_params.begin_auto_focus) {
        int brightest_blob, max_flux, focus_step;
        int brightest_blob_x, brightest_blob_y;
        char focus_str_cmd[10];
        char time_str[100];

        if (verbose) {
            printf("\n>> Still auto-focusing!\n");
        }

        // find the brightest blob per picture
        brightest_blob = -1;
        for (int blob = 0; blob < blob_count; blob++) {
            if (star_mags[blob] > brightest_blob) {
                brightest_blob = star_mags[blob];
                brightest_blob_x = (int) star_x[blob];
                brightest_blob_y = (int) star_y[blob];
            }
        }
        blob_mags[af_photo++] = brightest_blob;
        fprintf(log,"[%ld][camera.c][doCameraAndAstrometry] Brightest blob for photo %d at focus %d has value %d.\n", time(NULL), 
               af_photo, all_camera_params.focus_position, brightest_blob);

        strftime(time_str, sizeof(time_str), "%Y-%m-%d_%H:%M:%S", tm_info);
        sprintf(date, join_path(config.bvexcam.workdir,"/focus_data/auto_focus_at_%d_"
                      "brightest_blob_%d_at_x%d_y%d_%s.bmp"), 
                all_camera_params.focus_position, brightest_blob, 
                brightest_blob_x, brightest_blob_y, time_str);
        if (verbose) {
            printf("Saving auto-focusing image as: %s\n", date);
        }
        swprintf(filename, 200, L"%s", date);

        if (af_photo >= all_camera_params.photos_per_focus) {
            if (verbose) {
                printf("> Processing auto-focus images for focus %d -> do not "
                       "take an image...\n", all_camera_params.focus_position);
            }

            // find brightest of three brightest blobs for this batch of images
            max_flux = -1;
            for (int i = 0; i < all_camera_params.photos_per_focus; i++) {
                if (blob_mags[i] > max_flux) {
                    max_flux = blob_mags[i];
                }
            }
           
            all_camera_params.flux = max_flux;
            fprintf(log, "[%ld][camera.c][doCameraAndAstrometry] Brighest blob among %d photos for focus %d is %d.\n", time(NULL), 
                   all_camera_params.photos_per_focus, 
                   all_camera_params.focus_position,
                   max_flux);

            fprintf(af_file, "%3d\t%5d\n", max_flux,
                    all_camera_params.focus_position);
            fflush(af_file);

            send_data = 1;

            // if clients are listening and we want to guarantee data is sent to
            // them before continuing with auto-focusing, wait until data_sent
            // confirmation. If there are no clients, no need to slow down auto-
            // focusing
            //if (num_clients > 0) {
            //    while (!telemetry_sent) {
            //        if (verbose) {
            //            printf("> Waiting for data to send to client...\n");
            //        }

            //        usleep(100000);
            //    }
            //}

            send_data = 0;

            // since we are moving to next focus, re-start photo counter and get
            // rid of previous blob magnitudes
            af_photo = 0;
            for (int i = 0; i < all_camera_params.photos_per_focus; i++) {
                blob_mags[i] = 0;
            }

            // We have moved to the end (or past) the end focus position, so
            // calculate best focus and exit
            if (all_camera_params.focus_position >= 
                all_camera_params.end_focus_pos) {
                int best_focus; 
                // at very last focus position
                num_focus_pos++;

                best_focus = calculateOptimalFocus(log,num_focus_pos, af_filename);
                if (best_focus == -1000) {
                    // if we can't find optimal focus from auto-focusing data, 
                    // just go to the default
                    defaultFocusPosition(log);
                } else {
                    // if the calculated auto focus position is outside the
                    // possible range, set it to corresponding nearest focus
                    if (best_focus > all_camera_params.max_focus_pos) {
                        write_to_log(log, "camera.c","doCameraAndAstrometry","Auto focus is greater than max possible focus, "
                               "so just use that.");
                        best_focus = all_camera_params.max_focus_pos;
                    } else if (best_focus < all_camera_params.min_focus_pos) {
                        write_to_log(log, "camera.c","doCameraAndAstrometry","Auto focus is less than min possible focus, "
                               "so just use that.");
                        // this outcome is highly unlikely but just in case
                        best_focus = all_camera_params.min_focus_pos;
                    }

                    sprintf(focus_str_cmd, "mf %i\r", 
                            best_focus - all_camera_params.focus_position);
                    shiftFocus(log,focus_str_cmd);
                }

                fclose(af_file);
                af_file = NULL;

                // turn dynamic hot pixels back to whatever user had specified
                if (verbose) {
                    printf("> Auto-focusing finished, so restoring dynamic hot "
                           "pixels to previous value...\n");
                }

                all_blob_params.dynamic_hot_pixels = prev_dynamic_hp;
                if (verbose) {
                    printf("Now all_blob_params.dynamic_hot_pixels = %d\n", 
                           all_blob_params.dynamic_hot_pixels);
                }
		all_camera_params.focus_mode = 0;
            } else {
                // Move to the next focus position if we still have positions to
                // cover
                focus_step = min(all_camera_params.focus_step, 
                             all_camera_params.end_focus_pos - 
                             all_camera_params.focus_position);
                sprintf(focus_str_cmd, "mf %i\r", focus_step);
                if (!cancelling_auto_focus) {
                    shiftFocus(log,focus_str_cmd);
                    usleep(100000);
                }
                num_focus_pos++;
            }
        }
    } else {
        double start, end, camera_time;
        send_data = 1;

        if (verbose) {
            printf(">> No longer auto-focusing!\n");
        }

        strftime(date, sizeof(date), join_path(config.bvexcam.workdir,"/pics/saved_image_%Y-%m-%d_%H:%M:%S.bmp"), tm_info);
        swprintf(filename, 200, L"%s", date);

        // write blob and time information to data file
        strftime(buff, sizeof(buff), "%b %d %H:%M:%S", tm_info); 
        fprintf(log,"[%ld][camera.c][doCameraAndAstrometry] Time going into Astrometry.net: %s\n", time(NULL), buff);

        if (fprintf(fptr, "\r%li|%s|", seconds, buff) < 0) {
            fprintf(log, "[%ld][camera.c][doCameraAndAstrometry] Unable to write time and blob count to observing "
                            "file: %s.\n", time(NULL), strerror(errno));
        }
        fflush(fptr);

        // solve astrometry
        if (verbose) {
            printf("\n> Trying to solve astrometry...\n");
        }

        if (lostInSpace(log,star_x, star_y, star_mags, blob_count, tm_info, 
                        datafile) != 1) {
            write_to_log(log,"camera.c","doCameraAndAstrometry","Could not solve Astrometry.");
        }

        // get current time right after solving
        if (clock_gettime(CLOCK_REALTIME, &camera_tp_end) == -1) {
            fprintf(log, "[%ld][camera.c][doCameraAndAstrometry] Error ending timer: %s.\n", time(NULL), strerror(errno));
        }

        // calculate time it took camera program to run in nanoseconds
	    start = (double) (camera_tp_beginning.tv_sec*1e9) + 
                (double) camera_tp_beginning.tv_nsec;
	    end = (double) (camera_tp_end.tv_sec*1e9) + 
              (double) camera_tp_end.tv_nsec;
        camera_time = end - start;
	    fprintf(log,"[%ld][camera.c][doCamreaAndAstrometry] Camera completed one round in %f msec.\n", time(NULL), 
               camera_time*1e-6);

        // write this time to the data file
        if (fprintf(fptr, "|%f", camera_time*1e-6) < 0) {
            fprintf(log, "[%ld][camera.c][doCameraAndAstrometry] Unable to write Astrometry solution time to "
                            "observing file: %s.\n", time(NULL), strerror(errno));
        }
        fflush(fptr);
        fclose(fptr);
        fptr = NULL;
    }

    // save image for future reference
    ImageFileParams.pwchFileName = filename;
    if (is_ImageFile(camera_handle, IS_IMAGE_FILE_CMD_SAVE, 
                    (void *) &ImageFileParams, sizeof(ImageFileParams)) == -1) {
        const char * last_error_str = printCameraError();
        fprintf(log,"[%ld][camera.c][doCameraAndAstrometry] Failed to save image: %s\n", time(NULL), last_error_str);
    }

    wprintf(L"Saving to \"%s\"\n", filename);
    // unlink whatever the latest saved image was linked to before
    unlink(join_path(config.bvexcam.workdir,"/latest_saved_image.bmp"));
    // sym link current date to latest image for live Kst updates
    symlink(date, join_path(config.bvexcam.workdir,"/latest_saved_image.bmp"));

    // make a table of blobs for Kst
    if (makeTable("makeTable.txt", star_mags, star_x, star_y, blob_count) != 1) {
        write_to_log(log,"camera,.c","doCameraAndAstrometry","Error (above) writing blob table for Kst.\n");
    }

    // free alloc'd variables when we are shutting down
    if (shutting_down) {
        if (verbose) {
            printf("\n> Freeing allocated variables in camera.c...\n");
        }

        if (output_buffer != NULL) {
            free(output_buffer);
        }
        
        if (mask != NULL) {
            free(mask);
        }
        
        if (blob_mags != NULL) {
            free(blob_mags);
        }

        if (star_x != NULL) {
            free(star_x);
        }
        
        if (star_y != NULL) {
            free(star_y);
        }
        
        if (star_mags != NULL) {
            free(star_mags);
        }
    }
    return 1;
}
