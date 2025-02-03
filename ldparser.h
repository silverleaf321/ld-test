#ifndef LDPARSER_H
#define LDPARSER_H

#include <stdio.h>
#include <stdint.h>
#include <time.h>

// Forward declarations
struct ldVehicle;
struct ldVenue;
struct ldEvent;
struct ldHead;
struct ldChan;
struct ldData;

// Structures matching Python classes
typedef struct ldVehicle {
    char id[64];
    uint32_t weight;
    char type[32];
    char comment[32];
} ldVehicle;

typedef struct ldVenue {
    char name[64];
    uint16_t vehicle_ptr;
    ldVehicle* vehicle;
} ldVenue;

typedef struct ldEvent {
    char name[64];
    char session[64];
    char comment[1024];
    uint16_t venue_ptr;
    ldVenue* venue;
} ldEvent;

typedef struct ldHead {
    uint32_t meta_ptr;
    uint32_t data_ptr;
    uint32_t event_ptr;
    ldEvent* event;
    char driver[64];
    char vehicleid[64];
    char venue[64];
    struct tm datetime;
    char short_comment[64];
} ldHead;

typedef struct ldChan {
    char* file_path;
    uint32_t meta_ptr;
    float* data;
    
    uint32_t prev_meta_ptr;
    uint32_t next_meta_ptr;
    uint32_t data_ptr;
    uint32_t data_len;
    
    uint16_t dtype;
    uint16_t freq;
    int16_t shift;
    int16_t mul;
    int16_t scale;
    int16_t dec;
    
    char name[32];
    char short_name[8];
    char unit[12];
} ldChan;

typedef struct ldData {
    ldHead* head;
    ldChan** channs;
    size_t chann_count;
} ldData;

// Function declarations
ldData* read_ldfile(const char* filename);
void free_lddata(ldData* data);

ldVehicle* read_vehicle(FILE* f);
ldVenue* read_venue(FILE* f);
ldEvent* read_event(FILE* f);
ldHead* read_head(FILE* f);
ldChan* read_channel(const char* filename, uint32_t meta_ptr);
ldChan** read_channels(const char* filename, uint32_t meta_ptr, size_t* count);

void write_ldfile(const char* filename, ldData* data);

// Helper functions
char* decode_string(const char* bytes, size_t len);
float* read_channel_data(FILE* f, ldChan* chan);

#endif // LDPARSER_H