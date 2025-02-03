#include "ldparser.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Helper function to decode strings
char* decode_string(const char* bytes, size_t len) {
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    memcpy(result, bytes, len);
    result[len] = '\0';
    
    // Trim trailing nulls and whitespace
    size_t end = len;
    while (end > 0 && (result[end-1] == '\0' || result[end-1] == ' '))
        end--;
    
    result[end] = '\0';
    return result;
}

// Read vehicle information
ldVehicle* read_vehicle(FILE* f) {
    ldVehicle* vehicle = malloc(sizeof(ldVehicle));
    if (!vehicle) return NULL;
    
    char id[64];
    uint32_t weight;
    char type[32];
    char comment[32];
    
    if (fread(id, 64, 1, f) != 1 ||
        fread(&weight, 4, 1, f) != 1 ||
        fread(type, 32, 1, f) != 1 ||
        fread(comment, 32, 1, f) != 1) {
        free(vehicle);
        return NULL;
    }
    
    memcpy(vehicle->id, id, 64);
    vehicle->weight = weight;
    memcpy(vehicle->type, type, 32);
    memcpy(vehicle->comment, comment, 32);
    
    return vehicle;
}

// Read venue information
ldVenue* read_venue(FILE* f) {
    ldVenue* venue = malloc(sizeof(ldVenue));
    if (!venue) return NULL;
    
    char name[64];
    uint16_t vehicle_ptr;
    
    if (fread(name, 64, 1, f) != 1 ||
        fseek(f, 1034, SEEK_CUR) != 0 ||  // Skip 1034 bytes
        fread(&vehicle_ptr, 2, 1, f) != 1) {
        free(venue);
        return NULL;
    }
    
    memcpy(venue->name, name, 64);
    venue->vehicle_ptr = vehicle_ptr;
    
    if (vehicle_ptr > 0) {
        long pos = ftell(f);
        fseek(f, vehicle_ptr, SEEK_SET);
        venue->vehicle = read_vehicle(f);
        fseek(f, pos, SEEK_SET);
    } else {
        venue->vehicle = NULL;
    }
    
    return venue;
}

// Read event information
ldEvent* read_event(FILE* f) {
    ldEvent* event = malloc(sizeof(ldEvent));
    if (!event) return NULL;
    
    char name[64];
    char session[64];
    char comment[1024];
    uint16_t venue_ptr;
    
    if (fread(name, 64, 1, f) != 1 ||
        fread(session, 64, 1, f) != 1 ||
        fread(comment, 1024, 1, f) != 1 ||
        fread(&venue_ptr, 2, 1, f) != 1) {
        free(event);
        return NULL;
    }
    
    memcpy(event->name, name, 64);
    memcpy(event->session, session, 64);
    memcpy(event->comment, comment, 1024);
    event->venue_ptr = venue_ptr;
    
    if (venue_ptr > 0) {
        long pos = ftell(f);
        fseek(f, venue_ptr, SEEK_SET);
        event->venue = read_venue(f);
        fseek(f, pos, SEEK_SET);
    } else {
        event->venue = NULL;
    }
    
    return event;
}

// Main function to read an LD file
ldData* read_ldfile(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    
    ldData* data = malloc(sizeof(ldData));
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    // Read header
    data->head = read_head(f);
    if (!data->head) {
        free(data);
        fclose(f);
        return NULL;
    }
    
    // Read channels
    data->channs = read_channels(filename, data->head->meta_ptr, &data->chann_count);
    if (!data->channs) {
        free(data->head);
        free(data);
        fclose(f);
        return NULL;
    }
    
    fclose(f);
    return data;
}

// Free allocated memory
void free_lddata(ldData* data) {
    if (!data) return;
    
    if (data->head) {
        if (data->head->event) {
            if (data->head->event->venue) {
                if (data->head->event->venue->vehicle) {
                    free(data->head->event->venue->vehicle);
                }
                free(data->head->event->venue);
            }
            free(data->head->event);
        }
        free(data->head);
    }
    
    if (data->channs) {
        for (size_t i = 0; i < data->chann_count; i++) {
            if (data->channs[i]) {
                free(data->channs[i]->data);
                free(data->channs[i]->file_path);
                free(data->channs[i]);
            }
        }
        free(data->channs);
    }
    
    free(data);
}