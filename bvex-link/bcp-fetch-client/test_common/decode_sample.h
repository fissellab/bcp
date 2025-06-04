#include <../src/generated/nanopb/sample.pb.h>
#include <cstdint>
#include <nanopb/pb_decode.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

Sample* decode_sample(const uint8_t* data, size_t size)
{
    Sample* sample = (Sample*)malloc(sizeof(Sample));
    if(!sample) {
        return NULL;
    }
    *sample = Sample_init_zero;

    /* Create a stream that reads from the buffer. */
    pb_istream_t stream = pb_istream_from_buffer(data, size);

    /* Now we are ready to decode the message. */
    bool success = pb_decode(&stream, Sample_fields, sample);

    /* Check for errors... */
    if(success) {
        return sample;
    } else {
        free(sample);
        return NULL;
    }
}

#ifdef __cplusplus
}
#endif
